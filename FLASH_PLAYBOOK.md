# DialOS — FiiO Snowsky DISC Flash & Recovery Playbook (macOS)

This playbook uses a **single CLI tool (`./dialos-mac`)** for first-time USB Mask-ROM installation, 10-second microSD updates, and 100% stock FiiO firmware recovery on macOS.

---

## 1. Pre-Flight Checklist (First-Time USB Mask-ROM Flash)

1. **Ensure your Snowsky DISC is on Stock Firmware V2.57** (or V2.09 / V2.28 / V2.40).
   - The USB writer (`my_write6_dram.bin`) verifies the on-device kernel/recovery SHA-256 against the official FiiO catalogue before touching NAND.
2. **Use a data-capable USB-C cable** plugged into the **USB / DATA** port on the Snowsky DISC.
3. **Battery**: Have at least **40% battery** on the Snowsky DISC so it stays powered during the ~18-minute NAND programming & readback verification pass.

---

## 2. One-Command Full USB Mask-ROM Flash (~18 Minutes)

From the root of the repository (`/Users/emilio.flores/Documents/repos/dial-os`), run:

```bash
./dialos-mac flash
```

### What happens when you run `./dialos-mac flash`:
1. **Builds & verifies automatically (if needed)**:
   - Cross-compiles the static MIPS32r2 `mq_ui` binary (`FP64`, `NAN2008`, `EF_MIPS_NAN2008`).
   - Patches the verified FiiO SquashFS (`build/dialos_nand.bin`, `100,663,296` bytes) while preserving the original FiiO UI at `/usr/bin/mq_ui.stock`.
   - Saves a verified 100% stock recovery backup at `~/Library/Application Support/DialOS/backups/stock_V257_nand.bin`.
2. **Waits interactively for USB Mask-ROM Mode (`a108:eaef`)**:
   - If your Snowsky DISC isn't connected yet, `./dialos-mac flash` waits up to 3 minutes while you put the device into Mask-ROM mode:
     1. **Power OFF** the Snowsky DISC completely.
     2. **Hold down the Volume-Down button**.
     3. While holding **Volume-Down**, **plug the USB-C cable** into your Mac.
     4. **Release Volume-Down** (the screen will stay completely black — this is normal for Ingenic X2000 USB Mask-ROM mode).
3. **Flashes & Verifies with `caffeinate`**:
   - Keeps your Mac awake (`caffeinate -dimsu`) and prints progress every 30 seconds (`[MM:SS / 18:00]`).
   - Performs a full readback verification of NAND blocks `64..831` (`magic=0x4006E006`, `done=0x55555555`, `result=0x600DF10C`).
4. **Boot into DialOS**:
   - Once you see `[SUCCESS] NAND flash & readback verification passed!`, unplug the USB cable and **hold the Power button for ~10 seconds** to exit Mask-ROM mode and boot DialOS.

---

## 3. Future Updates in 10 Seconds (No USB Re-Flash Needed!)

Once DialOS is installed via USB Mask-ROM once, you **never need to wait 18 minutes again** for UI updates. DialOS includes an automatic microSD hot-reload hook on boot:

1. Insert your microSD card into your Mac (mounted at `/Volumes/MUSIC`).
2. Run:
   ```bash
   ./dialos-mac sd-update
   ```
3. Eject the microSD card, insert it into your Snowsky DISC, and power on — DialOS automatically loads `/tmp/sdcard/dialos/mq_ui` from the SD card on boot!

---

## 4. Instant Escape Hatch & Full Stock Recovery

You have **two ways** to return to the original FiiO Snowsky DISC UI at any time:

### Option A: Instant microSD Escape Hatch (5 seconds, no USB cable needed)
Because the original FiiO binary is preserved inside the firmware at `/usr/bin/mq_ui.stock`:
- Create an empty file named `use_stock` inside the `dialos/` folder on your microSD card:
  ```bash
  mkdir -p /Volumes/MUSIC/dialos && touch /Volumes/MUSIC/dialos/use_stock
  ```
- Boot the Snowsky DISC with the SD card inserted — it will immediately boot the original stock FiiO UI. Delete `use_stock` whenever you want to boot back into DialOS.

### Option B: Full 100% Stock NAND Restore over USB Mask-ROM
To restore the untouched factory SquashFS partition over USB Mask-ROM:
```bash
./dialos-mac restore-stock
```
