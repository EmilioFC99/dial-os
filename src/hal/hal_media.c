/**
 * @file hal_media.c
 * @brief Real Audio Playback Engine, Recursive Library Scanner, Directory Browser & Telemetry
 *
 * Scans real music directories on disk (e.g. ~/Music on macOS Simulator,
 * /mnt/mmcblk0p1 on Snowsky DISC), decodes real album art into 180x180 RGB565
 * textures, streams real FLAC / MP3 / WAV / M4A / AIFF audio via AudioToolbox
 * on macOS and /player POSIX IPC on Snowsky DISC, and queries real system
 * telemetry (localtime clock, battery sysfs/pmset, statvfs disk space).
 */
#define _POSIX_C_SOURCE 200809L
#include "dial_media.h"
#include "dial_hal.h"
#include "dial_ipc.h"
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

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
static size_t            g_album_count  = 0;
static size_t            g_artist_count = 0;
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
static bool               g_now_has_custom_cover = false;
static uint16_t           g_now_custom_cover[DIAL_VINYL_TEX_DIM * DIAL_VINYL_TEX_DIM];

/* File Browser State */
static char            g_cur_dir_path[512];
static char            g_cur_dir_label[128];
static dial_fs_entry_t g_fs_entries[DIAL_MAX_DIR_ENTRIES];
static size_t          g_fs_entry_count = 0;

/* Active Folder Playback Queue (populated when playing from Files) */
typedef struct {
    char     filepath[512];
    char     filename[128];
    uint32_t size_bytes;
} folder_queue_item_t;

static folder_queue_item_t g_folder_queue[DIAL_MAX_DIR_ENTRIES];
static size_t              g_folder_queue_count = 0;
static size_t              g_folder_queue_idx   = 0;
static bool                g_playing_from_folder = false;

/* Active Now-Playing State (starts cleanly in "Not Playing" state) */
static bool     g_has_active_track    = false;
static size_t   g_now_album_idx       = 0;
static size_t   g_now_track_idx       = 0;
static char     g_now_title[96]       = "Not Playing";
static char     g_now_artist[64]      = "";
static char     g_now_album[96]       = "";
static char     g_now_filepath[512]   = "";
static char     g_now_format[16]      = "";
static uint32_t g_now_file_size       = 0;
static uint32_t g_now_sample_rate     = 0;
static uint32_t g_now_bit_depth       = 0;
static uint32_t g_now_bitrate_kbps    = 0;
static uint32_t g_now_duration_sec    = 0;
static bool     g_portable_playing    = false;
static float    g_portable_progress   = 0.0f;
static uint32_t g_play_start_tick_ms  = 0;
static int      g_volume_0_100        = 76;

/* ── Helper: Check Audio & Image Extensions ── */
static bool is_audio_file(const char *name) {
    if (!name || name[0] == '.') return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return (strcasecmp(dot, ".flac") == 0 ||
            strcasecmp(dot, ".mp3")  == 0 ||
            strcasecmp(dot, ".wav")  == 0 ||
            strcasecmp(dot, ".m4a")  == 0 ||
            strcasecmp(dot, ".aiff") == 0 ||
            strcasecmp(dot, ".aif")  == 0 ||
            strcasecmp(dot, ".ogg")  == 0 ||
            strcasecmp(dot, ".opus") == 0 ||
            strcasecmp(dot, ".aac")  == 0 ||
            strcasecmp(dot, ".dsf")  == 0);
}

static bool is_cover_image(const char *name) {
    if (!name || name[0] == '.') return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return (strcasecmp(dot, ".jpg")  == 0 ||
            strcasecmp(dot, ".jpeg") == 0 ||
            strcasecmp(dot, ".png")  == 0);
}

static void extract_format_name(const char *filepath, char *out_fmt, size_t cap) {
    if (!out_fmt || cap == 0) return;
    out_fmt[0] = '\0';
    if (!filepath) return;
    const char *dot = strrchr(filepath, '.');
    if (!dot || !dot[1]) {
        strncpy(out_fmt, "AUDIO", cap - 1);
        out_fmt[cap - 1] = '\0';
        return;
    }
    const char *ext = dot + 1;
    size_t i = 0;
    for (; ext[i] && i < cap - 1; ++i) {
        out_fmt[i] = (char)toupper((unsigned char)ext[i]);
    }
    out_fmt[i] = '\0';
}

