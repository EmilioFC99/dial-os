# DialOS — Engineering & Release Roadmap (`v0.1.0` → `v1.0.0`)

Every milestone below ships with **atomic Conventional Commits**, **100% unit + headless PNG snapshot + live interactive UI state verification** (`AddressSanitizer` + `UBSan`, $\le 24.0\text{ MB}$ RAM), a **native 60 FPS interactive desktop emulator (`make sim && ./build/dialos-sim`)**, and an annotated **Semantic Version git tag**.

---

## Local Interactive Emulator & Testing Environment Setup

### 1. Zero-Dependency Build & Live Interactive Window (macOS / Linux)
- **macOS (Out of the Box):** Uses the system Apple Clang compiler and native macOS `Cocoa` + `CoreGraphics` frameworks (`-framework Cocoa -framework CoreGraphics -lz`). No third-party packages are required to open the live `360×360` interactive simulator window.
- **Optional SDL2 Backend (macOS / Linux):** If `SDL2` is installed (`brew install sdl2` or `apt install libsdl2-dev`), `Makefile` also supports `-DDIAL_HAS_SDL2`.

### 2. Running the Live 60 FPS Interactive C Emulator
```sh
make sim
./build/dialos-sim
```
- **Mouse / Touch Controls in the `360×360` Window:**
  - **Single-click `NP-01`:** Opens/closes `NP-02` (Quick Actions). Click any of the 4 bottom-arc pucks (`heart`, `shuffle`, `playlist`, `lyrics`) to toggle state or navigate.
  - **Double-click (`< 260 ms`) `NP-01`:** Toggles Play / Pause (`33.33 RPM` vinyl rotation).
  - **Drag inner vinyl ring (`24px <= r <= 54px`) OR Hold `300 ms` on vinyl & rotate:** Scrubs track position (`0°–359°` `DIAL_VINYL_SEEK_R = 38px`) in real time.
  - **Trace outer rim (`r >= 150px`):** Adjusts Volume (`NP-05` HUD on Now Playing) or steps through Fisheye Rim Lists / Home Carousel (`15°` detents, `3×` fast-spin).
  - **Pull down from top edge (`y < 40px`):** Opens `CC-01` (Control Center). Click any `66px` puck to toggle `Wi-Fi` (`fjord`), `Bluetooth` (`fjord`), `Mode` (`moss`), `Gain Low/High` (`cloudberry`), `USB Charge` (`moss`), or `Touch Lock` (`CC-02`, hold center `600 ms` to unlock).
  - **Swipe up from bottom (`y > 320px`):** Unlocks `LCK-01/02` to `HOM-01` or opens `NP-04` (Track Details) from `NP-01`.
- **Keyboard Shortcuts (Hardware Button & Debug Emulation):**
  - `Space`: Hardware `KEY_PLAYPAUSE` (Play / Pause)
  - `Up` / `Down`: Hardware `KEY_VOLUP` / `KEY_VOLDOWN` (`±2` volume steps + Volume HUD)
  - `Escape`: Hardware `KEY_POWER` (Lock / Wake screen)
  - `Left` / `Right` (or `[` / `]`): Cycle through all registered screen IDs for live inspection
  - `D`: Toggle between **Polar Night** (`DIAL_MODE_NIGHT`) and **Birch Day** (`DIAL_MODE_DAY`) palettes
  - `S`: Save a live PNG screenshot of the current window to `/tmp/dialos_live.png`

### 3. Mandatory Pre-Commit Verification Loop (`make test`)
Before any C code is committed, `make test` MUST pass all four gates under `AddressSanitizer` + `UndefinedBehaviorSanitizer`:
1. **`./build/test_polar_gestures`**: Polar math (`0°–359°`) and Gesture FSM unit + 1,000-step fuzz suite.
2. **`./build/test_draw_primitives`**: 2D AA primitives, all 70 Fjäll vector icons, `.train` marquee, and `<= 24.0 MB` RAM budget check.
3. **`./build/test_screens_*`**: Headless render & locked UX pixel-invariant verification across Night and Day modes.
4. **`./build/test_ui_interaction`**: End-to-end interactive UI simulation driving real touch/drag/tap/hold sequences into the live app controller (`dial_app_t`) and verifying that user interactions mutate both app state and rendered framebuffer pixels.

---

## Milestone Summary

