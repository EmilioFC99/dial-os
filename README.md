# DialOS — Fjäll Edition (`v1.0.0`)

> **Calm sound, natural interface.**  
> An open-source, tactile, rotary-driven operating system for the **FiiO Snowsky DISC** (`1.8"` `360×360` circular touch display, Ingenic X2000 MIPS32, dual Cirrus Logic CS43131 DACs).

---

## Quick Start: Live Interactive C Emulator, Real Audio & 5-Tier Test Suite

DialOS compiles out-of-the-box on macOS (zero external dependencies required — uses native macOS `Cocoa`, `CoreGraphics`, `ImageIO`, and `AudioToolbox`, or `SDL2` if installed) and cross-compiles for the **FiiO Snowsky DISC** (`mipsel-linux-gnu-gcc` via `Dockerfile`).

### 1. Run the Live Interactive `360×360` Desktop Window (`60 FPS` + Real Audio)
```bash
# Build the native C simulator
make sim

# Launch the live interactive 360x360 window with a real music folder (FLAC/MP3/WAV/M4A + cover.jpg)
./build/dialos-sim --music "/path/to/your/Music"

# Or launch with the built-in Nordic demo catalog / directly into any of the 38 screens:
./build/dialos-sim
./build/dialos-sim --screen HOM-01
./build/dialos-sim --screen SET-04 --mode day
```

**Mouse & Touch Gestures in the Live Window:**
* **`NP-01` Inner Vinyl Seek Ring (`24px ≤ r ≤ 54px`):** Click and drag around the `r = 38px` center vinyl border (with radial capture) to scrub through the live audio track, or hold `300 ms` anywhere on the vinyl (`r < 145px`) to enter Turntable Scrub mode.
* **`NP-01` Center Tap (`r < 145px`):** Single-click opens `NP-02` Quick Actions (`heart`, `shuffle`, `playlist`, `lyrics` pucks at `234°`, `198°`, `162°`, `126°`); double-click (`< 260 ms`) toggles Play/Pause (`"0201"`).
* **Outer Rim (`150px ≤ r ≤ 180px`):** Drag clockwise/counter-clockwise to scroll lists (`HOM-01`, `LIB-01..04`, `FIL-01`, `SET-01..07`), scrub albums on `LIB-02`, rotate the paddle in `GAM-02` Dial Pong, or adjust volume on the right rim (`NP-05`).
* **Top Edge Pull-Down (`y < 40px`):** Pull down from the top rim on any screen to open `CC-01` Control Center (`66px` icon-only `3×2` pucks); hold `450 ms` on `Wi-Fi`, `Bluetooth`, or `Mode` to open `SET-06`, `SET-07`, or `MOD-00`; hold `600 ms` on `CC-02` to unlock Touch Lock.
* **Bottom Handle Swipe-Up (`y > 320px`):** Swipe up to wake from `LCK-01`/`LCK-02`, open `NP-04` Track Details from `NP-01`, or return home.

**Keyboard Shortcuts in the Live Window:**
| Key | Action |
| :--- | :--- |
| `Space` | Toggle Play / Pause (`DIAL_CMD_PLAY_PAUSE`) |
| `Up` / `Down` | Step Volume `±2%` and trigger `NP-05` Volume HUD |
| `Left` / `Right` or `[` / `]` | Cycle through all 38 master screens (`SYS-00` → `OVL-04`) |
| `D` | Toggle **Polar Night** (`#121615`) ↔ **Birch Day** (`#F4F1EA`) live |
| `Esc` | Toggle Lock Screen (`LCK-02` / `LCK-01`) ↔ Home (`HOM-01`) |
| `S` | Save current `360×360` frame to `/tmp/dialos_live.png` |
| `Q` | Quit simulator |

