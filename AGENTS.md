# DialOS — Single Source of Truth (Fjäll Design System & Snowsky DISC Hardware Guide)

> **CRITICAL DIRECTIVE — REAL FUNCTIONAL DAP UI (ZERO DEMO / ZERO SLIDES):**
> We are building a **100% functional, data-driven, responsive Digital Audio Player (DAP) UI** for the **FiiO Snowsky DISC** (`360×360` round touchscreen).
> - `assets/nordic/fjall-design-system.html` is **strictly the visual theme, layout grammar, typography, colour palette, and component style guide**.
> - **DO NOT** build a linear slide-deck showcase (`screen_id + 1` / `screen_id - 1`).
> - **DO NOT** hardcode fake songs, artists, albums, Wi-Fi SSIDs (`"Studio 5G"`), Bluetooth headphones (`"WH-1000XM5"`), battery percentages (`"84%"`), or clocks (`"09:41"`).
> - Every list, status bar, and screen must be driven by **real filesystem/SD-card audio files**, **real hardware/sysfs telemetry**, and a **real hierarchical navigation stack (`nav_stack`)** with a universal **Left-Edge Swipe-Right ("Go Back")** gesture.

---

## 1. What Is Already Built & Verified in This Repository (Do Not Recreate)

1. **Fjäll Design System Assets (`assets/nordic/`):**
   - `assets/nordic/fjall-design-system.html` — Interactive visual specification & layout templates.
   - `assets/nordic/fjall-icons.svg` — 70 custom vector icons (`24×24` grid, `1.75px` stroke, round caps).
   - `assets/nordic/tokens.json` & `include/dial_theme.h` — Night ("Carbon") and Day ("Warm Paper") colour palettes and geometry tokens.
2. **Low-Level 2D Rendering, Font & Vector Icon Engine (`src/ui/`, `src/core/dial_polar.c`):**
   - `src/ui/dial_draw.c` (`include/dial_draw.h`): Sub-pixel anti-aliased circles, rings, arcs, rounded rectangles, rim lists, scroll arcs, and 8-bit alpha text rendering.
   - `src/ui/dial_icons.c` (`include/dial_icons.h`): All 70 Fjäll vector icons compiled into C stroke tables (`dial_draw_icon_by_name`).
   - `src/ui/dial_assets.c` (`include/dial_assets.h`): Pre-baked CoreText proportional font atlas (`10px..44px` with Nordic UTF-8 support).
   - `src/core/dial_polar.c` & `src/core/dial_gestures.c`: Polar coordinate math ($r, \theta$), rim detent tracker, and touch gesture disambiguator.
3. **Dual-Target Hardware Abstraction Layer (`src/hal/`):**
   - **macOS Live Simulator (`src/hal/hal_sdl2.c` & `src/hal/hal_media.c`):** Native macOS Cocoa `360×360` window + `AudioToolbox` real audio streaming (`.flac`, `.mp3`, `.wav`, `.m4a` playback on Mac) + `ImageIO` embedded cover art extractor + real directory scanner (`opendir`/`readdir`).
   - **FiiO Snowsky DISC Hardware Backend (`src/hal/hal_disc.c`):** Verified on physical Snowsky DISC hardware (see Section 2 below).
4. **macOS Native Firmware Builder & USB Mask-ROM Flasher (`./dialos-mac` & `macos/`):**
   - `./dialos-mac doctor` — Verifies macOS native tools (`usbboot`, `mksquashfs`, `unsquashfs`, `disc_spl_lpddr3.bin`, `my_write6_dram.bin`, `mq_ui`) and USB Mask-ROM connection (`a108:eaef`).
   - `./dialos-mac build <official_fiio_update.zip>` — Decrypts FiiO's `fo123` AES-256-CBC firmware chunks natively on macOS, saves a verified stock backup (`~/Library/Application Support/DialOS/backups/`), patches `/usr/bin/mq_ui`, and builds `build/dialos_nand.bin`.
   - `./dialos-mac flash` & `./dialos-mac restore-stock` — Flashes DialOS or restores 100% stock firmware over USB Mask-ROM.

