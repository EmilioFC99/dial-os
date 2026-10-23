# DialOS Roadmap — Functional Snowsky DISC Operating System (Fjäll Theme)

> **Guiding Principle:**
> The Fjäll showcase (`assets/nordic/fjall-design-system.html`) is the **visual theme, layout grammar, and interaction language** — **NOT** a static slide deck.
> Every screen in DialOS must be driven by **real hardware state**, **real filesystem/SD-card audio files**, and **real hierarchical navigation** with a universal **Go Back** gesture.

---

## Foundation Already Completed & Verified on Hardware
- [x] **Fjäll Rendering Engine (`src/ui/`, `src/core/dial_polar.c`):** Sub-pixel AA circles, rings, arcs, tick marks, polar geometry, baked font atlas, 70 vector icons, and Night ("Carbon") / Day ("Warm Paper") palettes.
- [x] **Snowsky DISC Hardware Backend (`src/hal/hal_disc.c`):** Verified on physical Snowsky DISC (V2.57) — `360×360` 180°-rotated `32 bpp XRGB8888` `ingenicfb` display, PWM backlight control, `cst816t` 180°-inverted touch, physical GPIO keys, LRA haptic motor, `mq_ui` watchdog survival, and `/player` & `/ui` POSIX mqueue IPC.
- [x] **macOS Native Installer & Recovery (`./dialos-mac`):** One-command macOS builder, USB Mask-ROM flasher, and `restore-stock` recovery tool.

---

## Active Rebuild Milestones (Real Functional DAP UI)

### Milestone 1 — Purge Demo Data & Build Real Navigation Engine (`nav_stack`)
- [ ] **Remove all 30-slide linear stepping (`screen_id + 1` / `screen_id - 1`):**
  - Replace hardcoded showcase slides with a real hierarchical view stack (`nav_stack`) and modal/overlay layer (`Control Center`, `Quick Actions`, `Volume HUD`, `Action Sheet`, `Lock Screen`).
- [ ] **Universal "Go Back" Gesture:**
  - Left-edge swipe-right (`x < 48px` -> `dx > +45px`) pops the current screen, modal, or folder level back to its parent.
- [ ] **Single Dynamic Home Launcher (`HOM`):**
  - One unified Home view (using the Fjäll `HOM` visual template) where rotating the rim or swiping scrolls through real launcher items (`Now Playing`, `Library`, `Browse Files`, `Operating Mode`, `Settings`, `Games`) and tapping opens the selected app.

### Milestone 2 — Real System Telemetry (`src/hal/hal_sys.c`)
- [ ] **Delete all fake status strings (`"STUDIO_5G"`, `"SONY WH-1000XM5"`, `"84%"`, `"09:41"`):**
  - Read real battery percentage and charging state from `/sys/class/power_supply/battery/capacity` & `status`.
  - Read real system time via `localtime()`.
  - Read/write real display brightness via `/sys/class/backlight/backlight/brightness`.
  - Read real Wi-Fi interface state (`/sys/class/net/wlan0/operstate`, `wpa_cli status`) and real Bluetooth state (`/usr/data/fiio/bt_state`, `bluetoothctl`). Show `Disconnected` / `Off` when nothing is connected.
  - Read real microSD storage capacity (`statvfs("/mnt/mmcblk0p1")`).

### Milestone 3 — Real SD-Card File Browser (`FIL`) & Music Library (`LIB`)
- [ ] **Real Hierarchical File Browser (`/mnt/mmcblk0p1` on device, `~/Music` on Mac sim):**
  - Dynamic directory stack (`Enter Folder` / `Back to Parent Folder`), kinetic touch scrolling & rim dial scrubbing through unlimited files/folders, and real long-press file/folder actions (`Rename`, `Delete` with `600 ms` hold confirmation).
- [ ] **Real Audio Metadata Indexer (`ID3v2` / `FLAC Vorbis` / `/usr/data/fiio/db/`):**
  - Populate `Library` (`Favorites`, `Playlists`, `Artists`, `Albums`, `Genres`, `Tracks`) strictly from real audio files on the user's microSD card (and clean empty states when no SD card or tracks exist).

### Milestone 4 — Real Playback & Bidirectional `mq_player` Engine (`NP`)
- [ ] **Full Bidirectional `/player` & `/ui` POSIX Message Queue Integration:**
  - Send real play/pause, track file path, seek, and hardware volume commands to `/usr/bin/mq_player`, and parse incoming state updates from `/ui` (plus real local audio playback via `AudioToolbox` in the macOS simulator).
- [ ] **Unified `Now Playing` State Machine (Following Fjäll `NP-01..05` Rules):**
  - `NP-01` base state (full-bleed spinning vinyl + `r = 38px` inner seek ring, zero text clutter).
  - Single tap toggles `NP-02` Quick Actions overlay (showing the **real** playing track's floating title/artist/album + Favorite/Shuffle/Playlist/Lyrics pucks).
  - Double-tap toggles Play/Pause; swipe left/right skips next/previous track in the current folder/album queue; swipe up opens `NP-04` real track audio specs; right-rim trace (`r > 150px`) opens `NP-05` Volume HUD.

### Milestone 5 — SD-Card Hot-Reload Loader for Instant Hardware Testing
- [ ] **MicroSD Override Boot Hook (`/mnt/mmcblk0p1/dialos/mq_ui`):**
  - Allow `/usr/bin/mq_ui` to execute `/mnt/mmcblk0p1/dialos/mq_ui` directly from the microSD card (or `/usr/bin/mq_ui.stock` if `/mnt/mmcblk0p1/dialos/use_stock` is present) so new UI builds can be tested on the Snowsky DISC in seconds without 18-minute USB NAND flashes.
