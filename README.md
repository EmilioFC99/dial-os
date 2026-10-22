# DialOS — Fjäll Edition

> **Calm sound, natural interface.**  
> An open-source, tactile, rotary-driven operating system for the **FiiO Snowsky DISC** (`1.8"` `360×360` circular touch display, Ingenic X2000 MIPS32, dual Cirrus Logic CS43131 DACs).

---

## Quick Start: Live Interactive C Emulator & 4-Tier Test Suite

DialOS compiles out-of-the-box on macOS (zero external dependencies required — uses native macOS `Cocoa`/`CoreGraphics` or `SDL2` if installed) and Linux (`SDL2`).

### 1. Run the Live Interactive `360×360` Desktop Window (`60 FPS`)
```bash
# Build the native C simulator
make sim

# Launch the live interactive 360x360 window (starts on NP-01 Now Playing)
./build/dialos-sim

# Or launch directly into any screen in Polar Night or Birch Day mode:
./build/dialos-sim --screen HOM-01
./build/dialos-sim --screen CC-01 --day
```

**Mouse & Touch Gestures in the Live Window:**
* **`NP-01` Inner Vinyl Seek Ring (`24px ≤ r ≤ 54px`):** Click and drag around the `r = 38px` center vinyl border (with radial capture) to scrub through the song, or hold `300 ms` anywhere on the vinyl (`r < 145px`) to enter Turntable Scrub mode.
* **`NP-01` Center Tap (`r < 145px`):** Single-click opens `NP-02` Quick Actions (`heart`, `shuffle`, `playlist`, `lyrics` pucks at `234°`, `198°`, `162°`, `126°`); double-click (`< 260 ms`) toggles Play/Pause (`"0201"`).
* **Outer Rim (`150px ≤ r ≤ 180px`):** Drag clockwise/counter-clockwise to scroll lists (`HOM-01`, `NP-03` lyrics) with `15°` haptic detents, or adjust volume on the right rim (`NP-05`).
* **Top Edge Pull-Down (`y < 40px`):** Pull down from the top rim on any screen to open `CC-01` Control Center (`66px` icon-only `3×2` pucks); hold `600 ms` on `CC-02` to unlock Touch Lock.
* **Bottom Handle Swipe-Up (`y > 320px`):** Swipe up to wake from `LCK-01`/`LCK-02`, open `NP-04` Track Details from `NP-01`, or return home.

**Keyboard Shortcuts in the Live Window:**
| Key | Action |
| :--- | :--- |
| `Space` | Toggle Play / Pause (`DIAL_CMD_PLAY_PAUSE`) |
| `Up` / `Down` | Step Volume `±2%` and trigger `NP-05` Volume HUD |
| `Left` / `Right` or `[` / `]` | Cycle through all implemented screens (`SYS-00` → `NP-05`) |
| `D` | Toggle **Polar Night** (`#121615`) ↔ **Birch Day** (`#F4F1EA`) live |
| `Esc` | Toggle Lock Screen (`LCK-02` / `LCK-01`) ↔ Home (`HOM-01`) |
| `S` | Save current `360×360` frame to `build/screenshots/live_<SCREEN>.png` |
| `Q` | Quit simulator |

### 2. Run the Mandatory 4-Tier Pre-Commit Verification Loop
Every commit touching C code must pass all 4 automated verification tiers under `AddressSanitizer` + `UndefinedBehaviorSanitizer`:
```bash
make test
```
1. **Tier 1 (`./build/test_polar_gestures`):** 12-o'clock polar math (`0°=top, 90°=right, 180°=bottom, 270°=left`), `r = 38px` seek ring mapping, and Gesture FSM disambiguation.
2. **Tier 2 (`./build/test_draw_primitives`):** Anti-aliased SDF arcs, full-bleed vinyl art, 70 Fjäll vector icons, and `.train` horizontal marquee.
3. **Tier 3 (`./build/test_screens_v030`):** Pixel-level headless framebuffer invariant assertions across Night & Day palettes (`<= 24.0 MB` static RAM check).
4. **Tier 4 (`./build/test_ui_interaction`):** End-to-end synthetic mouse/touch interaction through `dial_app_t` + `dial_gesture_fsm_t` + `dial_screens_render` verifying that real user gestures mutate live application state and update rendered pixels on screen.

---

## Live Interactive HTML Design Sheet (Browser Reference)

Explore the complete **26-slide Fjäll Design System** and all **38 Master Screens** directly in your browser (with live switching between **Polar Night** and **Birch Day** modes):

* **Local Interactive Deck:** Open [`assets/nordic/fjall-design-system.html`](assets/nordic/fjall-design-system.html)
* **GitHub Pages Showcase:** Automatically deployed on every push to `main` and tagged release (`v*`) via [`.github/workflows/pages.yml`](.github/workflows/pages.yml)

