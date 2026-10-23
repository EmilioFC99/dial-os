/**
 * @file dial_media.h
 * @brief Real Audio Playback Engine, Library Scanner & Album Art Decoder
 *
 * Strictly platform-agnostic C interface used by src/core/dial_app.c and
 * src/screens/. Concrete implementation lives inside src/hal/ (CoreAudio +
 * AudioToolbox + ImageIO on macOS Simulator; POSIX /player IPC + VFS on
 * Snowsky DISC).
 */
#ifndef DIAL_MEDIA_H
#define DIAL_MEDIA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dial_assets.h"

#define DIAL_MAX_ALBUMS          96
#define DIAL_MAX_TRACKS_PER_ALB  32
#define DIAL_MAX_DIR_ENTRIES     96

typedef struct {
    char     title[96];
    char     filename[128];
    char     filepath[512];
    uint32_t duration_sec;
    uint32_t file_size_bytes;
    uint8_t  track_num;
} dial_track_info_t;

typedef struct {
    char              name[96];
    char              artist[64];
    char              year[16];
    char              dir_path[512];
    char              cover_path[512];
    dial_track_info_t tracks[DIAL_MAX_TRACKS_PER_ALB];
    size_t            track_count;
} dial_album_info_t;

typedef struct {
    char     name[128];
    char     full_path[512];
    bool     is_dir;
    uint32_t size_bytes;
    uint32_t sub_count;
} dial_fs_entry_t;

/* Scan a real music directory (e.g. /Users/emilio.flores/Music/Au5) */
bool dial_media_scan_root(const char *root_path);
bool dial_media_is_loaded(void);
const char *dial_media_get_root_path(void);

/* Library Queries (LIB-01..04) */
size_t                   dial_media_get_album_count(void);
size_t                   dial_media_get_artist_count(void);
size_t                   dial_media_get_total_track_count(void);
const dial_album_info_t *dial_media_get_album(size_t album_idx);

/* Real File Browser Queries (FIL-01..04) */
bool                   dial_media_browse_dir(const char *dir_path);
bool                   dial_media_browse_parent_dir(void);
bool                   dial_media_is_at_root_dir(void);
const char            *dial_media_get_current_dir_path(void);
const char            *dial_media_get_current_dir_label(void);
size_t                 dial_media_get_fs_entry_count(void);
const dial_fs_entry_t *dial_media_get_fs_entry(size_t idx);
bool                   dial_media_play_fs_entry(size_t idx);
bool                   dial_media_delete_fs_entry(size_t idx);

/* Real Album Cover Texture (180x180 RGB565 decoded from cover.jpg) */
const uint16_t *dial_media_get_album_cover_rgb565(size_t album_idx);

/* Standard DAP Playback Order / Shuffle Modes */
typedef enum {
    DIAL_PLAY_MODE_SEQUENTIAL = 0, /* Sequential: Play in order */
    DIAL_PLAY_MODE_RANDOM     = 1, /* Random / Shuffle across queue/folder */
    DIAL_PLAY_MODE_REPEAT_ALL = 2, /* Repeat All: Loop folder/album */
    DIAL_PLAY_MODE_REPEAT_ONE = 3, /* Repeat 1: Loop current track */
    DIAL_PLAY_MODE_COUNT      = 4
} dial_play_mode_t;

/* Real Hardware/CoreAudio Playback Control (NP-01..05, LCK-02, LIB-03) */
bool             dial_media_has_active_track(void);
bool             dial_media_play_track(size_t album_idx, size_t track_idx);
bool             dial_media_play_file(const char *filepath, const char *title, const char *artist, const char *album);
void             dial_media_set_playing(bool playing);
bool             dial_media_get_playing(void);
void             dial_media_seek_progress(float progress_01);
float            dial_media_get_progress(uint32_t *out_pos_sec, uint32_t *out_dur_sec);
void             dial_media_set_volume(int vol_0_100);
int              dial_media_get_volume(void);
void             dial_media_next_track(void);
void             dial_media_prev_track(void);
void             dial_media_set_play_mode(dial_play_mode_t mode);
dial_play_mode_t dial_media_get_play_mode(void);
dial_play_mode_t dial_media_cycle_play_mode(void);

/* Active Now-Playing Track Metadata */
const char     *dial_media_get_now_title(void);
const char     *dial_media_get_now_artist(void);
const char     *dial_media_get_now_album(void);
const char     *dial_media_get_now_filepath(void);
const char     *dial_media_get_now_format_name(void);
uint32_t        dial_media_get_now_file_size(void);
uint32_t        dial_media_get_now_sample_rate(void);
uint32_t        dial_media_get_now_bit_depth(void);
uint32_t        dial_media_get_now_bitrate_kbps(void);
size_t          dial_media_get_now_queue_index(size_t *out_total);
const uint16_t *dial_media_get_now_cover_rgb565(void);

/* Real System & Hardware Telemetry (Clock, Battery, Storage) */
void dial_sys_get_clock_str(char *out_buf, size_t cap);
int  dial_sys_get_battery_pct(bool *out_charging);
void dial_sys_get_free_space_str(char *out_buf, size_t cap);

#endif /* DIAL_MEDIA_H */
