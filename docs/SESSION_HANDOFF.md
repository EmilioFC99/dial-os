# DialOS Session Handoff — Real Functional DAP Rebuild (No Demo / No Showcase Slides)

> **CRITICAL DIRECTIVE FOR THE NEXT AGENT:**
> The Fjall design sheets (`SYS-00` through `OVL-04`) are **ONLY a visual theme, typography, and component style guide**.
> **DO NOT** build a 30-slide showcase or hardcode any demo strings (`"TYCHO"`, `"BONOBO"`, `"STUDIO_5G"`, `"SONY WH-1000XM5"`, `"84%"`, `"09:41"`).
> Every screen, list, status indicator, and control in DialOS **MUST be 100% real, dynamic, and data-driven**.

---

## 1. What Is Already Built & Verified on Real Hardware (KEEP)

1. **Hardware Backend (`src/hal/hal_disc.c`) — Verified on Physical Snowsky DISC (V2.57):**
   - **Display (`/dev/fb0`, `ingenicfb`):** `360×360` visible (`360×1080` virtual), `32 bpp XRGB8888`, 180°-rotated blit, page 0 pan (`FB_ACTIVATE_NOW`), and PWM backlight wake (`/sys/class/backlight/backlight/brightness`).
   - **Touch (`/dev/input/event1`, `cst816t`):** Multitouch Type-B with 180° coordinate inversion (`359 - x`, `359 - y`).
   - **Physical Buttons (`/dev/input/event0`, `x2000_key`):** `KEY_POWER`, `KEY_PLAYPAUSE`, `KEY_VOLUMEUP`, `KEY_VOLUMEDOWN`.
   - **Watchdog Survival:** Sets process name `mq_ui` (`prctl(PR_SET_NAME, "mq_ui")`) to satisfy `/usr/project/fiio_init.sh`.
   - **Cross-Compilation (`make disc-mipsel`):** Native macOS `zig cc` build producing a static MIPS32r2 FP64A + NaN2008 (`e_flags = 0x70001405`) binary at `macos/vendor/mq_ui`.
2. **macOS Native Installer & Stock Recovery (`./dialos-mac` / `macos/dialos_mac_installer.py`):**
   - `./dialos-mac doctor` — Verifies macOS native tools (`usbboot`, `mksquashfs`, `unsquashfs`, `disc_spl_lpddr3.bin`, `my_write6_dram.bin`, `mq_ui`) and USB Mask-ROM connection (`a108:eaef`).
   - `./dialos-mac build <official_fiio_update.zip>` — Decrypts FiiO's `fo123` chunks natively on macOS, saves a verified stock backup (`~/Library/Application Support/DialOS/backups/stock_V257_nand.bin`), patches `/usr/bin/mq_ui` via `hdiutil` Case-Sensitive APFS, and outputs `build/dialos_nand.bin`.
   - `./dialos-mac flash` & `./dialos-mac restore-stock` — Flashes DialOS or restores 100% stock firmware over USB Mask-ROM.
3. **Fjall Rendering Primitives (`src/ui/dial_draw.c`, `src/ui/dial_icons.c`, `src/ui/dial_assets.c`) & macOS Live Simulator (`src/hal/hal_sdl2.c`):**
   - Sub-pixel AA circles, arcs, rings, tick marks, polar math (`src/core/dial_polar.c`), baked Fjall font atlas, and Day ("Warm Paper") / Night ("Carbon") palettes.

---

## 2. What Failed Hardware UX Testing & Must Be Replaced Immediately

During physical hardware testing on the Snowsky DISC, the current `src/screens/*` and `src/core/dial_app.c` failed usability because they were built as a **1:1 static showcase of the 30 spec-sheet images**:
- Hardcoded fake music (`Tycho`, `Bonobo`, `Boards of Canada`, `Nils Frahm`) instead of real SD card music.
- Hardcoded fake connectivity (`Studio 5G` Wi-Fi, `Sony WH-1000XM5` Bluetooth) and fake status (`84%` battery, `09:41` clock).
- Linear slide-deck navigation (`screen_id + 1` / `screen_id - 1` cycling through 30 enum values) with **no hierarchical navigation stack** and **no "Go Back" gesture**.

---

## 3. Architecture Plan for the New Session (Real Functional DAP OS)

