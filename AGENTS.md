# DialOS — Workspace Rules & Single Source of Truth (Fjäll Design System)

## 1. Visual & Token Single Source of Truth (SSOT)
All UI implementation, simulator views, and C firmware code MUST strictly conform to the **Fjäll** ("Nordic Office & Nature") design system:
- **Interactive Spec & 38 Master Screens:** `assets/nordic/fjall-design-system.html`
- **C Theme & Geometry Tokens:** `include/dial_theme.h` (synced with `assets/nordic/dial_theme_fjall.h`)
- **W3C DTCG JSON Tokens:** `assets/nordic/tokens.json`
- **70 Vector Icons (`24×24` grid, `1.75px` stroke, round caps):** `assets/nordic/fjall-icons.svg`
- **Hardware & System Architecture:** `docs/DIAL_OS_SYSTEM_REQUIREMENTS.md`
- **Milestone & Release Roadmap (`v0.1.0` → `v1.0.0`):** `docs/ROADMAP.md`

### Strict Token Discipline
- **Never hardcode raw hex colours, pixel sizes, or durations** in screen code. Always reference `dial_palette_t` (`DIAL_PALETTE_NIGHT` / `DIAL_PALETTE_DAY`) and the `DIAL_*` macros in `include/dial_theme.h` and `include/dial_config.h`.
- **Semantic Colour Rule:**
  - `moss` (`#8DB596` Night / `#47755A` Day) is the **only primary interactive accent** (focus rings, active toggles, track progress, active mode).
  - `fjord` is reserved for **connectivity** (`Wi-Fi`, `Bluetooth`).
  - `cloudberry` is reserved for **warnings, high gain (`+6 dB`), and game scores**.
  - `lingon` is reserved **exclusively for destructive actions and errors** (never decorative).
  - `birch` is reserved for **brightness and warm neutral highlights**.

---

## 2. Locked Screen & UX Architecture Decisions
Do not regress any of the following locked design specifications:

1. **Full-Bleed Spinning Vinyl (`NP-01`–`NP-05` & `LCK-02`):**
   - Album artwork covers `100%` of the circular display (`360×360`, $r = 180\text{px}$) with subtle vinyl groove rings and radial edge vignette.
   - `LCK-02` (Lock screen while playing) also uses `360×360` full-bleed spinning art with floating transparent track title/artist (no rectangular card box) and 3 bottom transport buttons.

2. **Now Playing (`NP-01`) — Pure Vinyl & Inner Circle Seek Ring:**
   - **Zero song title, artist, or album text** on `NP-01`.
   - **No left-side seek arc** on the outer rim.
   - **Inner Vinyl Circle (`r = 38px`, `DIAL_VINYL_SEEK_R`):** Single progress & seek indicator around the center label border. Maps angle $\theta \in [0^\circ, 360^\circ)$ clockwise from 12 o'clock (`0° = 0%`, `90° = 25%`, `180° = 50%`, `270° = 75%`, `359° = 99%`).
   - **Two Supported Seek Gestures:**
     1. **Direct slide on the inner ring (`24px ≤ r ≤ 54px`):** Grabbing and sliding along the inner ring scrubs directly to that angle (with radial capture so the thumb can drift outward once grabbed).
     2. **Long-press (`300 ms`) & turn anywhere on the vinyl (`r < 145px`):** Turntable scrub mode (pauses auto-spin, fires haptic tick, and rotates the inner seek ring).
   - **Bottom Handle:** Only the `^` (`up`) chevron icon at `bottom: 16px` (no text label). Swiping up opens `NP-04` (Track Details, read-only).
   - **Tap Gestures on `NP-01`:**
     - **1 touch (single tap):** Opens `NP-02` (Quick Actions).
     - **2 quick touches (double-tap):** Toggles Play / Pause immediately.
     - **Swipe Right / Left:** Next / Previous track.

3. **Quick Actions (`NP-02`) — Floating Metadata & Bottom Arc:**
   - Opened with **1 touch** on `NP-01`; **1 touch** again closes it.
   - Top overline: `QUICK ACTIONS`.
   - Center: **Song Title, Artist, and Album rendered as floating text on a transparent background** (no rectangular/pill card background).
   - Bottom Arc (`r = 118px` at polar angles `234°`, `198°`, `162°`, `126°`): 4 circular `52px` action pucks:
     1. `heart` (Favorite)
     2. `shuffle` (Shuffle)
     3. `playlist` (Add to playlist)
     4. `lyrics` (Speech bubble with quotation marks `“ ”` — opens `NP-03` Synced Lyrics)

4. **Volume HUD (`NP-05`):**
   - Triggered when tracing the right rim (`r > 150px`).
   - **Do NOT render any inner circle** (neither a `270°` center ring nor the `r = 38px` vinyl ring). Show **only the right-side outer rim arc** (`r = 166px`, `w = 6px`), the centered hero volume number (`82`), `VOLUME` caption, and output text (`4.4 mm BAL`).