### 2. Run the Automated Test Suite & Snowsky DISC Target Verification
Every commit touching C code must pass all 9 automated test binaries + `.fw` firmware package verification under `AddressSanitizer` + `UndefinedBehaviorSanitizer`:
```bash
make test
```
1. **`./build/test_polar_gestures`:** 12-o'clock polar math (`0°=top, 90°=right, 180°=bottom, 270°=left`), `r = 38px` seek ring mapping, and Gesture FSM disambiguation.
2. **`./build/test_draw_primitives`:** Anti-aliased SDF arcs, full-bleed vinyl art, 70 Fjäll vector icons, `.train` horizontal marquee, and `<= 24.0 MB` static RAM check (`2.44 MB` actual).
3. **`./build/test_screens_v030` / `v040` / `v050`:** Pixel-level headless framebuffer invariant assertions across all **38 Master Screens** in both Polar Night and Birch Day palettes.
4. **`./build/test_bt_cache`:** MRU `dial_bt_peer_cache_t` (`Default` at index `0`), `< 400 ms` unicast Direct Page (`HCI_Create_Connection`), cached `remote_seid`, and `LDAC 990` → `660/330` → `aptX HD` → `aptX` → `AAC` → `SBC` priority ladder.
5. **`./build/test_dac_relay`:** CS43131 anti-pop relay sequencer (`8 ms` digital soft-mute ramp → `GPIO_PD14` analogue relay switch → `12 ms` contact bounce settle → `8 ms` soft-unmute ramp).
6. **`./build/test_hal_disc` & `make fw`:** Direct verification of the Snowsky DISC hardware backend (`src/hal/hal_disc.c`, RGB565 `FBIOPAN_DISPLAY` pipeline, `/player` IPC ring) and CRC32-verified `.fw` update packaging (`build/dialos-fjall-v1.0.0-disc.fw`).
7. **`./build/test_ui_interaction`:** End-to-end synthetic mouse/touch interaction through `dial_app_t` + `dial_gesture_fsm_t` verifying that user gestures mutate live state and rendered framebuffer pixels.

### 3. Cross-Compile for FiiO Snowsky DISC (Ingenic X2000 MIPS32)
```bash
# Portable host verification of hal_disc.c + .fw image generation
make disc-check

# Full MIPS32 little-endian cross-compilation via Docker (mipsel-linux-gnu-gcc)
make docker-disc
```

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

## Screen & Interaction Tour (All 38 Master Screens Implemented in C)

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
  * **Row 2:** `Gain` (swaps between `gain_low` and `gain_high` in `cloudberry`, sequenced through the `28 ms` CS43131 anti-pop relay interlock), `USB Charge` (`moss` when ON), `Touch Lock` (`CC-02` 600 ms hold shield).
* **Tap** toggles state; **Hold (`450 ms`)** on `Wi-Fi`, `Bluetooth`, or `Mode` opens its submenu.

### 3. Music Library (`LIB-01` – `LIB-04`)
* **Fixed Hub Order (`LIB-01`):** `Favorites` → `Playlists` → `Artists` → `Albums` → `Genres`.
* **Albums Cover Arc (`LIB-02`):** Supports both horizontal swipes across the screen (1 album step) and right-side rim scrubbing for 200+ album libraries, decoding real `cover.jpg` files into `180×180` RGB565 textures.
* **Clean Track List (`LIB-03`):** Playing track displays animated `moss` equalizer bars + `Playing` without codec clutter.

### 4. Browse Files (`FIL-01` – `FIL-04`)
* Direct microSD directory tree (`/mnt/sdcard` or `--music` directory) with long-press **Action Sheet (`FIL-02`)**:
  * **On a File:** Edit metadata tags (ID3/Vorbis) + filename, move, or delete.
  * **On a Folder:** Rename folder, or **recursively move / delete** the folder and all nested contents (protected by a `600 ms` hold on the `lingon` red button in `FIL-03`).

### 5. Operating Modes (`MOD-00` – `MOD-06`) & Settings (`SET-01` – `SET-07`)
* **6 Hardware Modes:** `Player`, `USB DAC` (with smooth `.train` marquee for `"Draw power from host"`), `Digital Transport`, `Bluetooth RX`, `AirPlay`, and `File Transfer`.
* **Audio & EQ (`SET-02` – `SET-04`):** CS43131 5-filter reconstruction picker (`Fast/Slow Linear`, `Fast/Slow Min-Phase`, `NOS`) and 10-band Parametric EQ with live `moss` spline response curve.
* **Fast-Cache Bluetooth (`SET-07`):**
  * Persists `dial_bt_peer_cache_t` in NVS/flash where index `0` is always the **last connected device (`Default`)** with its `BD_ADDR`, baseband `clock_offset`, cached `remote_seid`, preferred codec, and `saved_volume`.
  * Turning on Bluetooth issues a unicast **Direct Page (`HCI_Create_Connection`)** to `peer_cache[0]` *before* any inquiry scan (`< 400 ms` lock) and negotiates the priority ladder: **`LDAC 990 kbps` → `LDAC 660/330` → `aptX HD` → `aptX` → `AAC` → `SBC`**.

### 6. Rotary Arcade (`GAM-01` – `GAM-04`) & System Overlays (`OVL-01` – `OVL-04`)
* Three built-in mini-games driven by the virtual click-wheel rim: **Dial Pong** (`GAM-02`), **2048 Wheel** (`GAM-03`), and **Boomshine** (`GAM-04`).
* Non-blocking top pill toasts (`OVL-01` Liked, `OVL-02` Bluetooth `fjord`), Low Battery caution sheet (`OVL-03`), and USB Charging mountain horizon (`OVL-04`).

---

## Repository Architecture

