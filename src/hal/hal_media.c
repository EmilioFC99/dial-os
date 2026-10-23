/**
 * @file hal_media.c
 * @brief Real Audio Playback Engine, Library Scanner & Cover Art Decoder
 *
 * Scans a real music folder (e.g. /Users/emilio.flores/Music/Au5), decodes
 * cover.jpg album art into 180x180 RGB565 textures via ImageIO/CoreGraphics,
 * and streams real FLAC / MP3 / WAV / M4A audio to the macOS audio output
 * via AudioToolbox (ExtAudioFile + AudioQueue) with zero heap churn in the
 * 60 FPS UI render loop.
 */
#include "dial_media.h"
#include "dial_hal.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
#include <AudioToolbox/AudioToolbox.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <pthread.h>
#endif

/* ── Static Library & File Browser Storage (Within 24.0 MB Budget) ── */
static bool              g_media_loaded = false;
static char              g_root_path[512];
static dial_album_info_t g_albums[DIAL_MAX_ALBUMS];
static size_t            g_album_count = 0;
static size_t            g_total_tracks = 0;

/* Cover texture LRU cache (12 slots of 180x180 RGB565 = 777.6 KB static) */
#define DIAL_COVER_CACHE_SLOTS 12
typedef struct {
    size_t   album_idx;
    bool     valid;
    uint16_t pixels[DIAL_VINYL_TEX_DIM * DIAL_VINYL_TEX_DIM];
} cover_cache_slot_t;

static cover_cache_slot_t g_cover_cache[DIAL_COVER_CACHE_SLOTS];
static size_t             g_cover_next_slot = 0;

/* File Browser State */
static char            g_cur_dir_path[512];
static char            g_cur_dir_label[128];
static dial_fs_entry_t g_fs_entries[DIAL_MAX_DIR_ENTRIES];
static size_t          g_fs_entry_count = 0;

/* Active Now-Playing State */
static size_t   g_now_album_idx = 0;
static size_t   g_now_track_idx = 0;
static char     g_now_title[96]       = "Teardrop";
static char     g_now_artist[64]      = "Massive Attack";
static char     g_now_album[96]       = "Mezzanine · 1998";
static char     g_now_filepath[512]   = "/mnt/sdcard/Music/Mezzanine/03.flac";
static uint32_t g_now_sample_rate     = 96000;
static uint32_t g_now_bit_depth       = 24;
static uint32_t g_now_bitrate_kbps    = 2842;
static uint32_t g_now_duration_sec    = 330;
static bool     g_portable_playing    = false;
static float    g_portable_progress   = 0.41f;

/* ── macOS AudioToolbox Real Streaming State ── */
#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
#define AUDIO_BUF_COUNT  3
#define AUDIO_BUF_FRAMES 4096

static pthread_mutex_t     g_audio_mtx = PTHREAD_MUTEX_INITIALIZER;
static ExtAudioFileRef     g_ext_file  = NULL;
static AudioQueueRef       g_aq        = NULL;
static AudioQueueBufferRef g_aq_bufs[AUDIO_BUF_COUNT];
static bool                g_aq_running = false;
static bool                g_aq_paused  = false;
static int64_t             g_total_frames = 0;
static int64_t             g_base_frame   = 0;
static double              g_out_rate     = 48000.0;
static float               g_gain_01      = 0.82f;

