# DialOS — FiiO Snowsky DISC (Ingenic X2000 MIPS32r2) Cross-Compilation Dockerfile
#
# Builds the native MIPS32 little-endian device binary (`dialos-disc`) using
# `mipsel-linux-gnu-gcc` and packages the signed/checksummed `.fw` update image
# (`dialos-fjall-v1.0.0-disc.fw`).
#
# Usage:
#   docker build -t dialos-mips32 .
#   docker run --rm -v "$(pwd)/build:/out" dialos-mips32

FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc-mipsel-linux-gnu \
    libc6-dev-mipsel-cross \
    file \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY include/ include/
COPY src/ src/
COPY tools/ tools/
COPY tests/ tests/
COPY Makefile Makefile

# 1. Cross-compile FiiO Snowsky DISC target binary (Ingenic X2000 MIPS32r2 LE)
RUN mkdir -p build && \
    mipsel-linux-gnu-gcc -std=c11 -O2 -mips32r2 -EL \
      -Wall -Wextra -Werror -DDIAL_TARGET_DISC -Iinclude \
      src/hal/hal_disc.c \
      src/hal/hal_media.c \
      src/core/dial_polar.c \
      src/core/dial_gestures.c \
      src/core/dial_bt.c \
      src/core/dial_dac.c \
      src/core/dial_app.c \
      src/ui/dial_assets.c \
      src/ui/dial_draw.c \
      src/ui/dial_icons.c \
      src/screens/dial_screens_core_np.c \
      src/screens/dial_screens_lib_fil.c \
      src/screens/dial_screens_mod_set.c \
      src/screens/dial_screens_gam_ovl.c \
      src/main.c \
      -o build/dialos-disc-mipsel -lm -lrt && \
    mipsel-linux-gnu-strip build/dialos-disc-mipsel && \
    file build/dialos-disc-mipsel

# 2. Build host `.fw` packager & generate verified Snowsky DISC `.fw` image
RUN gcc -std=c11 -O2 -Wall -Wextra -Werror tools/pack_disc_fw.c -o build/pack_disc_fw && \
    ./build/pack_disc_fw build/dialos-disc-mipsel build/dialos-fjall-v1.0.0-disc.fw && \
    ./build/pack_disc_fw --verify build/dialos-fjall-v1.0.0-disc.fw

CMD ["sh", "-c", "cp build/dialos-disc-mipsel build/dialos-fjall-v1.0.0-disc.fw /out/ 2>/dev/null || true && ls -lh build/dialos-disc-mipsel build/dialos-fjall-v1.0.0-disc.fw"]