### Step 1: Purge All Fake Data & Replace the 30-Slide Enum with a Real Navigation Stack
- Delete the 30 showcase slide screens in `src/screens/*` and replace them with **6 real, stateful views** styled with the Fjall theme:
  1. **`VIEW_HOME` (Circular Launcher):** Clean Fjall dial launcher to enter **Now Playing**, **Files (SD Card)**, **Music Library**, **Sound & EQ**, **Bluetooth / Wi-Fi**, and **Settings**.
  2. **`VIEW_FILES` (Real Directory & File Browser):**
     - Rooted at `/mnt/mmcblk0p1` on Snowsky DISC (and a real local folder like `~/Music` on the macOS simulator).
     - Full scrollable list (kinetic touch drag + rim dial scrubbing) for any number of files/folders.
     - Tap folder -> push folder onto path stack; **Swipe Right from left edge (or tap `<` Back)** -> pop to parent folder or back to `VIEW_HOME`.
     - Tap audio file -> start real playback and push `VIEW_NOW_PLAYING`.
  3. **`VIEW_LIBRARY` (Real Indexed Music Library):**
     - Populated strictly from real audio files (ID3v2 / FLAC Vorbis comment parser + FiiO's `/usr/data/fiio/db/` SQLite database on device).
     - Browse by **Artists -> Albums -> Tracks** or **All Songs**.
     - If no SD card or music is present, render a clean Fjall **"No Music Found on microSD"** empty state.
  4. **`VIEW_NOW_PLAYING` (Real Playback Engine):**
     - Shows real current track title, artist, album, format badge (`FLAC / MP3 / WAV`, real sample rate & bit depth), real elapsed/total time, and real volume.
     - Tap center/mode button to switch visual layout (Vinyl view, Minimal Typography view, Track Info) without changing screens.
     - Rim dial scrub seeks within the track or adjusts volume; Transport buttons (`Play/Pause`, `Next`, `Prev`) control real playback.
  5. **`VIEW_SOUND` (Real DAC & Audio Settings):**
     - Real 5-band Parametric EQ, CS43131 digital filter selector, High/Low Gain (`0 dB` / `+6 dB`), and L/R balance.
  6. **`VIEW_SETTINGS` & Global Overlays (`OVERLAY_CONTROL_CENTER`, `OVERLAY_VOLUME`, `OVERLAY_LOCK`):**
     - **Real System Telemetry (`src/hal/hal_sys.c`):**
       - Battery: `/sys/class/power_supply/battery/capacity` & `status`.
       - Clock: `localtime()`.
       - Brightness: Read/write `/sys/class/backlight/backlight/brightness` (`1..255`).
       - Wi-Fi: Read `/sys/class/net/wlan0/operstate` and `wpa_cli status` (show `Disconnected` or `Off` when not connected).
       - Bluetooth: Read real `/usr/data/fiio/bt_state` / `bluetoothctl` (show `Not Connected` when nothing is paired/connected).
       - Storage: `statvfs("/mnt/mmcblk0p1")` for real used/total GB.

### Step 2: Universal Gestures & Hardware Button Behavior
- **Swipe Right from Left Edge (`x < 48` -> `dx > +45`):** Universal **Go Back** gesture across the entire OS (pops `nav_stack` or steps up one directory level).
- **Swipe Down from Top Edge (`y < 48` -> `dy > +45`):** Pulls down the **Control Center** overlay from any screen; **Swipe Up** dismisses it.
- **Rim Dial Scrubbing (`r ∈ [132, 180] px`):** Scrolls lists in `VIEW_FILES` / `VIEW_LIBRARY` / `VIEW_SETTINGS`, or scrubs volume/seek in `VIEW_NOW_PLAYING`, with LRA haptic detents.
- **Global Physical Buttons:**
  - `Vol+` / `Vol-`: Adjusts hardware volume on any screen and triggers a 1.5-second transient Volume Ring overlay.
  - `Play/Pause`: Toggles playback from any screen.
  - `Power` short press: Toggles screen lock + turns PWM backlight off (`0`) / on (`180`).

### Step 3: SD-Card Hot-Reload Hook (Zero 18-Minute Re-Flashes)
- In `src/main.c` on `DIAL_TARGET_DISC`, check on startup:
  1. If `/mnt/mmcblk0p1/dialos/use_stock` exists -> `execv("/usr/bin/mq_ui.stock", argv)`.
  2. If `/mnt/mmcblk0p1/dialos/mq_ui` exists -> copy to `/tmp/mq_ui.sd`, `chmod +x`, and `execv("/tmp/mq_ui.sd", argv)`.
- Once flashed once, any new `mq_ui` build can be tested on the physical Snowsky DISC in seconds simply by copying `mq_ui` onto the microSD card!