static void aq_output_cb(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    (void)inUserData;
    pthread_mutex_lock(&g_audio_mtx);
    if (!g_ext_file || !g_aq_running) {
        memset(inBuffer->mAudioData, 0, inBuffer->mAudioDataBytesCapacity);
        inBuffer->mAudioDataByteSize = inBuffer->mAudioDataBytesCapacity;
        pthread_mutex_unlock(&g_audio_mtx);
        return;
    }

    UInt32 frames = AUDIO_BUF_FRAMES;
    AudioBufferList abl;
    abl.mNumberBuffers              = 1;
    abl.mBuffers[0].mNumberChannels = 2;
    abl.mBuffers[0].mDataByteSize   = inBuffer->mAudioDataBytesCapacity;
    abl.mBuffers[0].mData           = inBuffer->mAudioData;

    OSStatus err = ExtAudioFileRead(g_ext_file, &frames, &abl);
    pthread_mutex_unlock(&g_audio_mtx);

    if (err == noErr && frames > 0) {
        inBuffer->mAudioDataByteSize = frames * 4u;
        AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    } else {
        /* End of track reached -> loop cleanly to start of track */
        pthread_mutex_lock(&g_audio_mtx);
        if (g_ext_file) {
            ExtAudioFileSeek(g_ext_file, 0);
            g_base_frame = 0;
        }
        pthread_mutex_unlock(&g_audio_mtx);
        memset(inBuffer->mAudioData, 0, 1024);
        inBuffer->mAudioDataByteSize = 1024;
        AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    }
}

static void stop_real_audio_locked(void) {
    g_aq_running = false;
    if (g_aq) {
        AudioQueueRef aq = g_aq;
        g_aq = NULL;
        pthread_mutex_unlock(&g_audio_mtx);
        AudioQueueStop(aq, true);
        AudioQueueDispose(aq, true);
        pthread_mutex_lock(&g_audio_mtx);
    }
    if (g_ext_file) {
        ExtAudioFileDispose(g_ext_file);
        g_ext_file = NULL;
    }
}

static bool start_real_audio_file(const char *filepath, uint32_t file_size_bytes) {
    if (!filepath || !*filepath) return false;

    pthread_mutex_lock(&g_audio_mtx);
    stop_real_audio_locked();

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, (const UInt8 *)filepath, (CFIndex)strlen(filepath), false);
    if (!url) {
        pthread_mutex_unlock(&g_audio_mtx);
        return false;
    }

    OSStatus err = ExtAudioFileOpenURL(url, &g_ext_file);
    CFRelease(url);
    if (err != noErr || !g_ext_file) {
        g_ext_file = NULL;
        pthread_mutex_unlock(&g_audio_mtx);
        return false;
    }

    AudioStreamBasicDescription src_fmt;
    UInt32 prop_sz = sizeof(src_fmt);
    memset(&src_fmt, 0, sizeof(src_fmt));
    if (ExtAudioFileGetProperty(g_ext_file, kExtAudioFileProperty_FileDataFormat, &prop_sz, &src_fmt) == noErr) {
        if (src_fmt.mSampleRate > 0) {
            g_now_sample_rate = (uint32_t)src_fmt.mSampleRate;
        }
        if (src_fmt.mBitsPerChannel > 0) {
            g_now_bit_depth = (uint32_t)src_fmt.mBitsPerChannel;
        } else {
            g_now_bit_depth = 24;
        }
    }

    SInt64 file_frames = 0;
    prop_sz = sizeof(file_frames);
    if (ExtAudioFileGetProperty(g_ext_file, kExtAudioFileProperty_FileLengthFrames, &prop_sz, &file_frames) == noErr &&
        file_frames > 0 && src_fmt.mSampleRate > 0) {
        g_now_duration_sec = (uint32_t)((double)file_frames / src_fmt.mSampleRate + 0.5);
        if (g_now_duration_sec > 0 && file_size_bytes > 0) {
            g_now_bitrate_kbps = (uint32_t)(((uint64_t)file_size_bytes * 8ULL) / ((uint64_t)g_now_duration_sec * 1000ULL));
        }
    }

    g_out_rate = (src_fmt.mSampleRate >= 44100.0 && src_fmt.mSampleRate <= 96000.0)
                     ? src_fmt.mSampleRate
                     : 48000.0;

    AudioStreamBasicDescription client_fmt;
    memset(&client_fmt, 0, sizeof(client_fmt));
    client_fmt.mSampleRate       = g_out_rate;
    client_fmt.mFormatID         = kAudioFormatLinearPCM;
    client_fmt.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    client_fmt.mBytesPerPacket   = 4;
    client_fmt.mFramesPerPacket  = 1;
    client_fmt.mBytesPerFrame    = 4;
    client_fmt.mChannelsPerFrame = 2;
    client_fmt.mBitsPerChannel   = 16;

    ExtAudioFileSetProperty(g_ext_file, kExtAudioFileProperty_ClientDataFormat, sizeof(client_fmt), &client_fmt);

    g_total_frames = (int64_t)((double)g_now_duration_sec * g_out_rate);
    g_base_frame   = 0;

    err = AudioQueueNewOutput(&client_fmt, aq_output_cb, NULL, NULL, NULL, 0, &g_aq);
    if (err != noErr || !g_aq) {
        ExtAudioFileDispose(g_ext_file);
        g_ext_file = NULL;
        g_aq = NULL;
        pthread_mutex_unlock(&g_audio_mtx);
        return false;
    }

    g_aq_running = true;
    g_aq_paused  = false;
    AudioQueueSetParameter(g_aq, kAudioQueueParam_Volume, g_gain_01);

    for (int i = 0; i < AUDIO_BUF_COUNT; ++i) {
        AudioQueueAllocateBuffer(g_aq, AUDIO_BUF_FRAMES * 4u, &g_aq_bufs[i]);
    }
    pthread_mutex_unlock(&g_audio_mtx);

    for (int i = 0; i < AUDIO_BUF_COUNT; ++i) {
        aq_output_cb(NULL, g_aq, g_aq_bufs[i]);
    }
    AudioQueueStart(g_aq, NULL);
    return true;
}
#endif

