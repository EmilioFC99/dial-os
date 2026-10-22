---
name: dialos-test-debug
description: >-
  Use this skill when testing, debugging, or validating DialOS C code, polar gesture state
  machines, headless screen rendering snapshots, or memory/sanitizer budgets.
---

# DialOS Testing, Debugging & Verification Skill

Use this workflow to build, test, and visually debug DialOS on macOS before deploying to the Snowsky DISC hardware.

## 1. Three-Tier Verification Loop
Every new feature or bugfix must pass three checks on macOS:

### Tier A: Polar Math & Gesture FSM Unit Tests (`make test`)
Test the core state machines without opening a window:
- **Inner Vinyl Seek Ring (`r = 38px`, `24px <= r <= 54px`):**
  - Verify `atan2(x - 180, -(y - 180))` maps `(180, 142)` → `0° (0.00)`, `(218, 180)` → `90° (0.25)`, `(180, 218)` → `180° (0.50)`, `(142, 180)` → `270° (0.75)`.
  - Verify both **direct inner-ring drag** and **long-press (`>= 300 ms`) vinyl turntable scrub** emit `DIAL_CMD_SEEK` (`"0103"`) with the expected millisecond target.
- **Now Playing Tap Disambiguation (`NP-01`):**
  - Single tap (`1 touch` without drag) transitions `NP-01` ↔ `NP-02` (Quick Actions).
  - Double-tap (`2 touches` within `260 ms`) emits `DIAL_CMD_PLAY_PAUSE` (`"0201"`) and stays on `NP-01`.
- **Outer Rim Detent (`r >= 150px`):**
  - Verify every `15°` (`DIAL_DETENT_DEG`) of rotation increments/decrements list focus by `1` (or `3` when angular velocity `> 180°/s`) and triggers an `8 ms` haptic tick.

### Tier B: Headless Framebuffer Snapshot Comparison
The simulator binary must support a headless CLI flag:
```sh
./build/dialos-sim --headless --screen <SCREEN_ID> --mode <night|day> --out /tmp/<SCREEN_ID>.png
```
1. Render the target screen ID (e.g. `NP-01`, `NP-02`, `NP-05`, `CC-01`, `LIB-01`, `FIL-02`, `SET-07`) to `/tmp/<SCREEN_ID>.png`.
2. Call `read_file` on `/tmp/<SCREEN_ID>.png` to visually inspect the rendered C framebuffer.
3. Verify zero bezel clipping outside `r = 180px`, correct token colors, and exact alignment with [`assets/nordic/fjall-design-system.html`](../../assets/nordic/fjall-design-system.html).

### Tier C: AddressSanitizer, UBSan & 24 MB RAM Budget Gate
Compile the simulator/test binary with sanitizers enabled:
```sh
clang -O1 -g -fsanitize=address,undefined -Wall -Wextra -Werror ...
```
- Run through a 1,000-step automated navigation & rotary fuzz test.
- Assert zero memory leaks, zero out-of-bounds framebuffer writes (`360×360` bounds check), and total heap allocation `<= 24.0 MB`.

## 2. Commit & Release Gate
1. **Never ship code without tests:** Every `feat` or `fix` commit touching C code must include or update its corresponding test in `tests/` and pass `make test`.
2. **Atomic Conventional Commits:** Commit each logical unit separately (`feat(np): ...`, `test(np): ...`, `docs(readme): ...`).
3. **Release Tags:** Tag release milestones with annotated SemVer tags (`git tag -a vX.Y.Z -m "..."`).