5. **Control Center (`CC-01`) — Icon-Only `3×2` Pucks:**
   - `3×2` grid of `66px` circular icon pucks with **zero text labels** inside or beneath the pucks:
     - Row 1: `wifi` (`fjord` when ON), `bluetooth` (`fjord` when ON), `mode_*` (`moss`, displays the icon of the active mode: `mode_player`, `mode_usbdac`, `mode_transport`, `mode_btrx`, `mode_airplay`, `mode_transfer`).
     - Row 2: `gain_low` / `gain_high` (`cloudberry` when High Gain ON, swaps icon), `usb_charge` (`moss` when ON, unlit when OFF), `lock` (Touch Lock shield `CC-02`).
   - **Tap** = toggle state; **Hold (`450 ms`)** on `Wi-Fi`, `Bluetooth`, or `Mode` opens its respective submenu (`SET-06`, `SET-07`, `MOD-00`).

6. **Music Library (`LIB-01`–`LIB-04`):**
   - `LIB-01` row order is strictly:
     1. **Favorites** (`heart`)
     2. **Playlists** (`playlist`)
     3. **Artists** (`artist`)
     4. **Albums** (`album`)
     5. **Genres** (`genre`)
   - `LIB-02` (Albums Cover Arc): Supports **both** horizontal across-screen swipes (1 album step) and right-side rim scrolling (`scrollRim` fast scrub).
   - `LIB-03` (Track List): Playing track shows animated `moss` equalizer bars + `Playing` (no codec/bit-depth clutter in the track list).

7. **Browse Files (`FIL-01`–`FIL-04`):**
   - **Long-press on a File (`FIL-02`):** Opens the Action Sheet to **Edit tags** (edits song metadata tags ID3/Vorbis + file name, never raw audio bytes), **Move**, or **Delete**.
   - **Long-press on a Folder (`FIL-02`):** Allows **renaming** the folder, or **recursively moving / deleting** the folder and all nested contents.
   - **Delete Confirmation (`FIL-03`):** Requires a `600 ms` hold on the `lingon` red button (`Hold to delete`).

8. **Operating Modes (`MOD-02`) & Fast-Cache Bluetooth (`SET-07`):**
   - Long row titles in focused/compact rows (such as `"Draw power from host"` in `MOD-02`) use a smooth horizontal train-slide marquee rather than clipping with `..`.
   - **Fast-Cache Bluetooth (`SET-07`):**
     - Persist `dial_bt_peer_cache_t` in NVS/flash where index `0` is always the **last connected device (`Default`)** along with its `bd_addr[6]`, `clock_offset`, `page_scan_mode`, cached `remote_seid`, `preferred_codec`, and `saved_volume`.
     - When Bluetooth is enabled, issue a unicast **Direct Page (`HCI_Create_Connection`)** to `peer_cache[0].bd_addr` *before* any inquiry scan (`< 400 ms` lock).
     - Negotiate codecs via cached `remote_seid` or single-pass priority ladder: **`LDAC 990 kbps` → `LDAC 660/330` → `aptX HD` → `aptX` → `AAC` → `SBC`**, and restore `saved_volume` per MAC address.

---

## 3. Engineering & Dual-Target HAL Architecture
1. **Strict HAL Boundary (`src/hal/`):**
   - Screen, component, and state-machine code (`src/screens/`, `src/ui/`, `src/core/`) MUST be 100% platform-agnostic C.
   - Never include Linux-specific headers (`<linux/fb.h>`, `<linux/input.h>`, `<mqueue.h>`) outside `src/hal/`.
   - Every feature must build and run natively on **macOS Simulator (SDL2)** and cross-compile cleanly for **Snowsky DISC (Ingenic X2000 MIPS32 Linux)**.
2. **Memory & Performance Budget:**
   - Total UI process RAM footprint must remain **$\le 24.0\text{ MB}$** at `60 FPS` (`16.67 ms` frame budget).
   - Use static/arena allocation for UI nodes and texture atlases; avoid per-frame `malloc`/`free` in the render loop.

---

## 4. Git, Testing, Open-Source & Release Discipline (Mandatory)
1. **Atomic Conventional Commits:**
   - Every commit MUST be small, atomic (one logical change per commit), and formatted strictly according to [Conventional Commits](https://www.conventionalcommits.org/):
     - `feat(<scope>): ...` — new screen, component, gesture, or HAL feature
     - `fix(<scope>): ...` — bugfix, layout correction, or memory fix
     - `test(<scope>): ...` — unit, gesture FSM, or headless framebuffer snapshot tests
     - `docs(<scope>): ...` — documentation, design sheet, or README updates
     - `refactor(<scope>): ...` — internal restructuring without behavior change
     - `build:` / `ci:` / `chore:` — toolchain, GitHub Actions, or repo maintenance
2. **Zero Code Ships Without Tests (TDD Gate):**
   - **Never commit or merge C implementation code without accompanying automated tests** (`tests/`).
   - Every logic module (polar math, gesture FSM, Bluetooth cache, list physics) requires unit tests, and every UI screen (`SYS`, `LCK`, `HOM`, `CC`, `NP`, `LIB`, `FIL`, `MOD`, `SET`, `GAM`, `OVL`) requires headless snapshot verification (`--headless --screen <ID>`) passing under `AddressSanitizer` + `UndefinedBehaviorSanitizer`.
3. **Tagged Releases & Deployments:**
   - Every release or deployment milestone MUST be tagged with an annotated Semantic Version tag (`git tag -a vX.Y.Z -m "..."`).
4. **Open-Source Showcase & GitHub Pages Sync:**
   - This is an open-source project. Keep `README.md` and the interactive GitHub Pages showcase (`assets/nordic/fjall-design-system.html` deployed via `.github/workflows/pages.yml`) up to date whenever screens or features evolve.