/* ── Helper: Check Audio Extension & Clean Track Title ── */
static bool is_audio_file(const char *name) {
    if (!name || name[0] == '.') return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return (strcasecmp(dot, ".flac") == 0 ||
            strcasecmp(dot, ".mp3")  == 0 ||
            strcasecmp(dot, ".wav")  == 0 ||
            strcasecmp(dot, ".m4a")  == 0 ||
            strcasecmp(dot, ".aiff") == 0);
}

static bool is_cover_image(const char *name) {
    if (!name || name[0] == '.') return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return (strcasecmp(dot, ".jpg")  == 0 ||
            strcasecmp(dot, ".jpeg") == 0 ||
            strcasecmp(dot, ".png")  == 0);
}

static void parse_track_filename(const char *filename, uint8_t *out_num, char *out_title, size_t title_cap) {
    *out_num = 0;
    const char *p = filename;
    if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1])) {
        *out_num = (uint8_t)((p[0] - '0') * 10 + (p[1] - '0'));
        p += 2;
        while (*p == ' ' || *p == '-' || *p == '.') p++;
    }
    strncpy(out_title, p, title_cap - 1);
    out_title[title_cap - 1] = '\0';
    char *dot = strrchr(out_title, '.');
    if (dot) *dot = '\0';
}

static int cmp_tracks(const void *a, const void *b) {
    const dial_track_info_t *ta = (const dial_track_info_t *)a;
    const dial_track_info_t *tb = (const dial_track_info_t *)b;
    if (ta->track_num != tb->track_num) {
        return (int)ta->track_num - (int)tb->track_num;
    }
    return strcasecmp(ta->filename, tb->filename);
}

static int cmp_albums(const void *a, const void *b) {
    const dial_album_info_t *aa = (const dial_album_info_t *)a;
    const dial_album_info_t *ab = (const dial_album_info_t *)b;
    return strcasecmp(aa->name, ab->name);
}

static int cmp_fs_entries(const void *a, const void *b) {
    const dial_fs_entry_t *ea = (const dial_fs_entry_t *)a;
    const dial_fs_entry_t *eb = (const dial_fs_entry_t *)b;
    if (strcmp(ea->name, "..") == 0) return -1;
    if (strcmp(eb->name, "..") == 0) return 1;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcasecmp(ea->name, eb->name);
}

