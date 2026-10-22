# DialOS — Fjäll Edition

> **Calm sound, natural interface.**  
> An open-source, tactile, rotary-driven operating system for the **FiiO Snowsky DISC** (`1.8"` `360×360` circular touch display, Ingenic X2000 MIPS32, dual Cirrus Logic CS43131 DACs).

---

## Live Interactive UI Tour & Design System

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
│   ├── dial_config.h                      # Display & polar geometry constants
│   ├── dial_ipc.h                         # POSIX /player & /ui message queue & BT cache structs
│   └── dial_theme.h                       # Fjäll Night/Day palettes & layout tokens
└── docs/
    └── DIAL_OS_SYSTEM_REQUIREMENTS.md     # Hardware HAL, CS43131 DAC, GPIO & IPC blueprint
```

---

## Engineering & Contribution Standards

1. **Dual-Target HAL (`macOS SDL2 Simulator` + `Snowsky DISC MIPS32`):**
   * UI and state-machine code (`src/screens/`, `src/ui/`, `src/core/`) is 100% platform-agnostic C. Hardware calls (`/dev/fb0`, `/dev/input/event*`, `/player` POSIX mqueue) live strictly inside `src/hal/`.
2. **No Code Ships Without Tests:**
   * Every C module, polar gesture state machine, and screen requires automated unit tests and headless PNG snapshot verification (`--headless --screen <ID>`) passing under `AddressSanitizer` and `UndefinedBehaviorSanitizer` within the `24.0 MB` RAM budget.
3. **Atomic Conventional Commits & Semantic Version Tags:**
   * All commits follow [Conventional Commits](https://www.conventionalcommits.org/) (`feat:`, `fix:`, `test:`, `docs:`, `refactor:`, `chore:`).
   * Releases and deployment milestones are tagged with annotated Semantic Version tags (`v0.1.0`, `v0.2.0`, etc.).
