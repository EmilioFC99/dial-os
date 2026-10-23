# DialOS Makefile — Dual-Target Build, Native Live Simulator & ASan/UBSan Test Harness
CC       ?= clang
CFLAGS   := -std=c11 -Wall -Wextra -Werror -Iinclude
LIBS     := -lm -lz

# Auto-detect optional SDL2; on macOS default to native Cocoa + CoreGraphics window
SDL2_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2 2>/dev/null)
SDL2_LIBS   := $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2 2>/dev/null)
UNAME_S     := $(shell uname -s)

ifneq ($(strip $(SDL2_LIBS)),)
    CFLAGS += -DDIAL_HAS_SDL2 $(SDL2_CFLAGS)
    LIBS   += $(SDL2_LIBS)
else ifeq ($(UNAME_S),Darwin)
    LIBS   += -framework Cocoa -framework CoreGraphics -framework AudioToolbox -framework ImageIO -framework CoreFoundation
endif

ifeq ($(UNAME_S),Linux)
    DISC_LIBS := -lm -lrt
else
    DISC_LIBS := -lm
endif

SAN_FLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
OPT_FLAGS := -O2

BUILD_DIR := build
CORE_SRCS := src/hal/hal_sdl2.c \
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
             src/screens/dial_screens_gam_ovl.c

DISC_SRCS := src/hal/hal_disc.c \
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
             src/screens/dial_screens_gam_ovl.c

SIM_BIN   := $(BUILD_DIR)/dialos-sim
DISC_BIN  := $(BUILD_DIR)/dialos-disc
FW_TOOL   := $(BUILD_DIR)/pack_disc_fw
FW_IMG    := $(BUILD_DIR)/dialos-fjall-v1.0.0-disc.fw
TEST_POL  := $(BUILD_DIR)/test_polar_gestures
TEST_DRW  := $(BUILD_DIR)/test_draw_primitives
TEST_BT   := $(BUILD_DIR)/test_bt_cache
TEST_DAC  := $(BUILD_DIR)/test_dac_relay
TEST_DISC := $(BUILD_DIR)/test_hal_disc

.PHONY: all sim disc-check disc-mipsel fw docker-disc assets test clean

all: sim

assets: tools/bake_fjall_assets.m | $(BUILD_DIR)
	$(CC) -O2 -Wall -Wextra -Werror tools/bake_fjall_assets.m -framework Foundation -framework CoreGraphics -framework CoreText -o $(BUILD_DIR)/bake_fjall_assets
	./$(BUILD_DIR)/bake_fjall_assets

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

sim: $(SIM_BIN)

$(SIM_BIN): $(CORE_SRCS) src/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OPT_FLAGS) $(CORE_SRCS) src/main.c -o $(SIM_BIN) $(LIBS)

$(DISC_BIN): $(DISC_SRCS) src/main.c | $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude -DDIAL_TARGET_DISC $(OPT_FLAGS) $(DISC_SRCS) src/main.c -o $(DISC_BIN) $(DISC_LIBS)

$(FW_TOOL): tools/pack_disc_fw.c | $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -Werror $(OPT_FLAGS) tools/pack_disc_fw.c -o $(FW_TOOL)

$(FW_IMG): $(DISC_BIN) $(FW_TOOL)
	./$(FW_TOOL) $(DISC_BIN) $(FW_IMG)

fw: $(FW_IMG)
	./$(FW_TOOL) --verify $(FW_IMG)

disc-check: $(DISC_BIN) $(TEST_DISC) $(FW_IMG)
	./$(TEST_DISC)
	./$(FW_TOOL) --verify $(FW_IMG)

disc-mipsel: $(DISC_SRCS) src/main.c | $(BUILD_DIR)
	zig cc -target mipsel-linux-musleabihf -mcpu=mips32r2 -mfp64 -mno-odd-spreg -mnan=2008 -static -ffp-contract=off -O2 -s -std=c11 -Wall -Wextra -Werror -Iinclude -DDIAL_TARGET_DISC $(DISC_SRCS) src/main.c -o macos/vendor/mq_ui -lm
	python3 -c 'import struct; f=open("macos/vendor/mq_ui","r+b"); f.seek(36); ef=struct.unpack("<I",f.read(4))[0]; f.seek(36); f.write(struct.pack("<I",ef|0x400)); f.close()'
	cp macos/vendor/mq_ui $(BUILD_DIR)/dialos-disc-mipsel

docker-disc:
	docker build -t dialos-mips32 .
	docker run --rm -v "$(CURDIR)/build:/out" dialos-mips32

$(TEST_POL): $(CORE_SRCS) tests/test_polar_gestures.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_polar_gestures.c -o $(TEST_POL) $(LIBS)

$(TEST_DRW): $(CORE_SRCS) tests/test_draw_primitives.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_draw_primitives.c -o $(TEST_DRW) $(LIBS)

$(TEST_BT): $(CORE_SRCS) tests/test_bt_cache.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_bt_cache.c -o $(TEST_BT) $(LIBS)

$(TEST_DAC): $(CORE_SRCS) tests/test_dac_relay.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_dac_relay.c -o $(TEST_DAC) $(LIBS)

$(TEST_DISC): $(DISC_SRCS) tests/test_hal_disc.c | $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude -DDIAL_TARGET_DISC $(SAN_FLAGS) $(DISC_SRCS) tests/test_hal_disc.c -o $(TEST_DISC) $(DISC_LIBS)

test: $(TEST_POL) $(TEST_DRW) $(TEST_BT) $(TEST_DAC) $(TEST_DISC) $(SIM_BIN) $(FW_IMG)
	./$(TEST_POL)
	./$(TEST_DRW)
	./$(TEST_BT)
	./$(TEST_DAC)
	./$(TEST_DISC)
	./$(FW_TOOL) --verify $(FW_IMG)

clean:
	rm -rf $(BUILD_DIR)