/* Native FLAC / WAV Binary Header Probe (works on both Snowsky DISC Linux & macOS) */
static void probe_audio_header(const char *filepath,
                               uint32_t file_size_bytes,
                               uint32_t *io_sample_rate,
                               uint32_t *io_bit_depth,
                               uint32_t *io_duration_sec,
                               uint32_t *io_bitrate_kbps) {
    if (!filepath || !*filepath) return;
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return;

    uint8_t hdr[64];
    size_t n = fread(hdr, 1, sizeof(hdr), fp);
    fclose(fp);

    /* 1. Native FLAC STREAMINFO header ("fLaC") */
    if (n >= 26 && hdr[0] == 'f' && hdr[1] == 'L' && hdr[2] == 'a' && hdr[3] == 'C') {
        uint32_t sr = ((uint32_t)hdr[18] << 12) | ((uint32_t)hdr[19] << 4) | ((uint32_t)hdr[20] >> 4);
        uint32_t bps = ((((uint32_t)hdr[20] & 0x01u) << 4) | ((uint32_t)hdr[21] >> 4)) + 1u;
        uint64_t total_samples = (((uint64_t)(hdr[21] & 0x0Fu)) << 32) |
                                 (((uint64_t)hdr[22]) << 24) |
                                 (((uint64_t)hdr[23]) << 16) |
                                 (((uint64_t)hdr[24]) << 8)  |
                                 ((uint64_t)hdr[25]);
        if (sr > 0) *io_sample_rate = sr;
        if (bps > 0) *io_bit_depth = bps;
        if (sr > 0 && total_samples > 0) {
            uint32_t dur = (uint32_t)((total_samples + sr / 2u) / sr);
            if (dur == 0) dur = 1;
            *io_duration_sec = dur;
            if (file_size_bytes > 0) {
                *io_bitrate_kbps = (uint32_t)(((uint64_t)file_size_bytes * 8ULL) / ((uint64_t)dur * 1000ULL));
            }
        }
        return;
    }

    /* 2. Native RIFF WAVE header ("RIFF....WAVEfmt ") */
    if (n >= 44 && hdr[0] == 'R' && hdr[1] == 'I' && hdr[2] == 'F' && hdr[3] == 'F' &&
        hdr[8] == 'W' && hdr[9] == 'A' && hdr[10] == 'V' && hdr[11] == 'E') {
        uint32_t sr = (uint32_t)hdr[24] | ((uint32_t)hdr[25] << 8) |
                      ((uint32_t)hdr[26] << 16) | ((uint32_t)hdr[27] << 24);
        uint32_t byte_rate = (uint32_t)hdr[28] | ((uint32_t)hdr[29] << 8) |
                             ((uint32_t)hdr[30] << 16) | ((uint32_t)hdr[31] << 24);
        uint32_t bps = (uint32_t)hdr[34] | ((uint32_t)hdr[35] << 8);
        if (sr > 0) *io_sample_rate = sr;
        if (bps > 0) *io_bit_depth = bps;
        if (byte_rate > 0 && file_size_bytes > 44) {
            uint32_t dur = (file_size_bytes - 44u + byte_rate / 2u) / byte_rate;
            if (dur == 0) dur = 1;
            *io_duration_sec = dur;
            *io_bitrate_kbps = (byte_rate * 8u) / 1000u;
        }
        return;
    }

    /* Fallback estimate from file size & extension */
    if (*io_sample_rate == 0) *io_sample_rate = 44100;
    if (*io_bit_depth == 0)   *io_bit_depth   = 16;
    if (*io_duration_sec == 0 && file_size_bytes > 0) {
        const char *dot = strrchr(filepath, '.');
        uint32_t bytes_per_sec = (dot && strcasecmp(dot, ".flac") == 0) ? 110000u : 32000u;
        uint32_t dur = file_size_bytes / bytes_per_sec;
        if (dur == 0) dur = 1;
        *io_duration_sec = dur;
        *io_bitrate_kbps = (bytes_per_sec * 8u) / 1000u;
    }
}

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
static float               g_gain_01      = 0.76f;

static bool decode_cover_file_rgb565(const char *cpath, uint16_t *out_pixels) {
    if (!cpath || !*cpath || !out_pixels) return false;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, (const UInt8 *)cpath, (CFIndex)strlen(cpath), false);
    if (!url) return false;

    CGImageSourceRef src = CGImageSourceCreateWithURL(url, NULL);
    CFRelease(url);
    if (!src) return false;

    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
    if (!img) return false;

    static uint8_t rgba_scratch[DIAL_VINYL_TEX_DIM * DIAL_VINYL_TEX_DIM * 4];
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        rgba_scratch, DIAL_VINYL_TEX_DIM, DIAL_VINYL_TEX_DIM, 8, DIAL_VINYL_TEX_DIM * 4,
        cs, (CGBitmapInfo)kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);

    bool ok = false;
    if (ctx) {
        CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
        CGContextDrawImage(ctx, CGRectMake(0, 0, DIAL_VINYL_TEX_DIM, DIAL_VINYL_TEX_DIM), img);
        CGContextRelease(ctx);

        for (int i = 0; i < DIAL_VINYL_TEX_DIM * DIAL_VINYL_TEX_DIM; ++i) {
            uint32_t r = rgba_scratch[i * 4 + 0];
            uint32_t g = rgba_scratch[i * 4 + 1];
            uint32_t b = rgba_scratch[i * 4 + 2];
            out_pixels[i] = (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
        }
        ok = true;
    }
    CGImageRelease(img);
    return ok;
}

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
        }
    }

    SInt64 file_frames = 0;
    prop_sz = sizeof(file_frames);
    if (ExtAudioFileGetProperty(g_ext_file, kExtAudioFileProperty_FileLengthFrames, &prop_sz, &file_frames) == noErr &&
        file_frames > 0 && src_fmt.mSampleRate > 0) {
        g_now_duration_sec = (uint32_t)((double)file_frames / src_fmt.mSampleRate + 0.5);
        if (g_now_duration_sec == 0) g_now_duration_sec = 1;
        if (file_size_bytes > 0) {
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
    if (g_total_frames <= 0) g_total_frames = (int64_t)g_out_rate;
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

/* ── Track Filename & Artist/Title Parser ── */
static void parse_track_filename(const char *filename,
                                 uint8_t *out_num,
                                 char *out_title,
                                 size_t title_cap,
                                 char *out_artist,
                                 size_t artist_cap) {
    if (out_num) *out_num = 0;
    if (out_artist && artist_cap > 0) out_artist[0] = '\0';

    const char *p = filename ? filename : "";
    if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1])) {
        if (out_num) *out_num = (uint8_t)((p[0] - '0') * 10 + (p[1] - '0'));
        p += 2;
        while (*p == ' ' || *p == '-' || *p == '.' || *p == '_') p++;
    } else if (isdigit((unsigned char)p[0]) && (p[1] == '.' || p[1] == '-' || p[1] == ' ')) {
        if (out_num) *out_num = (uint8_t)(p[0] - '0');
        p += 2;
        while (*p == ' ' || *p == '-' || *p == '.') p++;
    }

    char raw[128];
    strncpy(raw, p, sizeof(raw) - 1);
    raw[sizeof(raw) - 1] = '\0';
    char *dot = strrchr(raw, '.');
    if (dot) *dot = '\0';

    /* Check if filename contains "Artist - Title" */
    char *sep = strstr(raw, " - ");
    if (sep && out_artist && artist_cap > 0) {
        *sep = '\0';
        strncpy(out_artist, raw, artist_cap - 1);
        out_artist[artist_cap - 1] = '\0';
        strncpy(out_title, sep + 3, title_cap - 1);
        out_title[title_cap - 1] = '\0';
    } else {
        strncpy(out_title, raw, title_cap - 1);
        out_title[title_cap - 1] = '\0';
    }
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