| Tag | Status | Milestone | Scope |
| :--- | :--- | :--- | :--- |
| **`v0.1.0`** | **Done** | **Fjäll Design System & Architecture Foundation** | 26-slide interactive spec (`38` screens), `70` custom vector icons, C/JSON tokens, hardware SRS, workspace rules & skills, GitHub Pages showcase. |
| **`v0.2.0`** | **Done** | **Core C Engine, Dual-HAL & Test Harness** | `Makefile`, headless PNG snapshot runner, macOS SDL2 simulator HAL, polar math & gesture FSM (`src/core/`), 2D anti-aliased circular renderer + 70-icon rasterizer (`src/ui/`), and unit test suite (`tests/`). |
| **`v0.3.0`** | **Done** | **Core System, Home & Now Playing (`SYS`, `LCK`, `HOM`, `CC`, `NP`)** | `SYS-00`, `LCK-01..02` (full-bleed lock), `HOM-01..03`, `CC-01..02` (`3×2` icon-only pucks), and `NP-01..05` (full-bleed spinning vinyl, `r=38px` `0°–359°` inner seek ring, `NP-02` floating metadata + 4-puck bottom arc, `NP-05` right-arc Volume HUD). |
| **`v0.4.0`** | **Next** | **Music Library & Browse Files (`LIB`, `FIL`)** | `LIB-01..04` (`Favorites` → `Playlists` → `Artists` → `Albums` → `Genres`, `LIB-02` cover arc with swipe + rim scrub, `LIB-03` animated EQ bars) and `FIL-01..04` (metadata tag + filename editor, recursive folder move/delete + `600 ms` hold confirm). |
| **`v0.5.0`** | Planned | **Modes, Settings, Fast-Cache BT & Arcade (`MOD`, `SET`, `GAM`, `OVL`)** | `MOD-00..06` (`.train` marquee), `SET-01..07` (CS43131 filters, 10-band PEQ, `< 400 ms` Fast-Cache Bluetooth `dial_bt_peer_cache_t` + `LDAC 990` → `SBC` ladder), `GAM-01..04` (Dial Pong, 2048 Wheel, Boomshine), and `OVL-01..04`. |
| **`v1.0.0`** | Planned | **Snowsky DISC Hardware Target (`hal_disc.c`) & `.fw` Packaging** | Ingenic X2000 MIPS32 cross-compile, `/dev/fb0` DMA double-buffering, `/dev/input/event*`, `/player` & `/ui` POSIX mqueues (verified against `b0hemia/diskos`), CS43131 anti-pop relay sequencing, and flashable `.fw` update generator. |

---

## Detailed Milestone Breakdown

### Milestone 1 — `v0.2.0`: Core C Engine, Dual-HAL & Test Harness
1. **Build & Sanitizer Harness (`Makefile`):**
   - Targets: `make sim` (macOS SDL2 interactive window), `make test` (unit + headless snapshot suite with `-fsanitize=address,undefined`), `make clean`.
   - CLI flags on simulator binary: `--headless --screen <ID> --mode <night|day> --out <path.png>`.
2. **Polar Math & Gesture Disambiguation FSM (`src/core/dial_polar.c`, `src/core/dial_gestures.c`):**
   - Convert `(x, y)` to `(r, deg)` clockwise from 12 o'clock (`0° = top`, `90° = right`, `180° = bottom`, `270° = left`).
   - **Inner Vinyl Seek Ring (`NP-01`):**
     - Direct ring slide (`24px <= r <= 54px`) with radial capture once grabbed.
     - Turntable scrub mode (`300 ms` hold anywhere on vinyl `r < 145px` + rotate).
     - Maps angle $\theta \in [0^\circ, 360^\circ)$ to track progress $[0\%, 100\%)$ and emits `DIAL_CMD_SEEK` (`"0103"`).
   - **Tap & Edge Disambiguation:**
     - `1 touch` on `NP-01` toggles `NP-02` (Quick Actions); `2 touches` (`< 260 ms`) toggles Play/Pause (`DIAL_CMD_PLAY_PAUSE`).
     - Top pull-down (`y < 40px`) opens `CC-01`; bottom swipe-up (`y > 320px`) opens `NP-04` or unlocks `LCK-01/02`.
     - Outer rim (`r >= 150px`) fires `15°` detents (`3×` step when angular velocity `> 180°/s`).
3. **2D Circular Drawing Engine (`src/ui/dial_draw.c`, `src/ui/dial_icons.c`):**
   - Anti-aliased thick arcs (`rimArc`, `scrollRim`, `r = 38px` inner progress ring).
   - Rounded rectangles, pills, chips, toggles, `72%` scrim blur/dim, circular clipping mask (`r = 180px`), and `.train` horizontal marquee text.
   - Vector icon renderer for all `70` Fjäll icons (`24×24` grid, `1.75px` stroke).
