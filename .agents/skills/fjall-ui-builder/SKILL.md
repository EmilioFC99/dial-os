---
name: fjall-ui-builder
description: >-
  Use this skill when implementing, modifying, or reviewing any DialOS screen, UI component,
  icon, typography role, or circular layout in C or HTML/SVG to ensure pixel-accurate
  compliance with the Fjäll Design System (assets/nordic/fjall-design-system.html).
---

# Fjäll UI Builder Skill

Follow this workflow whenever building or updating DialOS UI components or screens (`SYS`, `LCK`, `HOM`, `CC`, `NP`, `LIB`, `FIL`, `MOD`, `SET`, `GAM`, `OVL`).

## 1. Consult the Single Source of Truth First
Before writing rendering code for any screen:
1. Read the corresponding screen definition (`SCREENS` array) in [`assets/nordic/fjall-design-system.html`](../../assets/nordic/fjall-design-system.html) (lines 934–1140).
2. Check the C token definitions in [`include/dial_theme.h`](../../include/dial_theme.h) and icon paths in [`assets/nordic/fjall-icons.svg`](../../assets/nordic/fjall-icons.svg).

## 2. Concentric Geometry & Layout Math (`360 × 360`, Center = `(180, 180)`)
Use these exact coordinates and radii in all C rendering primitives:
- **Polar helper (`0°` = 12 o'clock, clockwise):**
  ```c
  float rad = deg * (M_PI / 180.0f);
  float x = 180.0f + r * sinf(rad);
  float y = 180.0f - r * cosf(rad);
  ```
- **Rim Zone (`150 <= r <= 180`):**
  - Outer rim arcs (`rimArc`): radius `r = 166` (`DIAL_RIM_ARC_R`), stroke width `4px` (`6px` on `NP-05` Volume HUD).
  - Right scroll indicator (`scrollRim`): `52°` to `128°`, `r = 170`, `w = 3px`.
- **Fisheye Rim List (`rimList`):**
  - Focused row (`d == 0`): `284 × 52 px`, radius `18px`, `surface_2` fill, `1.5px` `moss` border, `1.0` opacity. Shows subtext (`em`).
  - Immediate neighbours (`d == 1`): `258 × 44 px`, radius `15px`, `surface_1` fill, `0.92` opacity.
  - Outer rows (`d >= 2`): `226 × 44 px`, radius `15px`, `surface_1` fill, `0.45` opacity.
  - Row vertical gap: `8px` (`DIAL_SPACE_2`).
- **Now Playing (`NP-01`–`NP-05`):**
  - `NP-01`: Full-bleed `360×360` spinning disc (`33.33 RPM`). Inner vinyl progress/seek ring at `r = 38px` (`DIAL_VINYL_SEEK_R`), stroke `3.5px`, knob `r = 4.5px`, mapping `0° = 0%` to `359° = 99%`. Bottom handle is only the `up` chevron at `y = 324..344`.
  - `NP-02`: Dimmed vinyl + `72%` scrim (`DIAL_SCRIM_ALPHA = 184`). Top overline `QUICK ACTIONS` at `y = 46`. Floating transparent Song Title / Artist / Album at `y = 106` (no card box). 4 round action buttons (`52×52px`) on the bottom arc at `r = 118px`, angles `234°` (`heart`), `198°` (`shuffle`), `162°` (`playlist`), `126°` (`lyrics`).
  - `NP-05`: Right-side volume arc only (`145°` → `35°`, `r = 166px`, `w = 6px`) + hero number + `VOLUME` + output text. No inner circles.
- **Control Center (`CC-01`):**
  - `3×2` grid of `66×66px` circular pucks at `left = 60px, top = 98px, width = 240px, row-gap = 22px`.
  - Icons only (`wifi`, `bluetooth`, `mode_*`, `gain_low`/`gain_high`, `usb_charge`, `lock`). Lit = tinted `18%` over `surface_1` with `1.5px` inset ring; Unlit = `surface_1` with `text_3` icon and `1px` `line` border.

## 3. Verification Step
After adding or modifying a screen:
1. Build and run the headless screenshot dump (see `dialos-test-debug` skill) in both `DIAL_MODE_NIGHT` and `DIAL_MODE_DAY`.
2. Inspect the generated PNG using `read_file` and verify it matches [`assets/nordic/fjall-design-system.html`](../../assets/nordic/fjall-design-system.html).
