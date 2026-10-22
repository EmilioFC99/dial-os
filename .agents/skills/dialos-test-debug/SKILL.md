---
name: dialos-test-debug
description: >-
  Use this skill when testing, debugging, or validating DialOS C code, polar gesture state
  machines, headless screen rendering snapshots, or memory/sanitizer budgets.
---

# DialOS Testing, Debugging & Verification Skill

Use this workflow to build, test, and visually debug DialOS on macOS before deploying to the Snowsky DISC hardware.

## 1. Four-Tier Verification Loop (Mandatory Before Every Commit)
Every new feature or bugfix must pass four checks on macOS:

### Tier A: Polar Math & Gesture FSM Unit Tests (`./build/test_polar_gestures`)
Test the core state machines without opening a window:
- **Inner Vinyl Seek Ring (`r = 38px`, `24px <= r <= 54px`):**
  - Verify `atan2(x - 180, -(y - 180))` maps `(180, 142)` → `0° (0.00)`, `(218, 180)` → `90° (0.25)`, `(180, 218)` → `180° (0.50)`, `(142, 180)` → `270° (0.75)`.
  - Verify both **direct inner-ring drag** and **long-press (`>= 300 ms`) vinyl turntable scrub** emit `DIAL_CMD_SEEK` (`"0103"`) with the expected millisecond target.
- **Now Playing Tap Disambiguation (`NP-01`):**
  - Single tap (`1 touch` without drag) transitions `NP-01` ↔ `NP-02` (Quick Actions).
  - Double-tap (`2 touches` within `260 ms`) emits `DIAL_CMD_PLAY_PAUSE` (`"0201"`) and stays on `NP-01`.
- **Outer Rim Detent (`r >= 150px`):**
  - Verify every `15°` (`DIAL_DETENT_DEG`) of rotation increments/decrements list focus by `1` (or `3` when angular velocity `> 180°/s`) and triggers an `8 ms` haptic tick.

### Tier B: End-to-End Interactive UI Simulation (`./build/test_ui_interaction` & Live Window)
1. **Automated Interactive Flows (`./build/test_ui_interaction`):**
   - Drive real `dial_touch_event_t` sequences into the live application state machine (`dial_app_t`) and render the resulting framebuffer after each gesture:
     - Unlock `LCK-02` via bottom swipe-up → lands on `HOM-01`.
     - Trace outer rim on `HOM-01` → rotates focused app pebble (`HOM-01` → `HOM-02`), tap center to open screen.
     - On `NP-01`: drag `r = 38px` inner vinyl seek ring → mutates live `seek_progress` and moves the rendered `moss` knob on screen; single-tap → opens `NP-02`; tap `heart` puck (`234°`) → toggles favorite state and puck tint; pull down top edge (`y < 40`) → opens `CC-01`; tap `gain` puck → swaps `gain_low` ↔ `gain_high` (`cloudberry`); tap `lock` puck → opens `CC-02` and hold center `600 ms` → fills unlock arc and unlocks.
2. **Live Desktop Window (`make sim && ./build/dialos-sim`):**
   - Runs a native macOS `Cocoa` / `CoreGraphics` (or `SDL2`) `360×360` interactive window at `60 FPS` with mouse touch emulation and hardware button hotkeys (`Space`, `Up`/`Down`, `Left`/`Right`, `D` for Night/Day, `S` for screenshot).

### Tier C: Headless Framebuffer Snapshot Comparison
The simulator binary must support a headless CLI flag:
```sh
./build/dialos-sim --headless --screen <SCREEN_ID> --mode <night|day> --out /tmp/<SCREEN_ID>.png
```
1. Render the target screen ID (e.g. `NP-01`, `NP-02`, `NP-05`, `CC-01`, `LIB-01`, `FIL-02`, `SET-07`) to `/tmp/<SCREEN_ID>.png`.
2. Call `read_file` on `/tmp/<SCREEN_ID>.png` to visually inspect the rendered C framebuffer.
3. Verify zero bezel clipping outside `r = 180px`, correct token colors, and exact alignment with [`assets/nordic/fjall-design-system.html`](../../assets/nordic/fjall-design-system.html).

### Tier D: AddressSanitizer, UBSan & 24 MB RAM Budget Gate
Compile the simulator/test binaries with sanitizers enabled:
```sh
clang -O1 -g -fsanitize=address,undefined -Wall -Wextra -Werror ...
```
- Run through a 1,000-step automated navigation & rotary fuzz test.
- Assert zero memory leaks, zero out-of-bounds framebuffer writes (`360×360` bounds check), and total heap/arena allocation `<= 24.0 MB`.

## 2. Commit & Release Gate
1. **Never ship code without tests:** Every `feat` or `fix` commit touching C code must include or update its corresponding test in `tests/` and pass `make test`.
2. **Atomic Conventional Commits:** Commit each logical unit separately (`feat(np): ...`, `test(np): ...`, `docs(readme): ...`).
3. **Release Tags:** Tag release milestones with annotated SemVer tags (`git tag -a vX.Y.Z -m "..."`).