4. **Tests (`tests/test_polar_gestures.c`, `tests/test_draw_primitives.c`):**
   - 100% unit test coverage of polar angle math, gesture state transitions, and memory bounds.

---

### Milestone 2 — `v0.3.0`: System, Home & Now Playing (`SYS`, `LCK`, `HOM`, `CC`, `NP`)
- **Screens Implemented & Snapshot-Tested (`12` screens):**
  - `SYS-00` (Boot breathing mark)
  - `LCK-01` (Idle mountain horizon clock), `LCK-02` (Full-bleed spinning cover + floating metadata + 3 transport buttons)
  - `HOM-01`, `HOM-02` (Home pebble carousel), `HOM-03` (6-app wheel at `60°` intervals)
  - `CC-01` (Icon-only `3×2` `66px` pucks: `wifi`, `bluetooth`, `mode_*`, `gain_low`/`gain_high`, `usb_charge`, `lock`; tap toggles, `450 ms` hold opens submenu), `CC-02` (`600 ms` hold unlock shield)
  - `NP-01` (Pure full-bleed vinyl + `r = 38px` inner seek ring + bottom `^` chevron), `NP-02` (Floating transparent Song/Artist/Album + 4-puck bottom arc: `heart`, `shuffle`, `playlist`, `lyrics`), `NP-03` (Synced `.lrc` lyrics), `NP-04` (Read-only track details sheet), `NP-05` (Right-arc-only Volume HUD)

---

### Milestone 3 — `v0.4.0`: Music Library & Browse Files (`LIB`, `FIL`)
- **Screens Implemented & Snapshot-Tested (`8` screens):**
  - `LIB-01` (`Favorites`, `Playlists`, `Artists`, `Albums`, `Genres` Fisheye Rim List)
  - `LIB-02` (Albums cover arc `132 / 84 / 56 px` with both horizontal swipe and right-rim scroll)
  - `LIB-03` (Track list with animated `moss` equalizer bars + `Playing`, no codec clutter)
  - `LIB-04` (Artists A–Z fast-spin letter jump bubble)
  - `FIL-01` (microSD directory browser)
  - `FIL-02` (Action sheet: **Edit tags & filename** for files; **Rename** or **recursive Move / Delete** for folders)
  - `FIL-03` (`600 ms` hold-to-delete confirmation in `lingon`)
  - `FIL-04` (Empty folder state with shortcut to File Transfer mode)

---

### Milestone 4 — `v0.5.0`: Modes, Settings, Fast-Cache BT & Arcade (`MOD`, `SET`, `GAM`, `OVL`)
- **Screens Implemented & Snapshot-Tested (`18` screens):**
  - `MOD-00` (6-mode ring picker) & `MOD-01..06` (`Player`, `USB DAC` with `.train` marquee on `"Draw power from host"`, `Transport`, `Bluetooth RX`, `AirPlay`, `File Transfer`)
  - `SET-01..06` (`Settings Hub`, `Audio`, `CS43131 DAC Filter Picker`, `10-Band Parametric EQ`, `Display Night/Day/Accent`, `Wi-Fi`)
  - `SET-07` (**Fast-Cache Bluetooth** `src/core/dial_bt.c` + `tests/test_bt_cache.c`: MRU `dial_bt_peer_cache_t` with index `0` as `Default`, `< 400 ms` Direct Page before inquiry scan, cached `remote_seid`, `LDAC 990` → `660/330` → `aptX HD` → `aptX` → `AAC` → `SBC` ladder, per-MAC saved volume & codec)
  - `GAM-01..04` (`Arcade Hub`, `Dial Pong`, `2048 Wheel`, `Boomshine`)
  - `OVL-01..04` (`Toast`, `Connection Toast`, `Low Battery Dialog`, `Power / Reboot Sheet`)

---

### Milestone 5 — `v1.0.0`: Snowsky DISC Hardware Backend (`src/hal/hal_disc.c`)
- Ingenic X2000 MIPS32 cross-compilation (`mipsel-linux-gnu-gcc`).
- `/dev/fb0` `mmap` double-buffered driver + `/dev/input/event0` & `event1` reader.
- POSIX `/player` and `/ui` message queue bridge verified against `b0hemia/diskos`.
- CS43131 anti-pop digital mute sequencer around `GPIO_PD14` analogue gain relay transitions.
- Automated `.fw` firmware update package builder and hardware smoke test suite.
