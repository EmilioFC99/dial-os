#!/usr/bin/env bash
# ==============================================================================
# DialOS — Snowsky DISC SSH Hot-Swap & Local Virtual Device Installer
# ==============================================================================
# Usage:
#   1. Local Virtual Snowsky DISC Simulation + Live Interactive UI:
#      ./tools/deploy_disc_ssh.sh --local [--headless-check]
#
#   2. Real Snowsky DISC Hardware RAM Hot-Swap over Wi-Fi SSH (Zero Flash Risk):
#      ./tools/deploy_disc_ssh.sh <DISC_IP>
#
#   3. Real Snowsky DISC Persistent Dual-Boot Install (/usr/data/mq_ui):
#      ./tools/deploy_disc_ssh.sh <DISC_IP> --persist
# ==============================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

MODE="${1:---local}"
SECOND_ARG="${2:-}"

if [[ "$MODE" == "--local" ]]; then
    echo "=================================================================="
    echo " DialOS v1.0.0 — Local Virtual Snowsky DISC Device & Live UI"
    echo "=================================================================="

    echo "[1/4] Building DialOS Simulator, Snowsky DISC HAL & .fw image..."
    make all fw

    VDISC="$REPO_ROOT/build/virtual-disc"
    mkdir -p "$VDISC/usr/data" "$VDISC/dev" "$VDISC/tmp" "$VDISC/etc"

    echo "[2/4] Provisioning virtual Snowsky DISC rootfs at build/virtual-disc/..."
    # Pre-allocate 360x1080x4 (1,555,200 bytes) triple-buffered 32bpp XRGB8888 ingenicfb
    dd if=/dev/zero of="$VDISC/dev/fb0" bs=518400 count=3 status=none
    : > "$VDISC/tmp/mq_player.log"

    # Install Snowsky DISC UI binary as /usr/data/mq_ui + OTA .fw image
    cp "$REPO_ROOT/build/dialos-disc" "$VDISC/usr/data/mq_ui"
    chmod +x "$VDISC/usr/data/mq_ui"
    cp "$REPO_ROOT/build/dialos-fjall-v1.0.0-disc.fw" "$VDISC/usr/data/dialos-fjall-v1.0.0-disc.fw"

    cat > "$VDISC/etc/diskos_manifest" <<EOF
DEVICE=FiiO Snowsky DISC (Ingenic X2000 / Dual CS43131)
UI_BINARY=/usr/data/mq_ui
PRCTL_NAME=mq_ui
FB_DEVICE=/dev/fb0 (360x360 visible, 360x1080 virtual, 32bpp XRGB8888, 180-deg rotated)
KEYS_DEVICE=/dev/input/event0 (x2000_key: VOLUP=114, VOLDOWN=115, POWER=116, PLAY=164)
TOUCH_DEVICE=/dev/input/event1 (cst816t MT Type-B, 180-deg inverted)
IPC_QUEUE=/player (FiiO 12-char hex wire frames: 0201/0647/0649/0653/0715)
EOF

    echo "[3/4] Running virtual /dev/fb0 180-deg XRGB8888 hardware smoke check..."
    DIAL_VIRTUAL_FB0="$VDISC/dev/fb0" \
    DIAL_VIRTUAL_MQ_LOG="$VDISC/tmp/mq_player.log" \
        "$VDISC/usr/data/mq_ui" --headless --screen NP-01 --mode night --out "$VDISC/tmp/np01_rgb565.raw"

    echo "      Installed UI Binary : $VDISC/usr/data/mq_ui"
    echo "      Installed OTA Image : $VDISC/usr/data/dialos-fjall-v1.0.0-disc.fw"
    echo "      Virtual ingenicfb   : $VDISC/dev/fb0 (1,555,200 bytes, 32bpp XRGB8888)"
    echo "      Virtual /player log : $VDISC/tmp/mq_player.log"

    if [[ "$SECOND_ARG" == "--headless-check" ]]; then
        echo "[4/4] Headless virtual device verification complete."
        exit 0
    fi

    echo "[4/4] Launching interactive DialOS Live UI Simulator..."
    echo "------------------------------------------------------------------"
    echo " Live Controls:"
    echo "   - Drag inner vinyl ring (r=38px) : Real-time circular seek"
    echo "   - Trace outer rim (r=148..180px) : 24-detent rotary scroll / volume / EQ"
    echo "   - Single-tap center on NP-01     : Open NP-02 Quick Actions arc"
    echo "   - Double-tap center on NP-01     : Play / Pause toggle"
    echo "   - Pull down from top edge (y<28) : Open CC-01 Control Center"
    echo "   - Hold (450ms) Wi-Fi/BT/Mode puck: Jump to SET-06 / SET-07 / MOD-00"
    echo "   - Keys: [Space] Play/Pause | [Up/Down] Volume | [Left/Right] Cycle 38 Screens"
    echo "           [D] Polar Night / Birch Day | [Esc] Lock Screen | [S] Screenshot"
    echo "------------------------------------------------------------------"

    DIAL_VIRTUAL_FB0="$VDISC/dev/fb0" \
    DIAL_VIRTUAL_MQ_LOG="$VDISC/tmp/mq_player.log" \
        exec "$REPO_ROOT/build/dialos-sim"
fi

# ==============================================================================
# Real Snowsky DISC Hardware Deployment via Wi-Fi SSH
# ==============================================================================
DISC_IP="$MODE"
MIPSEL_BIN="$REPO_ROOT/build/dialos-disc-mipsel"

if [[ ! -f "$MIPSEL_BIN" ]]; then
    echo "[DialOS SSH] Cross-compiling static MIPS32r2 binary via Docker..."
    docker build -t dialos-mipsel-builder "$REPO_ROOT"
    mkdir -p "$REPO_ROOT/build"
    docker run --rm -v "$REPO_ROOT/build:/out" dialos-mipsel-builder \
        sh -c "cp build/dialos-disc-mipsel build/mq_ui build/*.fw /out/"
fi

if [[ "$SECOND_ARG" == "--persist" ]]; then
    echo "[DialOS SSH] Installing persistent DialOS binary to root@${DISC_IP}:/usr/data/mq_ui..."
    scp "$MIPSEL_BIN" "root@${DISC_IP}:/usr/data/mq_ui"
    ssh "root@${DISC_IP}" "chmod +x /usr/data/mq_ui && killall mq_ui || true; /usr/data/mq_ui </dev/null >/dev/null 2>&1 &"
    echo "[DialOS SSH] Persistent install complete and launched on Snowsky DISC (${DISC_IP})."
else
    echo "[DialOS SSH] Hot-swapping DialOS in RAM on root@${DISC_IP}:/tmp/mq_ui (reboot restores stock UI)..."
    scp "$MIPSEL_BIN" "root@${DISC_IP}:/tmp/mq_ui"
    ssh "root@${DISC_IP}" "chmod +x /tmp/mq_ui && killall mq_ui || true; /tmp/mq_ui </dev/null >/tmp/dialos.log 2>&1 &"
    echo "[DialOS SSH] Live RAM hot-swap active on Snowsky DISC (${DISC_IP})."
fi