/* Recursively scan directories up to depth 5 for folders containing real audio files */
static void scan_dir_recursive(const char *dir_path, const char *parent_name, int depth) {
    if (depth > 5 || g_album_count >= DIAL_MAX_ALBUMS) return;
    DIR *d = opendir(dir_path);
    if (!d) return;

    const char *dir_base = strrchr(dir_path, '/');
    dir_base = dir_base ? (dir_base + 1) : dir_path;

    /* First pass: collect audio files directly inside dir_path */
    dial_album_info_t *alb = &g_albums[g_album_count];
    memset(alb, 0, sizeof(*alb));
    strncpy(alb->name, (*dir_base) ? dir_base : "Root", sizeof(alb->name) - 1);
    strncpy(alb->artist, (parent_name && *parent_name) ? parent_name : alb->name, sizeof(alb->artist) - 1);
    strncpy(alb->dir_path, dir_path, sizeof(alb->dir_path) - 1);

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (strstr(de->d_name, ".musiclibrary") != NULL) continue;

        bool is_cover = is_cover_image(de->d_name);
        bool is_audio = !is_cover && is_audio_file(de->d_name);
        if (!is_cover && !is_audio) continue;

        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir_path, de->d_name);

        if (is_cover) {
            if (alb->cover_path[0] == '\0' ||
                strcasecmp(de->d_name, "cover.jpg") == 0 ||
                strcasecmp(de->d_name, "folder.jpg") == 0) {
                strncpy(alb->cover_path, full, sizeof(alb->cover_path) - 1);
            }
        } else if (is_audio && alb->track_count < DIAL_MAX_TRACKS_PER_ALB) {
            struct stat st;
            uint32_t fsize = (stat(full, &st) == 0 && S_ISREG(st.st_mode)) ? (uint32_t)st.st_size : 0u;
            dial_track_info_t *tr = &alb->tracks[alb->track_count];
            memset(tr, 0, sizeof(*tr));
            strncpy(tr->filename, de->d_name, sizeof(tr->filename) - 1);
            strncpy(tr->filepath, full, sizeof(tr->filepath) - 1);
            char parsed_artist[64];
            parse_track_filename(de->d_name, &tr->track_num, tr->title, sizeof(tr->title),
                                 parsed_artist, sizeof(parsed_artist));
            if (parsed_artist[0] != '\0' && strcmp(alb->artist, alb->name) == 0) {
                strncpy(alb->artist, parsed_artist, sizeof(alb->artist) - 1);
            }
            tr->file_size_bytes = fsize;
            uint32_t est_dur = (fsize > 0) ? (fsize / 110000u) : 180u;
            if (est_dur < 1u) est_dur = 1u;
            tr->duration_sec = est_dur;
            alb->track_count++;
            g_total_tracks++;
        }
    }
    closedir(d);

    if (alb->track_count > 0) {
        for (size_t t = 0; t < alb->track_count; ++t) {
            if (alb->tracks[t].track_num == 0) {
                alb->tracks[t].track_num = (uint8_t)(t + 1);
            }
        }
        qsort(alb->tracks, alb->track_count, sizeof(dial_track_info_t), cmp_tracks);
        g_album_count++;
    }

    /* Second pass: recurse into subdirectories */
    d = opendir(dir_path);
    if (!d) return;
    while ((de = readdir(d)) != NULL && g_album_count < DIAL_MAX_ALBUMS) {
        if (de->d_name[0] == '.') continue;
        if (strstr(de->d_name, ".musiclibrary") != NULL) continue;
#if defined(DT_DIR) && defined(DT_UNKNOWN)
        if (de->d_type != DT_DIR && de->d_type != DT_UNKNOWN) continue;
#endif
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir_path, de->d_name);
#if defined(DT_DIR)
        bool is_sub = (de->d_type == DT_DIR);
        if (!is_sub) {
            struct stat st;
            is_sub = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));
        }
#else
        struct stat st;
        bool is_sub = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));
#endif
        if (is_sub) {
            const char *next_parent = (depth == 0) ? NULL : dir_base;
            scan_dir_recursive(full, next_parent, depth + 1);
        }
    }
    closedir(d);
}