---

## 2. FiiO Snowsky DISC Hardware Reverse-Engineering Specifications (Verified on V2.57)

1. **SoC & Toolchain (`make disc-mipsel`):**
   - **CPU:** Ingenic X2000 (MIPS32r2 Little-Endian, XBurst2 dual-core, 64-bit FPU with hardware IEEE 754-2008 NaN).
   - **Required ELF ABI (`e_flags = 0x70001405`):** Every binary on FiiO's rootfs uses O32 FP64A + `EF_MIPS_NAN2008` (`0x400`). Cross-compile on macOS via `make disc-mipsel` (`zig cc -target mipsel-linux-musleabihf -mcpu=mips32r2 -mfp64 -mno-odd-spreg -mnan=2008 -static -ffp-contract=off -O2 -s` + setting `0x400` in ELF `e_flags`).
2. **Display (`/dev/fb0`, `ingenicfb`):**
   - `360×360` visible resolution (`360×1080` triple-buffered virtual height), **`32 bpp XRGB8888`**.
   - **Physical Panel Rotation:** Mounted **180° upside-down** in the chassis. Every frame blit must write `dst[(359 - y) * stride + (359 - x)] = src[y * 360 + x]`.
   - **Init Sequence:** Clear all 3 pages (`memset`), pan to `yoffset = 0` with `FB_ACTIVATE_NOW` (`FBIOPAN_DISPLAY`), and turn on PWM backlight (`write "180\n"` to `/sys/bus/platform/drivers/pwm-backlight/backlight/backlight/backlight/brightness` or `/sys/class/backlight/backlight/brightness`).
3. **Capacitive Touch (`/dev/input/event1`, `cst816t`):**
   - Emits **Linux Multitouch Type-B** (`ABS_MT_TRACKING_ID`, `ABS_MT_POSITION_X`, `ABS_MT_POSITION_Y`), NOT single-touch `ABS_X/Y`.
   - **180° Coordinate Inversion:** Map raw coordinates via `x = 359 - raw_x`, `y = 359 - raw_y`.
4. **Physical Side Buttons (`/dev/input/event0`, `x2000_key`):**
   - `KEY_POWER` (`116`), `KEY_PLAYPAUSE` (`164`), `KEY_VOLUMEUP` (`115`), `KEY_VOLUMEDOWN` (`114`).
5. **Stock Watchdog (`/usr/project/fiio_init.sh`) & Boot Order:**
   - On boot, `/usr/project/fiio_init.sh` runs `mq_ui &`, sleeps 2 seconds, then runs `mq_player &`, and loops every 5 seconds checking `pgrep -x mq_ui`.
   - Our binary replaces `/usr/bin/mq_ui`, calls `prctl(PR_SET_NAME, "mq_ui")`, creates the `/ui` POSIX mqueue upfront (`O_CREAT | O_RDONLY | O_NONBLOCK`), and lazily opens `/player` (`O_WRONLY | O_NONBLOCK`) once `mq_player` starts 2 seconds later.
   - **SD-Card Hot-Reload Hook (`src/main.c`):** On boot, if `/mnt/mmcblk0p1/dialos/use_stock` exists, `mq_ui` execs `/usr/bin/mq_ui.stock`. If `/mnt/mmcblk0p1/dialos/mq_ui` exists, it copies it to `/tmp/mq_ui.sd` and execs it — allowing instant SD-card testing without 18-minute USB NAND flashes.
6. **Audio Daemon IPC (`/player` & `/ui` POSIX Message Queues):**
   - Wire frame format (`dial_ipc_format_frame` in `include/dial_ipc.h`): ASCII `TAG(4) + TOTAL_LEN(4 hex) + VALUE1(4 hex) [+ payload]`:
     - Play: `"0101000C0001"` | Pause: `"0201000C0000"` | Resume: `"0201000C0001"`
     - Next: `"0647000C0000"` | Prev: `"0649000C0000"`
     - Volume (`0..100` mapped to `0..120` / `0x00..0x78`): `"0715000C%04X"`
     - Seek (`ms`): `"01030010%08X"` | Get State Snapshot: `"02020008"`
     - Play File Path: `"0602%04X0000%s"`