bool dial_media_scan_root(const char *root_path) {
    if (!root_path || !*root_path) return false;
    DIR *root = opendir(root_path);
    if (!root) return false;

    memset(g_albums, 0, sizeof(g_albums));
    memset(g_cover_cache, 0, sizeof(g_cover_cache));
    g_album_count  = 0;
    g_total_tracks = 0;
    strncpy(g_root_path, root_path, sizeof(g_root_path) - 1);

    /* Extract artist name from root directory basename (e.g. "Au5") */
    const char *artist_base = strrchr(root_path, '/');
    artist_base = artist_base ? (artist_base + 1) : root_path;
    if (!*artist_base) artist_base = "Au5";

    struct dirent *de;
    while ((de = readdir(root)) != NULL && g_album_count < DIAL_MAX_ALBUMS) {
        if (de->d_name[0] == '.') continue;
        char sub_path[512];
        snprintf(sub_path, sizeof(sub_path), "%s/%s", root_path, de->d_name);

        struct stat st;
        if (stat(sub_path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        DIR *adir = opendir(sub_path);
        if (!adir) continue;

        dial_album_info_t *alb = &g_albums[g_album_count];
        memset(alb, 0, sizeof(*alb));
        strncpy(alb->name, de->d_name, sizeof(alb->name) - 1);
        strncpy(alb->artist, artist_base, sizeof(alb->artist) - 1);
        strncpy(alb->year, "2024", sizeof(alb->year) - 1);
        strncpy(alb->dir_path, sub_path, sizeof(alb->dir_path) - 1);

        struct dirent *fe;
        while ((fe = readdir(adir)) != NULL) {
            if (fe->d_name[0] == '.') continue;
            char fpath[512];
            snprintf(fpath, sizeof(fpath), "%s/%s", sub_path, fe->d_name);

            if (is_cover_image(fe->d_name)) {
                if (alb->cover_path[0] == '\0' || strcasecmp(fe->d_name, "cover.jpg") == 0) {
                    strncpy(alb->cover_path, fpath, sizeof(alb->cover_path) - 1);
                }
            } else if (is_audio_file(fe->d_name) && alb->track_count < DIAL_MAX_TRACKS_PER_ALB) {
                dial_track_info_t *tr = &alb->tracks[alb->track_count];
                strncpy(tr->filename, fe->d_name, sizeof(tr->filename) - 1);
                strncpy(tr->filepath, fpath, sizeof(tr->filepath) - 1);
                parse_track_filename(fe->d_name, &tr->track_num, tr->title, sizeof(tr->title));
                struct stat fst;
                if (stat(fpath, &fst) == 0) {
                    tr->file_size_bytes = (uint32_t)fst.st_size;
                    /* Estimate duration from FLAC/audio size (~165 KB/sec for 24/48 FLAC) */
                    tr->duration_sec = (uint32_t)(fst.st_size / 165000u);
                    if (tr->duration_sec < 30) tr->duration_sec = 210;
                } else {
                    tr->duration_sec = 220;
                }
                alb->track_count++;
                g_total_tracks++;
            }
        }
        closedir(adir);

        if (alb->track_count > 0) {
            for (size_t t = 0; t < alb->track_count; ++t) {
                if (alb->tracks[t].track_num == 0) {
                    alb->tracks[t].track_num = (uint8_t)(t + 1);
                }
            }
            qsort(alb->tracks, alb->track_count, sizeof(dial_track_info_t), cmp_tracks);
            g_album_count++;
        }
    }
    closedir(root);

    if (g_album_count == 0) return false;
    qsort(g_albums, g_album_count, sizeof(dial_album_info_t), cmp_albums);

    g_media_loaded = true;
    dial_media_browse_dir(root_path);

    /* Default to first multi-track LP if available (e.g. Bridges Between or Divinorum LP) */
    size_t init_alb = 0;
    for (size_t i = 0; i < g_album_count; ++i) {
        if (strcasecmp(g_albums[i].name, "Divinorum LP") == 0 ||
            strcasecmp(g_albums[i].name, "Bridges Between") == 0) {
            init_alb = i;
            break;
        }
    }
    dial_media_play_track(init_alb, 0);
    printf("[DialOS Media] Loaded %zu real albums (%zu tracks) from %s\n",
           g_album_count, g_total_tracks, root_path);
    return true;
}

bool dial_media_is_loaded(void) {
    return g_media_loaded;
}

const char *dial_media_get_root_path(void) {
    return g_root_path;
}

size_t dial_media_get_album_count(void) {
    return g_album_count;
}

size_t dial_media_get_total_track_count(void) {
    return g_total_tracks;
}

const dial_album_info_t *dial_media_get_album(size_t album_idx) {
    if (!g_media_loaded || g_album_count == 0) return NULL;
    return &g_albums[album_idx % g_album_count];
}

/* ── Real Cover Art Decoder (ImageIO + CoreGraphics -> 180x180 RGB565) ── */
const uint16_t *dial_media_get_album_cover_rgb565(size_t album_idx) {
    if (!g_media_loaded || g_album_count == 0) return NULL;
    album_idx %= g_album_count;

    for (size_t i = 0; i < DIAL_COVER_CACHE_SLOTS; ++i) {
        if (g_cover_cache[i].valid && g_cover_cache[i].album_idx == album_idx) {
            return g_cover_cache[i].pixels;
        }
    }

#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    const char *cpath = g_albums[album_idx].cover_path;
    if (!cpath || !*cpath) return NULL;

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, (const UInt8 *)cpath, (CFIndex)strlen(cpath), false);
    if (!url) return NULL;

    CGImageSourceRef src = CGImageSourceCreateWithURL(url, NULL);
    CFRelease(url);
    if (!src) return NULL;

    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
    if (!img) return NULL;

    cover_cache_slot_t *slot = &g_cover_cache[g_cover_next_slot];
    g_cover_next_slot = (g_cover_next_slot + 1) % DIAL_COVER_CACHE_SLOTS;

    static uint8_t rgba_scratch[DIAL_VINYL_TEX_DIM * DIAL_VINYL_TEX_DIM * 4];
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        rgba_scratch, DIAL_VINYL_TEX_DIM, DIAL_VINYL_TEX_DIM, 8, DIAL_VINYL_TEX_DIM * 4,
        cs, (CGBitmapInfo)kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);

    if (ctx) {
        CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
        CGContextDrawImage(ctx, CGRectMake(0, 0, DIAL_VINYL_TEX_DIM, DIAL_VINYL_TEX_DIM), img);
        CGContextRelease(ctx);

        for (int i = 0; i < DIAL_VINYL_TEX_DIM * DIAL_VINYL_TEX_DIM; ++i) {
            uint32_t r = rgba_scratch[i * 4 + 0];
            uint32_t g = rgba_scratch[i * 4 + 1];
            uint32_t b = rgba_scratch[i * 4 + 2];
            slot->pixels[i] = (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
        }
        slot->album_idx = album_idx;
        slot->valid     = true;
    }
    CGImageRelease(img);
    return slot->valid ? slot->pixels : NULL;
#else
    (void)g_cover_next_slot;
    return NULL;
#endif
}

/* ── Real Directory Browser (FIL-01..04) ── */
bool dial_media_browse_dir(const char *dir_path) {
    if (!dir_path || !*dir_path) return false;
    DIR *d = opendir(dir_path);
    if (!d) return false;

    strncpy(g_cur_dir_path, dir_path, sizeof(g_cur_dir_path) - 1);
    const char *base = strrchr(dir_path, '/');
    base = base ? (base + 1) : dir_path;
    snprintf(g_cur_dir_label, sizeof(g_cur_dir_label), "Music / %s", base);

    g_fs_entry_count = 0;

    /* Add ".." parent folder entry if deeper than root */
    if (strcmp(dir_path, g_root_path) != 0 && strlen(dir_path) > strlen(g_root_path)) {
        dial_fs_entry_t *up = &g_fs_entries[g_fs_entry_count++];
        memset(up, 0, sizeof(*up));
        strcpy(up->name, "..");
        strncpy(up->full_path, dir_path, sizeof(up->full_path) - 1);
        char *slash = strrchr(up->full_path, '/');
        if (slash) *slash = '\0';
        up->is_dir = true;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL && g_fs_entry_count < DIAL_MAX_DIR_ENTRIES) {
        if (de->d_name[0] == '.') continue;
        char fpath[512];
        snprintf(fpath, sizeof(fpath), "%s/%s", dir_path, de->d_name);
        struct stat st;
        if (stat(fpath, &st) != 0) continue;

        bool is_d = S_ISDIR(st.st_mode);
        if (!is_d && !is_audio_file(de->d_name)) continue;

        dial_fs_entry_t *ent = &g_fs_entries[g_fs_entry_count++];
        memset(ent, 0, sizeof(*ent));
        strncpy(ent->name, de->d_name, sizeof(ent->name) - 1);
        strncpy(ent->full_path, fpath, sizeof(ent->full_path) - 1);
        ent->is_dir     = is_d;
        ent->size_bytes = (uint32_t)st.st_size;
    }
    closedir(d);

    qsort(g_fs_entries, g_fs_entry_count, sizeof(dial_fs_entry_t), cmp_fs_entries);
    return true;
}

const char *dial_media_get_current_dir_label(void) {
    return g_media_loaded ? g_cur_dir_label : "sdcard / Music / Trip-hop";
}

size_t dial_media_get_fs_entry_count(void) {
    return g_fs_entry_count;
}

const dial_fs_entry_t *dial_media_get_fs_entry(size_t idx) {
    if (idx >= g_fs_entry_count) return NULL;
    return &g_fs_entries[idx];
}

/* ── Real Playback Control ── */
bool dial_media_play_track(size_t album_idx, size_t track_idx) {
    if (!g_media_loaded || g_album_count == 0) return false;
    g_now_album_idx = album_idx % g_album_count;
    const dial_album_info_t *alb = &g_albums[g_now_album_idx];
    if (alb->track_count == 0) return false;
    g_now_track_idx = track_idx % alb->track_count;
    const dial_track_info_t *tr = &alb->tracks[g_now_track_idx];

    strncpy(g_now_title, tr->title, sizeof(g_now_title) - 1);
    strncpy(g_now_artist, alb->artist, sizeof(g_now_artist) - 1);
    snprintf(g_now_album, sizeof(g_now_album), "%s", alb->name);
    strncpy(g_now_filepath, tr->filepath, sizeof(g_now_filepath) - 1);
    g_now_duration_sec  = tr->duration_sec;
    g_portable_playing  = true;
    g_portable_progress = 0.0f;

#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    start_real_audio_file(tr->filepath, tr->file_size_bytes);
#endif
    return true;
}

bool dial_media_play_file(const char *filepath, const char *title, const char *artist, const char *album) {
    if (!filepath) return false;
    if (title)  strncpy(g_now_title, title, sizeof(g_now_title) - 1);
    if (artist) strncpy(g_now_artist, artist, sizeof(g_now_artist) - 1);
    if (album)  strncpy(g_now_album, album, sizeof(g_now_album) - 1);
    strncpy(g_now_filepath, filepath, sizeof(g_now_filepath) - 1);
    g_portable_playing  = true;
    g_portable_progress = 0.0f;

    struct stat st;
    uint32_t sz = (stat(filepath, &st) == 0) ? (uint32_t)st.st_size : 0u;
#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    return start_real_audio_file(filepath, sz);
#else
    (void)sz;
    return true;
#endif
}

void dial_media_set_playing(bool playing) {
    g_portable_playing = playing;
#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    pthread_mutex_lock(&g_audio_mtx);
    if (g_aq && g_aq_running) {
        if (playing && g_aq_paused) {
            AudioQueueStart(g_aq, NULL);
            g_aq_paused = false;
        } else if (!playing && !g_aq_paused) {
            AudioQueuePause(g_aq);
            g_aq_paused = true;
        }
    }
    pthread_mutex_unlock(&g_audio_mtx);
#endif
}

bool dial_media_get_playing(void) {
#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    return g_aq_running && !g_aq_paused;
#else
    return g_portable_playing;
#endif
}

void dial_media_seek_progress(float progress_01) {
    if (progress_01 < 0.0f) progress_01 = 0.0f;
    if (progress_01 > 0.999f) progress_01 = 0.999f;
    g_portable_progress = progress_01;
#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    pthread_mutex_lock(&g_audio_mtx);
    if (g_ext_file && g_aq && g_total_frames > 0) {
        SInt64 target_frame = (SInt64)((double)progress_01 * (double)g_total_frames);
        ExtAudioFileSeek(g_ext_file, target_frame);
        g_base_frame = target_frame;
    }
    pthread_mutex_unlock(&g_audio_mtx);
#endif
}

float dial_media_get_progress(uint32_t *out_pos_sec, uint32_t *out_dur_sec) {
    if (out_dur_sec) *out_dur_sec = g_now_duration_sec;
#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    pthread_mutex_lock(&g_audio_mtx);
    if (g_ext_file && g_total_frames > 0) {
        SInt64 cur_frame = g_base_frame;
        ExtAudioFileTell(g_ext_file, &cur_frame);
        pthread_mutex_unlock(&g_audio_mtx);
        if (cur_frame < 0) cur_frame = 0;
        if (cur_frame > g_total_frames) cur_frame = g_total_frames;
        if (out_pos_sec && g_out_rate > 0.0) {
            *out_pos_sec = (uint32_t)((double)cur_frame / g_out_rate);
        }
        return (float)((double)cur_frame / (double)g_total_frames);
    }
    pthread_mutex_unlock(&g_audio_mtx);
#endif
    if (out_pos_sec) *out_pos_sec = (uint32_t)((float)g_now_duration_sec * g_portable_progress + 0.5f);
    return g_portable_progress;
}

void dial_media_set_volume(int vol_0_100) {
    if (vol_0_100 < 0) vol_0_100 = 0;
    if (vol_0_100 > 100) vol_0_100 = 100;
#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    g_gain_01 = (float)vol_0_100 / 100.0f;
    pthread_mutex_lock(&g_audio_mtx);
    if (g_aq) {
        AudioQueueSetParameter(g_aq, kAudioQueueParam_Volume, g_gain_01);
    }
    pthread_mutex_unlock(&g_audio_mtx);
#endif
}

void dial_media_next_track(void) {
    if (!g_media_loaded || g_album_count == 0) return;
    const dial_album_info_t *alb = &g_albums[g_now_album_idx];
    if (g_now_track_idx + 1 < alb->track_count) {
        dial_media_play_track(g_now_album_idx, g_now_track_idx + 1);
    } else {
        dial_media_play_track((g_now_album_idx + 1) % g_album_count, 0);
    }
}

void dial_media_prev_track(void) {
    if (!g_media_loaded || g_album_count == 0) return;
    if (g_now_track_idx > 0) {
        dial_media_play_track(g_now_album_idx, g_now_track_idx - 1);
    } else {
        size_t prev_alb = (g_now_album_idx + g_album_count - 1) % g_album_count;
        size_t last_tr = g_albums[prev_alb].track_count > 0 ? (g_albums[prev_alb].track_count - 1) : 0;
        dial_media_play_track(prev_alb, last_tr);
    }
}

const char *dial_media_get_now_title(void)       { return g_now_title; }
const char *dial_media_get_now_artist(void)      { return g_now_artist; }
const char *dial_media_get_now_album(void)       { return g_now_album; }
const char *dial_media_get_now_filepath(void)    { return g_now_filepath; }
uint32_t    dial_media_get_now_sample_rate(void) { return g_now_sample_rate; }
uint32_t    dial_media_get_now_bit_depth(void)   { return g_now_bit_depth; }
uint32_t    dial_media_get_now_bitrate_kbps(void){ return g_now_bitrate_kbps; }

const uint16_t *dial_media_get_now_cover_rgb565(void) {
    if (!g_media_loaded) return NULL;
    return dial_media_get_album_cover_rgb565(g_now_album_idx);
}