bool dial_media_scan_root(const char *root_path) {
    if (!root_path || !*root_path) return false;

    memset(g_albums, 0, sizeof(g_albums));
    memset(g_cover_cache, 0, sizeof(g_cover_cache));
    g_album_count  = 0;
    g_artist_count = 0;
    g_total_tracks = 0;

    strncpy(g_root_path, root_path, sizeof(g_root_path) - 1);
    g_root_path[sizeof(g_root_path) - 1] = '\0';
    size_t rlen = strlen(g_root_path);
    while (rlen > 1 && g_root_path[rlen - 1] == '/') {
        g_root_path[--rlen] = '\0';
    }

    DIR *root = opendir(g_root_path);
    if (!root) {
        g_media_loaded = false;
        return false;
    }
    closedir(root);

    scan_dir_recursive(g_root_path, NULL, 0);

    if (g_album_count > 1) {
        qsort(g_albums, g_album_count, sizeof(dial_album_info_t), cmp_albums);
    }

    /* Count unique artists across discovered albums */
    for (size_t i = 0; i < g_album_count; ++i) {
        bool seen = false;
        for (size_t j = 0; j < i; ++j) {
            if (strcasecmp(g_albums[i].artist, g_albums[j].artist) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen) g_artist_count++;
    }

    /* Always initialize the real directory browser at root_path even if 0 tracks */
    dial_media_browse_dir(g_root_path);
    g_media_loaded = (g_album_count > 0);

    printf("[DialOS Media] Scanned %s: %zu albums, %zu artists, %zu tracks\n",
           g_root_path, g_album_count, g_artist_count, g_total_tracks);
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

size_t dial_media_get_artist_count(void) {
    return g_artist_count;
}

size_t dial_media_get_total_track_count(void) {
    return g_total_tracks;
}

const dial_album_info_t *dial_media_get_album(size_t album_idx) {
    if (g_album_count == 0) return NULL;
    return &g_albums[album_idx % g_album_count];
}

/* ── Real Cover Art Decoder (ImageIO + CoreGraphics -> 180x180 RGB565) ── */
const uint16_t *dial_media_get_album_cover_rgb565(size_t album_idx) {
    if (g_album_count == 0) return NULL;
    album_idx %= g_album_count;

    for (size_t i = 0; i < DIAL_COVER_CACHE_SLOTS; ++i) {
        if (g_cover_cache[i].valid && g_cover_cache[i].album_idx == album_idx) {
            return g_cover_cache[i].pixels;
        }
    }

#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    const char *cpath = g_albums[album_idx].cover_path;
    if (!cpath || !*cpath) return NULL;

    cover_cache_slot_t *slot = &g_cover_cache[g_cover_next_slot];
    if (decode_cover_file_rgb565(cpath, slot->pixels)) {
        g_cover_next_slot = (g_cover_next_slot + 1) % DIAL_COVER_CACHE_SLOTS;
        slot->album_idx = album_idx;
        slot->valid     = true;
        return slot->pixels;
    }
    return NULL;
#else
    (void)g_cover_next_slot;
    return NULL;
#endif
}

/* ── Real Directory Browser (FIL-01..04) ── */
static uint32_t count_dir_visible_items(const char *path) {
    DIR *d = opendir(path);
    if (!d) return 0;
    uint32_t count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
#if defined(DT_DIR) && defined(DT_REG)
        if (de->d_type == DT_DIR) {
            count++;
            continue;
        }
        if (de->d_type == DT_REG) {
            if (is_audio_file(de->d_name)) count++;
            continue;
        }
#endif
        char sub[512];
        snprintf(sub, sizeof(sub), "%s/%s", path, de->d_name);
        struct stat st;
        if (stat(sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode) || is_audio_file(de->d_name)) {
            count++;
        }
    }
    closedir(d);
    return count;
}

static void update_dir_breadcrumb_label(const char *dir_path) {
    const char *root_base = strrchr(g_root_path, '/');
    root_base = (root_base && root_base[1]) ? (root_base + 1) : g_root_path;
    if (strncmp(g_root_path, "/mnt/mmcblk", 11) == 0) {
        root_base = "microSD";
    } else if (strlen(root_base) > 10) {
        root_base = "Root";
    }

    size_t rlen = strlen(g_root_path);
    if (strcmp(dir_path, g_root_path) == 0 || strlen(dir_path) <= rlen) {
        snprintf(g_cur_dir_label, sizeof(g_cur_dir_label), "Files · %s", root_base);
    } else {
        const char *rel = dir_path + rlen;
        while (*rel == '/') rel++;
        const char *last_slash = strrchr(rel, '/');
        const char *leaf = last_slash ? (last_slash + 1) : rel;
        snprintf(g_cur_dir_label, sizeof(g_cur_dir_label), "%s / %s", root_base, leaf);
    }
    if (strlen(g_cur_dir_label) > 22) {
        g_cur_dir_label[22] = '\0';
    }
}

bool dial_media_browse_dir(const char *dir_path) {
    if (!dir_path || !*dir_path) return false;
    DIR *d = opendir(dir_path);
    if (!d) return false;

    strncpy(g_cur_dir_path, dir_path, sizeof(g_cur_dir_path) - 1);
    g_cur_dir_path[sizeof(g_cur_dir_path) - 1] = '\0';
    size_t clen = strlen(g_cur_dir_path);
    while (clen > 1 && g_cur_dir_path[clen - 1] == '/') {
        g_cur_dir_path[--clen] = '\0';
    }

    update_dir_breadcrumb_label(g_cur_dir_path);
    g_fs_entry_count = 0;

    /* Add ".." parent folder entry if deeper than root_path */
    if (!dial_media_is_at_root_dir()) {
        dial_fs_entry_t *up = &g_fs_entries[g_fs_entry_count++];
        memset(up, 0, sizeof(*up));
        strcpy(up->name, "..");
        strncpy(up->full_path, g_cur_dir_path, sizeof(up->full_path) - 1);
        char *slash = strrchr(up->full_path, '/');
        if (slash && slash != up->full_path) {
            *slash = '\0';
        } else {
            strcpy(up->full_path, "/");
        }
        up->is_dir = true;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL && g_fs_entry_count < DIAL_MAX_DIR_ENTRIES) {
        if (de->d_name[0] == '.') continue;
        char fpath[512];
        snprintf(fpath, sizeof(fpath), "%s/%s", g_cur_dir_path, de->d_name);
        struct stat st;
        if (stat(fpath, &st) != 0) continue;

        bool is_d = S_ISDIR(st.st_mode);
        if (!is_d && !is_audio_file(de->d_name)) continue;

        dial_fs_entry_t *ent = &g_fs_entries[g_fs_entry_count++];
        memset(ent, 0, sizeof(*ent));
        strncpy(ent->name, de->d_name, sizeof(ent->name) - 1);
        strncpy(ent->full_path, fpath, sizeof(ent->full_path) - 1);
        ent->is_dir     = is_d;
        ent->size_bytes = is_d ? 0u : (uint32_t)st.st_size;
        ent->sub_count  = is_d ? count_dir_visible_items(fpath) : 0u;
    }
    closedir(d);

    if (g_fs_entry_count > 1) {
        qsort(g_fs_entries, g_fs_entry_count, sizeof(dial_fs_entry_t), cmp_fs_entries);
    }
    return true;
}

bool dial_media_is_at_root_dir(void) {
    if (!g_root_path[0] || !g_cur_dir_path[0]) return true;
    return strcmp(g_cur_dir_path, g_root_path) == 0 ||
           strlen(g_cur_dir_path) <= strlen(g_root_path);
}

bool dial_media_browse_parent_dir(void) {
    if (dial_media_is_at_root_dir()) return false;
    char parent[512];
    strncpy(parent, g_cur_dir_path, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    char *slash = strrchr(parent, '/');
    if (!slash) return false;
    if (slash == parent) {
        parent[1] = '\0';
    } else {
        *slash = '\0';
    }
    return dial_media_browse_dir(parent);
}

const char *dial_media_get_current_dir_path(void) {
    return g_cur_dir_path;
}

const char *dial_media_get_current_dir_label(void) {
    return g_cur_dir_label[0] ? g_cur_dir_label : "Files";
}

size_t dial_media_get_fs_entry_count(void) {
    return g_fs_entry_count;
}

const dial_fs_entry_t *dial_media_get_fs_entry(size_t idx) {
    if (idx >= g_fs_entry_count) return NULL;
    return &g_fs_entries[idx];
}

static int remove_recursive(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    if (!S_ISDIR(st.st_mode)) {
        return unlink(path);
    }
    DIR *d = opendir(path);
    if (!d) return -1;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        char sub[512];
        snprintf(sub, sizeof(sub), "%s/%s", path, de->d_name);
        (void)remove_recursive(sub);
    }
    closedir(d);
    return rmdir(path);
}

bool dial_media_delete_fs_entry(size_t idx) {
    if (idx >= g_fs_entry_count) return false;
    const dial_fs_entry_t *ent = &g_fs_entries[idx];
    if (strcmp(ent->name, "..") == 0) return false;

    char target[512];
    strncpy(target, ent->full_path, sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';

    dial_hal_send_ipc(DIAL_CMD_DELETE_FILE, target);
    int rc = remove_recursive(target);
    dial_media_browse_dir(g_cur_dir_path);
    return (rc == 0);
}

/* ── Real Playback Control ── */
static void load_folder_cover_for_file(const char *filepath) {
    g_now_has_custom_cover = false;
#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    if (!filepath || !*filepath) return;
    char dir[512];
    strncpy(dir, filepath, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *slash = strrchr(dir, '/');
    if (!slash) return;
    *slash = '\0';

    DIR *d = opendir(dir);
    if (!d) return;
    char best_cover[512] = "";
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (is_cover_image(de->d_name)) {
            if (best_cover[0] == '\0' ||
                strcasecmp(de->d_name, "cover.jpg") == 0 ||
                strcasecmp(de->d_name, "folder.jpg") == 0) {
                snprintf(best_cover, sizeof(best_cover), "%s/%s", dir, de->d_name);
            }
        }
    }
    closedir(d);
    if (best_cover[0] != '\0') {
        g_now_has_custom_cover = decode_cover_file_rgb565(best_cover, g_now_custom_cover);
    }
#else
    (void)filepath;
#endif
}

bool dial_media_has_active_track(void) {
    return g_has_active_track;
}

bool dial_media_play_track(size_t album_idx, size_t track_idx) {
    if (g_album_count == 0) return false;
    g_playing_from_folder = false;
    g_now_album_idx = album_idx % g_album_count;
    const dial_album_info_t *alb = &g_albums[g_now_album_idx];
    if (alb->track_count == 0) return false;
    g_now_track_idx = track_idx % alb->track_count;
    const dial_track_info_t *tr = &alb->tracks[g_now_track_idx];

    strncpy(g_now_title, tr->title, sizeof(g_now_title) - 1);
    strncpy(g_now_artist, alb->artist, sizeof(g_now_artist) - 1);
    snprintf(g_now_album, sizeof(g_now_album), "%s", alb->name);
    strncpy(g_now_filepath, tr->filepath, sizeof(g_now_filepath) - 1);
    extract_format_name(tr->filepath, g_now_format, sizeof(g_now_format));
    g_now_file_size     = tr->file_size_bytes;
    g_now_duration_sec  = tr->duration_sec;
    g_now_sample_rate   = 0;
    g_now_bit_depth     = 0;
    g_now_bitrate_kbps  = 0;
    probe_audio_header(tr->filepath, tr->file_size_bytes,
                       &g_now_sample_rate, &g_now_bit_depth,
                       &g_now_duration_sec, &g_now_bitrate_kbps);

    g_has_active_track   = true;
    g_portable_playing   = true;
    g_portable_progress  = 0.0f;
    g_play_start_tick_ms = dial_hal_ticks_ms();
    g_now_has_custom_cover = false;

    dial_hal_send_ipc("0602", tr->filepath);
    dial_hal_send_ipc(DIAL_CMD_PLAY_PAUSE, "play");

#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    start_real_audio_file(tr->filepath, tr->file_size_bytes);
#endif
    return true;
}

bool dial_media_play_file(const char *filepath, const char *title, const char *artist, const char *album) {
    if (!filepath || !*filepath) return false;

    struct stat st;
    uint32_t sz = (stat(filepath, &st) == 0) ? (uint32_t)st.st_size : 0u;

    const char *fname = strrchr(filepath, '/');
    fname = fname ? (fname + 1) : filepath;

    char parsed_title[96]  = "";
    char parsed_artist[64] = "";
    uint8_t tr_num = 0;
    parse_track_filename(fname, &tr_num, parsed_title, sizeof(parsed_title),
                         parsed_artist, sizeof(parsed_artist));

    /* Infer album & artist from parent directories if not explicitly passed */
    char dir_copy[512];
    strncpy(dir_copy, filepath, sizeof(dir_copy) - 1);
    dir_copy[sizeof(dir_copy) - 1] = '\0';
    char *last_slash = strrchr(dir_copy, '/');
    const char *folder_album = "";
    const char *folder_artist = "";
    if (last_slash) {
        *last_slash = '\0';
        char *alb_slash = strrchr(dir_copy, '/');
        if (alb_slash) {
            folder_album = alb_slash + 1;
            *alb_slash = '\0';
            char *art_slash = strrchr(dir_copy, '/');
            folder_artist = art_slash ? (art_slash + 1) : dir_copy;
        } else {
            folder_album = dir_copy;
        }
    }

    strncpy(g_now_title, (title && *title) ? title : parsed_title, sizeof(g_now_title) - 1);
    if (artist && *artist) {
        strncpy(g_now_artist, artist, sizeof(g_now_artist) - 1);
    } else if (parsed_artist[0] != '\0') {
        strncpy(g_now_artist, parsed_artist, sizeof(g_now_artist) - 1);
    } else if (folder_artist && *folder_artist) {
        strncpy(g_now_artist, folder_artist, sizeof(g_now_artist) - 1);
    } else {
        strncpy(g_now_artist, folder_album, sizeof(g_now_artist) - 1);
    }

    strncpy(g_now_album, (album && *album) ? album : folder_album, sizeof(g_now_album) - 1);
    strncpy(g_now_filepath, filepath, sizeof(g_now_filepath) - 1);
    extract_format_name(filepath, g_now_format, sizeof(g_now_format));

    g_now_file_size     = sz;
    g_now_sample_rate   = 0;
    g_now_bit_depth     = 0;
    g_now_duration_sec  = 0;
    g_now_bitrate_kbps  = 0;
    probe_audio_header(filepath, sz,
                       &g_now_sample_rate, &g_now_bit_depth,
                       &g_now_duration_sec, &g_now_bitrate_kbps);

    g_has_active_track   = true;
    g_portable_playing   = true;
    g_portable_progress  = 0.0f;
    g_play_start_tick_ms = dial_hal_ticks_ms();

    load_folder_cover_for_file(filepath);

    dial_hal_send_ipc("0602", filepath);
    dial_hal_send_ipc(DIAL_CMD_PLAY_PAUSE, "play");

#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    return start_real_audio_file(filepath, sz);
#else
    return true;
#endif
}

bool dial_media_play_fs_entry(size_t idx) {
    if (idx >= g_fs_entry_count) return false;
    const dial_fs_entry_t *target = &g_fs_entries[idx];
    if (target->is_dir) return false;

    /* Build folder queue of all audio files in g_cur_dir_path */
    g_folder_queue_count = 0;
    g_folder_queue_idx   = 0;
    for (size_t i = 0; i < g_fs_entry_count; ++i) {
        if (!g_fs_entries[i].is_dir) {
            if (i == idx) {
                g_folder_queue_idx = g_folder_queue_count;
            }
            folder_queue_item_t *qi = &g_folder_queue[g_folder_queue_count++];
            strncpy(qi->filepath, g_fs_entries[i].full_path, sizeof(qi->filepath) - 1);
            strncpy(qi->filename, g_fs_entries[i].name, sizeof(qi->filename) - 1);
            qi->size_bytes = g_fs_entries[i].size_bytes;
        }
    }
    g_playing_from_folder = (g_folder_queue_count > 0);
    return dial_media_play_file(target->full_path, NULL, NULL, NULL);
}

void dial_media_set_playing(bool playing) {
    if (!g_has_active_track) return;
    g_portable_playing = playing;
    g_play_start_tick_ms = dial_hal_ticks_ms();
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
    if (!g_has_active_track) return false;
#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    if (g_aq && g_aq_running) {
        return !g_aq_paused;
    }
#endif
    return g_portable_playing;
}

void dial_media_seek_progress(float progress_01) {
    if (!g_has_active_track) return;
    if (progress_01 < 0.0f) progress_01 = 0.0f;
    if (progress_01 > 0.999f) progress_01 = 0.999f;
    g_portable_progress  = progress_01;
    g_play_start_tick_ms = dial_hal_ticks_ms();
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
    if (!g_has_active_track) {
        if (out_pos_sec) *out_pos_sec = 0;
        return 0.0f;
    }
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
    /* Advance portable elapsed progress from monotonic clock when playing */
    if (g_portable_playing && g_now_duration_sec > 0) {
        uint32_t now_ms = dial_hal_ticks_ms();
        uint32_t dt_ms  = (now_ms > g_play_start_tick_ms) ? (now_ms - g_play_start_tick_ms) : 0u;
        g_play_start_tick_ms = now_ms;
        g_portable_progress += (float)dt_ms / ((float)g_now_duration_sec * 1000.0f);
        if (g_portable_progress >= 1.0f) {
            g_portable_progress = 0.0f;
        }
    }
    if (out_pos_sec) {
        *out_pos_sec = (uint32_t)((float)g_now_duration_sec * g_portable_progress + 0.5f);
    }
    return g_portable_progress;
}

void dial_media_set_volume(int vol_0_100) {
    if (vol_0_100 < 0) vol_0_100 = 0;
    if (vol_0_100 > 100) vol_0_100 = 100;
    g_volume_0_100 = vol_0_100;

    char payload[16];
    snprintf(payload, sizeof(payload), "%d", vol_0_100);
    dial_hal_send_ipc(DIAL_CMD_VOL_SET, payload);

#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    g_gain_01 = (float)vol_0_100 / 100.0f;
    pthread_mutex_lock(&g_audio_mtx);
    if (g_aq) {
        AudioQueueSetParameter(g_aq, kAudioQueueParam_Volume, g_gain_01);
    }
    pthread_mutex_unlock(&g_audio_mtx);
#endif
}

int dial_media_get_volume(void) {
    return g_volume_0_100;
}

static dial_play_mode_t g_play_mode = DIAL_PLAY_MODE_SEQUENTIAL;
static uint32_t         g_rng_state = 0x9E3779B9u;

static uint32_t next_rand_u32(void) {
    g_rng_state ^= (g_rng_state << 13);
    g_rng_state ^= (g_rng_state >> 17);
    g_rng_state ^= (g_rng_state << 5);
    g_rng_state += dial_hal_ticks_ms() | 1u;
    return g_rng_state;
}

void dial_media_set_play_mode(dial_play_mode_t mode) {
    g_play_mode = (dial_play_mode_t)((unsigned)mode % (unsigned)DIAL_PLAY_MODE_COUNT);
    char payload[8];
    snprintf(payload, sizeof(payload), "%u", (unsigned)g_play_mode);
    dial_hal_send_ipc(DIAL_CMD_PLAY_MODE, payload);
}

dial_play_mode_t dial_media_get_play_mode(void) {
    return g_play_mode;
}

dial_play_mode_t dial_media_cycle_play_mode(void) {
    dial_play_mode_t next = (dial_play_mode_t)(((unsigned)g_play_mode + 1u) % (unsigned)DIAL_PLAY_MODE_COUNT);
    dial_media_set_play_mode(next);
    return g_play_mode;
}

void dial_media_next_track(void) {
    if (g_play_mode == DIAL_PLAY_MODE_REPEAT_ONE && g_has_active_track) {
        dial_media_seek_progress(0.0f);
        dial_hal_send_ipc(DIAL_CMD_SEEK, "0");
        return;
    }

    dial_hal_send_ipc(DIAL_CMD_NEXT_TRACK, "0");
    if (g_playing_from_folder && g_folder_queue_count > 0) {
        if (g_play_mode == DIAL_PLAY_MODE_RANDOM && g_folder_queue_count > 1) {
            size_t step = 1u + (next_rand_u32() % (g_folder_queue_count - 1u));
            g_folder_queue_idx = (g_folder_queue_idx + step) % g_folder_queue_count;
        } else {
            g_folder_queue_idx = (g_folder_queue_idx + 1u) % g_folder_queue_count;
        }
        dial_media_play_file(g_folder_queue[g_folder_queue_idx].filepath, NULL, NULL, NULL);
        return;
    }
    if (g_album_count == 0) return;
    if (g_play_mode == DIAL_PLAY_MODE_RANDOM) {
        size_t alb_idx = next_rand_u32() % g_album_count;
        const dial_album_info_t *ralb = &g_albums[alb_idx];
        size_t tr_idx = (ralb->track_count > 0) ? (next_rand_u32() % ralb->track_count) : 0;
        if (alb_idx == g_now_album_idx && tr_idx == g_now_track_idx && ralb->track_count > 1) {
            tr_idx = (tr_idx + 1u) % ralb->track_count;
        }
        dial_media_play_track(alb_idx, tr_idx);
        return;
    }
    const dial_album_info_t *alb = &g_albums[g_now_album_idx];
    if (g_now_track_idx + 1 < alb->track_count) {
        dial_media_play_track(g_now_album_idx, g_now_track_idx + 1);
    } else {
        dial_media_play_track((g_now_album_idx + 1) % g_album_count, 0);
    }
}

void dial_media_prev_track(void) {
    uint32_t pos_sec = 0;
    dial_media_get_progress(&pos_sec, NULL);
    if (g_has_active_track && pos_sec > DIAL_RESTART_THRESHOLD_S) {
        dial_media_seek_progress(0.0f);
        dial_hal_send_ipc(DIAL_CMD_SEEK, "0");
        return;
    }

    dial_hal_send_ipc(DIAL_CMD_PREV_TRACK, "0");
    if (g_playing_from_folder && g_folder_queue_count > 0) {
        g_folder_queue_idx = (g_folder_queue_idx + g_folder_queue_count - 1) % g_folder_queue_count;
        dial_media_play_file(g_folder_queue[g_folder_queue_idx].filepath, NULL, NULL, NULL);
        return;
    }
    if (g_album_count == 0) return;
    if (g_now_track_idx > 0) {
        dial_media_play_track(g_now_album_idx, g_now_track_idx - 1);
    } else {
        size_t prev_alb = (g_now_album_idx + g_album_count - 1) % g_album_count;
        size_t last_tr = g_albums[prev_alb].track_count > 0 ? (g_albums[prev_alb].track_count - 1) : 0;
        dial_media_play_track(prev_alb, last_tr);
    }
}

const char *dial_media_get_now_title(void)       { return g_has_active_track ? g_now_title : "Not Playing"; }
const char *dial_media_get_now_artist(void)      { return g_has_active_track ? g_now_artist : ""; }
const char *dial_media_get_now_album(void)       { return g_has_active_track ? g_now_album : ""; }
const char *dial_media_get_now_filepath(void)    { return g_has_active_track ? g_now_filepath : ""; }
const char *dial_media_get_now_format_name(void) { return g_has_active_track ? g_now_format : ""; }
uint32_t    dial_media_get_now_file_size(void)   { return g_has_active_track ? g_now_file_size : 0u; }
uint32_t    dial_media_get_now_sample_rate(void) { return g_has_active_track ? g_now_sample_rate : 0u; }
uint32_t    dial_media_get_now_bit_depth(void)   { return g_has_active_track ? g_now_bit_depth : 0u; }
uint32_t    dial_media_get_now_bitrate_kbps(void){ return g_has_active_track ? g_now_bitrate_kbps : 0u; }

size_t dial_media_get_now_queue_index(size_t *out_total) {
    if (!g_has_active_track) {
        if (out_total) *out_total = 0;
        return 0;
    }
    if (g_playing_from_folder && g_folder_queue_count > 0) {
        if (out_total) *out_total = g_folder_queue_count;
        return g_folder_queue_idx + 1;
    }
    if (g_album_count > 0) {
        const dial_album_info_t *alb = &g_albums[g_now_album_idx % g_album_count];
        if (out_total) *out_total = alb->track_count;
        return g_now_track_idx + 1;
    }
    if (out_total) *out_total = 1;
    return 1;
}

const uint16_t *dial_media_get_now_cover_rgb565(void) {
    if (!g_has_active_track) return NULL;
    if (g_now_has_custom_cover) return g_now_custom_cover;
    if (g_album_count > 0 && !g_playing_from_folder) {
        return dial_media_get_album_cover_rgb565(g_now_album_idx);
    }
    return NULL;
}

/* ── Real System & Hardware Telemetry ── */
void dial_sys_get_clock_str(char *out_buf, size_t cap) {
    if (!out_buf || cap == 0) return;
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (lt) {
        snprintf(out_buf, cap, "%02d:%02d", lt->tm_hour, lt->tm_min);
    } else {
        snprintf(out_buf, cap, "--:--");
    }
}

int dial_sys_get_battery_pct(bool *out_charging) {
    if (out_charging) *out_charging = false;

    /* 1. Try Linux / Snowsky DISC sysfs battery capacity & status */
    FILE *cfp = fopen("/sys/class/power_supply/battery/capacity", "r");
    if (cfp) {
        int cap = -1;
        if (fscanf(cfp, "%d", &cap) == 1 && cap >= 0 && cap <= 100) {
            fclose(cfp);
            FILE *sfp = fopen("/sys/class/power_supply/battery/status", "r");
            if (sfp) {
                char st[32] = "";
                if (fgets(st, sizeof(st), sfp) && out_charging) {
                    *out_charging = (strncasecmp(st, "Charging", 8) == 0 ||
                                     strncasecmp(st, "Full", 4) == 0);
                }
                fclose(sfp);
            }
            return cap;
        }
        fclose(cfp);
    }

#if defined(__APPLE__) && !defined(DIAL_TARGET_DISC)
    /* 2. On macOS, query real battery via pmset (cached for 15 seconds) */
    static uint32_t s_last_batt_tick = 0;
    static int      s_cached_pct     = -1;
    static bool     s_cached_chg     = false;
    uint32_t now_ms = dial_hal_ticks_ms();
    if (s_cached_pct < 0 || (now_ms - s_last_batt_tick) > 15000u) {
        s_last_batt_tick = now_ms;
        FILE *pp = popen("pmset -g batt 2>/dev/null", "r");
        if (pp) {
            char line[256];
            while (fgets(line, sizeof(line), pp) != NULL) {
                char *pct_sign = strchr(line, '%');
                if (pct_sign) {
                    char *p = pct_sign;
                    while (p > line && isdigit((unsigned char)p[-1])) p--;
                    if (p < pct_sign) {
                        s_cached_pct = atoi(p);
                        s_cached_chg = (strstr(line, "AC Power") != NULL) ||
                                       (strstr(line, "charging") != NULL &&
                                        strstr(line, "discharging") == NULL);
                    }
                }
            }
            pclose(pp);
        }
        if (s_cached_pct < 0) s_cached_pct = 100; /* Desktop Mac on AC */
    }
    if (out_charging) *out_charging = s_cached_chg;
    return s_cached_pct;
#else
    return 100;
#endif
}

void dial_sys_get_free_space_str(char *out_buf, size_t cap) {
    if (!out_buf || cap == 0) return;
    static char     s_cached_path[512] = "";
    static char     s_cached_str[64]   = "";
    static uint32_t s_last_vfs_tick    = 0;

    const char *path = g_root_path[0] ? g_root_path : ".";
    uint32_t now_ms  = dial_hal_ticks_ms();
    if (s_cached_str[0] == '\0' ||
        strcmp(s_cached_path, path) != 0 ||
        (now_ms - s_last_vfs_tick) > 5000u) {
        strncpy(s_cached_path, path, sizeof(s_cached_path) - 1);
        s_cached_path[sizeof(s_cached_path) - 1] = '\0';
        s_last_vfs_tick = now_ms;

        const char *prefix = (strncmp(path, "/mnt/mmcblk", 11) == 0) ? "microSD" : "Disk";
        struct statvfs vfs;
        if (statvfs(path, &vfs) == 0) {
            uint64_t free_bytes = (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize;
            double free_gb = (double)free_bytes / (1024.0 * 1024.0 * 1024.0);
            if (free_gb >= 10.0) {
                snprintf(s_cached_str, sizeof(s_cached_str), "%s · %.0f GB free", prefix, free_gb);
            } else if (free_gb >= 1.0) {
                snprintf(s_cached_str, sizeof(s_cached_str), "%s · %.1f GB free", prefix, free_gb);
            } else {
                double free_mb = (double)free_bytes / (1024.0 * 1024.0);
                snprintf(s_cached_str, sizeof(s_cached_str), "%s · %.0f MB free", prefix, free_mb);
            }
        } else {
            snprintf(s_cached_str, sizeof(s_cached_str), "%s · Ready", prefix);
        }
    }
    snprintf(out_buf, cap, "%s", s_cached_str);
}