```text
dial-os/
├── AGENTS.md                              # Always-on workspace rules & Fjäll SSOT
├── Dockerfile                             # Ingenic X2000 MIPS32 (mipsel-linux-gnu-gcc) cross-build
├── Makefile                               # Dual-target simulator, Snowsky DISC HAL & 9-suite test runner
├── .agents/skills/                        # Agent skills for UI building, HAL/IPC, and testing
│   ├── fjall-ui-builder/SKILL.md
│   ├── snowsky-disc-hal/SKILL.md
│   └── dialos-test-debug/SKILL.md
├── .github/workflows/
│   └── pages.yml                          # Automated GitHub Pages showcase deployment
├── assets/nordic/                         # Fjäll Design System & token exports
│   ├── fjall-design-system.html           # 26-slide interactive design sheet & 38 master screens
│   ├── fjall-icons.svg                    # 70 custom vector icons (24x24, 1.75px stroke)
│   ├── dial_theme_fjall.h                 # Generated C theme header
│   ├── tokens.json                        # W3C DTCG JSON tokens
│   └── moodboard/                         # birch.jpg, moss.jpg, fjord.jpg
├── include/                               # Core C firmware headers
│   ├── dial_app.h                         # Live interactive application state controller
│   ├── dial_assets.h                      # Pre-baked 8-bit AA fonts & 180x180 RGB565 vinyl covers
│   ├── dial_bt.h                          # Fast-Cache Bluetooth MRU & codec negotiation engine
│   ├── dial_config.h                      # Display & polar geometry constants
│   ├── dial_dac.h                         # CS43131 8ms/12ms/8ms anti-pop relay sequencer
│   ├── dial_draw.h                        # 360x360 SDF drawing primitives & typography
│   ├── dial_gestures.h                    # Touch & click-wheel gesture disambiguation FSM
│   ├── dial_hal.h                         # Dual-target HAL interface (macOS Cocoa/SDL2 & Linux FB)
│   ├── dial_icons.h                       # 70 Fjäll vector icon renderer
│   ├── dial_ipc.h                         # POSIX /player & /ui message queue & BT cache structs
│   ├── dial_media.h                       # Real audio streaming, directory scanner & cover art cache
│   ├── dial_polar.h                       # 12-o'clock polar math & r=38px seek mapping
│   ├── dial_screens.h                     # All 38 screen IDs & state renderers
│   └── dial_theme.h                       # Fjäll Night/Day palettes & layout tokens
├── src/                                   # Platform-agnostic C engine & HAL implementations
│   ├── core/                              # dial_polar.c, dial_gestures.c, dial_bt.c, dial_dac.c, dial_app.c
│   ├── hal/                               # hal_sdl2.c, hal_disc.c (/dev/fb0 + mqueue), hal_media.c
│   ├── screens/                           # dial_screens_core_np.c, lib_fil.c, mod_set.c, gam_ovl.c
│   ├── ui/                                # dial_assets.c, dial_draw.c, dial_icons.c
│   └── main.c                             # CLI & live 60 FPS interactive desktop / device loop
├── tools/                                 # Asset & firmware packaging utilities
│   ├── bake_fjall_assets.m                # CoreText 8-bit font atlas & vinyl cover baker
│   └── pack_disc_fw.c                     # Flashable .fw firmware image packager & CRC32 verifier
├── tests/                                 # 9-suite automated test harness (ASan + UBSan)
│   ├── test_polar_gestures.c              # Polar math & gesture FSM unit + fuzz tests
│   ├── test_draw_primitives.c             # SDF arcs, 70 icons, marquee & 24MB RAM budget check
│   ├── test_screens_v030.c                # Core & Now Playing (SYS, LCK, HOM, CC, NP) invariants
│   ├── test_screens_v040.c                # Library & Files (LIB-01..04, FIL-01..04) invariants
│   ├── test_screens_v050.c                # Modes, Settings, Arcade & Overlays (38/38 screens)
│   ├── test_bt_cache.c                    # Fast-Cache Bluetooth (<400ms Direct Page & codec ladder)
│   ├── test_dac_relay.c                   # CS43131 8ms/12ms/8ms anti-pop relay sequencer tests
│   ├── test_hal_disc.c                    # Snowsky DISC HAL, RGB565 framebuffer & /player IPC ring
│   └── test_ui_interaction.c              # End-to-end synthetic touch/gesture UI interaction tests
└── docs/
    ├── DIAL_OS_SYSTEM_REQUIREMENTS.md     # Hardware HAL, CS43131 DAC, GPIO & IPC blueprint
    ├── ROADMAP.md                         # Milestone & release roadmap (v0.1.0 -> v1.0.0)
    └── SESSION_HANDOFF.md                 # Session state, verification commands & milestone summary
```