| Theme Mode | Surface Background | Primary Text | Primary Accent (`Moss`) | Connectivity (`Fjord`) | Caution / High Gain (`Cloudberry`) | Destructive (`Lingon`) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Polar Night** | `#121615` *(Pine Night)* | `#ECEEE8` *(Snow)* | `#8DB596` | `#8FB3C9` | `#E2A66A` | `#E07F76` |
| **Birch Day** | `#F4F1EA` *(Warm Snow)* | `#1C2220` *(Charcoal)* | `#47755A` | `#3B6A86` | `#A9652B` | `#AE463D` |

---

## Screen & Interaction Tour (38 Screens Across 6 Core Apps)

DialOS replaces rectangular smartphone lists with a **concentric 3-zone circular architecture** (`Core: r < 60px`, `Stage: 60 ≤ r < 150px`, `Rim: 150 ≤ r ≤ 180px`) so your thumb never blocks what you are reading.

### 1. Now Playing (`NP-01` – `NP-05`) & Lock (`LCK-01` – `LCK-02`)
* **100% Full-Bleed Spinning Vinyl (`NP-01`, `LCK-02`):** Album artwork covers the entire `360×360` circle at `33⅓ RPM` with zero text clutter on `NP-01`.
* **Inner Vinyl Circle Seek Ring (`r = 38px`):** A `360°` progress & seek indicator around the center vinyl label (`0° = 0%` at 12 o'clock → `359° = 99%`).
  * **Two ways to seek:** Slide the inner circle directly (with radial angle capture) **or** long-press (`300 ms`) & turn anywhere on the vinyl like a physical record.
* **Quick Actions (`NP-02`):** `1 touch` on `NP-01` reveals floating transparent **Song Title, Artist, and Album** metadata above a bottom arc of 4 circular pucks (`heart`, `shuffle`, `playlist`, and the quote-bubble `lyrics` icon). `1 touch` again closes it; `2 quick touches` toggles Play/Pause immediately.
* **Volume HUD (`NP-05`):** Tracing the right rim displays only the outer right-side volume arc (`r = 166px`), hero volume number, and output path (`4.4 mm BAL`)—no inner ring clutter.

### 2. Icon-Only Control Center (`CC-01` – `CC-02`)
* Pull down from the top edge (`y < 40px`) from anywhere in the OS.
* **`3×2` Grid of `66px` Icon Pucks (Zero Text Clutter):**
  * **Row 1:** `Wi-Fi` (`fjord`), `Bluetooth` (`fjord`), `Active Mode` (`moss` — swaps between 6 custom mode icons).
  * **Row 2:** `Gain` (swaps between `gain_low` and `gain_high` in `cloudberry`), `USB Charge` (`moss` when ON), `Touch Lock` (`CC-02` 600 ms hold shield).
* **Tap** toggles state; **Hold (`450 ms`)** on `Wi-Fi`, `Bluetooth`, or `Mode` opens its submenu.

### 3. Music Library (`LIB-01` – `LIB-04`)
* **Fixed Hub Order (`LIB-01`):** `Favorites` → `Playlists` → `Artists` → `Albums` → `Genres`.
* **Albums Cover Arc (`LIB-02`):** Supports both horizontal swipes across the screen (1 album step) and right-side rim scrubbing for 200+ album libraries.
* **Clean Track List (`LIB-03`):** Playing track displays animated `moss` equalizer bars + `Playing` without codec clutter.

### 4. Browse Files (`FIL-01` – `FIL-04`)
* Direct microSD directory tree (`/mnt/sdcard`) with long-press **Action Sheet (`FIL-02`)**:
  * **On a File:** Edit metadata tags (ID3/Vorbis) + filename, move, or delete.
  * **On a Folder:** Rename folder, or **recursively move / delete** the folder and all nested contents (protected by a `600 ms` hold on the `lingon` red button in `FIL-03`).

### 5. Operating Modes (`MOD-00` – `MOD-06`) & Settings (`SET-01` – `SET-07`)
* **6 Hardware Modes:** `Player`, `USB DAC` (with smooth `.train` marquee for long labels), `Digital Transport`, `Bluetooth RX`, `AirPlay`, and `File Transfer`.
* **Fast-Cache Bluetooth (`SET-07`):**
  * Persists `dial_bt_peer_cache_t` in NVS/flash where index `0` is always the **last connected device (`Default`)** with its `BD_ADDR`, baseband `clock_offset`, cached `remote_seid`, preferred codec, and `saved_volume`.
  * Turning on Bluetooth issues a unicast **Direct Page (`HCI_Create_Connection`)** to `peer_cache[0]` *before* any inquiry scan (`< 400 ms` lock) and negotiates the priority ladder: **`LDAC 990 kbps` → `LDAC 660/330` → `aptX HD` → `aptX` → `AAC` → `SBC`**.

### 6. Rotary Arcade (`GAM-01` – `GAM-04`)
* Three built-in mini-games driven by the virtual click-wheel rim: **Dial Pong**, **2048 Wheel**, and **Boomshine**.

---

## Repository Architecture

```text
dial-os/
├── AGENTS.md                              # Always-on workspace rules & Fjäll SSOT
├── Makefile                               # Native macOS Cocoa/SDL2 simulator & 4-tier ASan/UBSan test runner
├── .agents/skills/                        # Agent skills for UI building, HAL/IPC, and testing
│   ├── fjall-ui-builder/SKILL.md
│   ├── snowsky-disc-hal/SKILL.md
│   └── dialos-test-debug/SKILL.md
├── .github/workflows/
│   └── pages.yml                          # Automated GitHub Pages showcase deployment
├── assets/nordic/                         # Fjäll Design System & token exports
│   ├── fjall-design-system.html           # 26-slide interactive design sheet & 38 screens
│   ├── fjall-icons.svg                    # 70 custom vector icons (24x24, 1.75px stroke)
│   ├── dial_theme_fjall.h                 # Generated C theme header
│   ├── tokens.json                        # W3C DTCG JSON tokens
│   └── moodboard/                         # birch.jpg, moss.jpg, fjord.jpg
├── include/                               # Core C firmware headers
│   ├── dial_app.h                         # Live interactive application state controller
│   ├── dial_config.h                      # Display & polar geometry constants
│   ├── dial_draw.h                        # 360x360 SDF drawing primitives & typography
│   ├── dial_gestures.h                    # Touch & click-wheel gesture disambiguation FSM
│   ├── dial_hal.h                         # Dual-target HAL interface (macOS Cocoa/SDL2 & Linux FB)
│   ├── dial_icons.h                       # 70 Fjäll vector icon renderer
│   ├── dial_ipc.h                         # POSIX /player & /ui message queue & BT cache structs
│   ├── dial_polar.h                       # 12-o'clock polar math & r=38px seek mapping
│   ├── dial_screens.h                     # Screen IDs & state renderers
│   └── dial_theme.h                       # Fjäll Night/Day palettes & layout tokens
├── src/                                   # Platform-agnostic C engine & HAL implementations
│   ├── core/                              # dial_polar.c, dial_gestures.c, dial_app.c
│   ├── hal/                               # hal_sdl2.c (macOS Cocoa/CoreGraphics + SDL2 + PNG writer)
│   ├── screens/                           # dial_screens_core_np.c (SYS, LCK, HOM, CC, NP)
│   ├── ui/                                # dial_draw.c, dial_icons.c
│   └── main.c                             # CLI & live 60 FPS interactive desktop loop
├── tests/                                 # 4-tier automated test suite (ASan + UBSan)
│   ├── test_polar_gestures.c              # Tier 1: Polar math & gesture FSM unit tests
│   ├── test_draw_primitives.c             # Tier 2: SDF arcs, icons & marquee tests
│   ├── test_screens_v030.c                # Tier 3: Headless screen invariant verification
│   └── test_ui_interaction.c              # Tier 4: Live synthetic touch/gesture UI interaction tests
└── docs/
    ├── DIAL_OS_SYSTEM_REQUIREMENTS.md     # Hardware HAL, CS43131 DAC, GPIO & IPC blueprint
    └── ROADMAP.md                         # Milestone & release roadmap (v0.1.0 -> v1.0.0)
```

---

## Engineering & Contribution Standards

1. **Dual-Target HAL (`macOS Cocoa / SDL2 Simulator` + `Snowsky DISC MIPS32`):**
   * UI and state-machine code (`src/screens/`, `src/ui/`, `src/core/`) is 100% platform-agnostic C. Hardware calls (`/dev/fb0`, `/dev/input/event*`, `/player` POSIX mqueue) live strictly inside `src/hal/`.
2. **Zero Code Ships Without 4-Tier Tests + Live Interactive Verification:**
   * Every C module, polar gesture state machine, and screen requires automated unit tests, headless PNG snapshot verification (`--headless --screen <ID>`), and end-to-end interactive UI flow verification (`tests/test_ui_interaction.c` + live `./build/dialos-sim` window) passing under `AddressSanitizer` and `UndefinedBehaviorSanitizer` within the `24.0 MB` RAM budget.
3. **Atomic Conventional Commits & Semantic Version Tags:**
   * All commits follow [Conventional Commits](https://www.conventionalcommits.org/) (`feat:`, `fix:`, `test:`, `docs:`, `refactor:`, `chore:`).
   * Releases and deployment milestones are tagged with annotated Semantic Version tags (`v0.1.0`, `v0.2.0`, `v0.3.0`, etc.).