7. **Real Device Paths on Snowsky DISC:**
   - **microSD Card Mount:** `/mnt/mmcblk0p1`
   - **FiiO SQLite Music DB:** `/usr/data/fiio/db/`
   - **Battery:** `/sys/class/power_supply/battery/capacity` (`0..100`) & `/sys/class/power_supply/battery/status`
   - **Backlight Brightness:** `/sys/class/backlight/backlight/brightness` (`0..255`)
   - **LRA Haptic Motor:** `/sys/class/leds/vibrator/duration` & `/sys/class/leds/vibrator/activate`
   - **Wi-Fi / Bluetooth:** `/sys/class/net/wlan0/operstate`, `/usr/data/fiio/bt_state`

---

## 3. Fjäll Design & UX Rules (Visual & Interaction Grammar)

1. **Strict Token Discipline:**
   - Always use `dial_palette_t` (`DIAL_PALETTE_NIGHT` / `DIAL_PALETTE_DAY`) and `include/dial_theme.h`.
   - `moss` (`#8DB596` Night / `#47755A` Day) = primary interactive accent (focus rings, active toggles, track progress).
   - `fjord` = connectivity (`Wi-Fi`, `Bluetooth`).
   - `cloudberry` = warnings, high gain (`+6 dB`), game scores.
   - `lingon` = destructive actions and errors only.
   - `birch` = brightness and warm neutral highlights.
2. **Real Hierarchical Navigation & Universal "Go Back" Gesture:**
   - Maintain a real navigation stack (`nav_stack`) of active views and directory paths.
   - **Swipe Right from Left Edge (`x < 48px`, `dx > +45px`)** or tapping the 9 o'clock `<` back chevron **pops the current view or steps up to the parent directory** from anywhere in the OS.
   - **Swipe Down from Top Rim (`y < 48px`, `dy > +45px`)** pulls down the **Control Center** overlay from any screen; **Swipe Up** dismisses it.
3. **Core Views to Build Step-by-Step:**
   - **Home Launcher:** Circular pebble carousel (`Now Playing`, `Library`, `Files`, `Modes`, `Settings`, `Arcade`) with live dynamic subtitles (real playing track or `"Not Playing"`, real track/artist count, real free microSD space).
   - **Files (Real Directory Browser):** Browse real folders and audio files rooted at `/mnt/mmcblk0p1` (or `--music <dir>` on Mac), with rim/touch scrolling, folder drill-down, long-press file action sheet (`FIL-02`), and `600 ms` hold-to-delete (`FIL-03`).
   - **Now Playing:** Full-bleed `360×360` spinning vinyl art (`NP-01`) with the `r = 38px` inner seek ring (`0°..359°`), zero text clutter on `NP-01`, single-tap to toggle `NP-02` Quick Actions (floating real song title/artist/album + Favorite/Shuffle/Playlist/Lyrics pucks), double-tap to toggle Play/Pause, swipe left/right for Prev/Next track in the active folder/album queue, swipe up for `NP-04` real audio specs, and right-rim trace (`r > 150px`) for `NP-05` Volume HUD.
   - **Library:** Strict 5-row hub (`Favorites`, `Playlists`, `Artists`, `Albums`, `Genres`), Cover Arc (`LIB-02`), and Track List (`LIB-03`) populated strictly from real scanned audio files (showing a clean empty state if no music is present).
   - **Control Center (`CC-01`) & Settings (`SET` / `MOD`):** `3×2` icon-only `66px` pucks (`wifi`, `bluetooth`, `mode_*`, `gain`, `usb_charge`, `lock`) and settings submenus wired to real hardware state and sysfs controls.
