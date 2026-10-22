---
name: snowsky-disc-hal
description: >-
  Use this skill when working on the FiiO Snowsky DISC Hardware Abstraction Layer (src/hal/),
  POSIX message queues (/player and /ui), CS43131 DAC & relay control, fast-cache Bluetooth
  reconnection, or cross-referencing reverse-engineered hardware behavior from b0hemia/diskos.
---

# Snowsky DISC HAL & IPC Skill

Use this guide when implementing or debugging hardware integration for the **FiiO Snowsky DISC** (Ingenic X2000 MIPS32, dual CS43131 DACs, Linux framebuffer `/dev/fb0`).

## 1. Architecture: Dual-Backend HAL (`src/hal/`)
Maintain a clean C interface in `include/dial_hal.h` with two implementations:
- `src/hal/hal_sdl2.c` (macOS / Linux desktop simulator):
  - Renders the `360×360` framebuffer to an SDL2 window with circular bezel mask.
  - Simulates `/player` and `/ui` POSIX message queues in-memory with mock library/tracks, battery, and Bluetooth peers.
- `src/hal/hal_disc.c` (Ingenic X2000 MIPS32 target):
  - Maps `/dev/fb0` (`360×360` RGB565/ARGB8888 with DMA double-buffering).
  - Polls `/dev/input/event0` (capacitive touch `120 Hz`) and `/dev/input/event1` (physical keys: `KEY_POWER`, `KEY_PLAYPAUSE`, `KEY_VOLUP`, `KEY_VOLDOWN`).
  - Connects to FiiO's backend `player` daemon over POSIX message queues (`/player` and `/ui`) using opcodes in [`include/dial_ipc.h`](../../include/dial_ipc.h).

## 2. Reference Lookup in `b0hemia/diskos`
When you need to verify an undocumented FiiO `/player` message format, sysfs path, or `.fw` update packaging script:
1. Clone or inspect `https://github.com/b0hemia/diskos` into `/tmp/diskos-ref` (outside the repo tree) to check its IPC command strings and device node paths.
2. **Never copy `diskos` UI/theme code** into `dial-os`; only extract hardware constants, `ioctl` parameters, or `/player` message payload formats into `include/dial_ipc.h` and `src/hal/hal_disc.c`.

## 3. Fast-Cache Bluetooth Reconnect & Codec Priority Ladder (`SET-07`)
Implement Bluetooth management in `src/core/dial_bt.c` using this exact sequence:
1. **Persistent MRU Cache (`bt_peer_cache_t`):**
   - Stored at `/mnt/sdcard/.dialos/bt_cache.bin` (or NVS), holding up to 8 paired devices ordered by `last_connected_ts` descending.
   - `peer_cache[0]` is always the **Default (Last Connected)** device.
   - Fields per entry: `uint8_t bd_addr[6]`, `char name[64]`, `uint16_t clock_offset`, `uint8_t page_scan_mode`, `uint8_t remote_seid`, `dial_bt_codec_t preferred_codec`, `dial_ldac_quality_t ldac_qual`, `uint8_t saved_volume`.
2. **Instant Reconnect on BT Enable (`< 400 ms`):**
   - Do **not** start `HCI_Inquiry` scan when Bluetooth turns on.
   - Immediately issue a Direct Page (`HCI_Create_Connection`) to `peer_cache[0].bd_addr` using its cached `clock_offset` and `page_scan_mode`.
   - Only if `peer_cache[0]` times out (`1800 ms`) should the stack try `peer_cache[1]` or fall back to background discovery scan.
3. **Swift Codec Negotiation (`LDAC 990` → `SBC`):**
   - If `peer_cache[0].remote_seid` is valid, immediately send `AVDTP_SET_CONFIGURATION` with the cached `remote_seid` and `preferred_codec` (`DIAL_CMD_BT_CODEC` / `DIAL_CMD_BT_LDAC_QUAL`).
   - Otherwise, evaluate remote SEPs in a single pass against the priority ladder:
     `LDAC (990 kbps) -> LDAC (660/330 kbps on RSSI < -78 dBm) -> aptX HD -> aptX -> AAC -> SBC`.
   - Restore `peer_cache[0].saved_volume` prior to unmuting audio.

## 4. CS43131 Anti-Pop & Gain Relay Safety
Refer to Section 4 of [`docs/DIAL_OS_SYSTEM_REQUIREMENTS.md`](../../docs/DIAL_OS_SYSTEM_REQUIREMENTS.md):
- Before toggling the `GPIO_PD14` analogue gain relay (`Low` ↔ `High` in `CC-01` or `SET-02`), ramp the CS43131 digital volume to mute over `8 ms`, switch the relay, wait `12 ms` for contact bounce to settle, and ramp volume back up over `8 ms`.
