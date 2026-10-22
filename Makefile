# DialOS Makefile — Dual-Target Build & ASan/UBSan Verification Harness
CC       ?= clang
CFLAGS   := -std=c11 -Wall -Wextra -Werror -Iinclude
LIBS     := -lm -lz

# Auto-detect optional SDL2 for interactive desktop window
SDL2_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2 2>/dev/null)
SDL2_LIBS   := $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2 2>/dev/null)
ifneq ($(strip $(SDL2_LIBS)),)
    CFLAGS += -DDIAL_HAS_SDL2 $(SDL2_CFLAGS)
    LIBS   += $(SDL2_LIBS)
endif

SAN_FLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
OPT_FLAGS := -O2

BUILD_DIR := build
CORE_SRCS := src/hal/hal_sdl2.c \
             src/core/dial_polar.c \
             src/core/dial_gestures.c \
             src/ui/dial_draw.c \
             src/ui/dial_icons.c

SIM_BIN   := $(BUILD_DIR)/dialos-sim
TEST_POL  := $(BUILD_DIR)/test_polar_gestures
TEST_DRW  := $(BUILD_DIR)/test_draw_primitives

.PHONY: all sim test clean

all: sim

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

sim: $(SIM_BIN)

$(SIM_BIN): $(CORE_SRCS) src/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OPT_FLAGS) $(CORE_SRCS) src/main.c -o $(SIM_BIN) $(LIBS)

$(TEST_POL): $(CORE_SRCS) tests/test_polar_gestures.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_polar_gestures.c -o $(TEST_POL) $(LIBS)

$(TEST_DRW): $(CORE_SRCS) tests/test_draw_primitives.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_draw_primitives.c -o $(TEST_DRW) $(LIBS)

test: $(TEST_POL) $(TEST_DRW) $(SIM_BIN)
	./$(TEST_POL)
	./$(TEST_DRW)
	./$(SIM_BIN) --headless --screen NP-01 --mode night --out /tmp/dialos_m1_np01_night.png
	./$(SIM_BIN) --headless --screen LIB-01 --mode day --out /tmp/dialos_m1_lib01_day.png
	./$(SIM_BIN) --headless --screen TEST-ICONS --mode night --out /tmp/dialos_m1_icons.png

clean:
	rm -rf $(BUILD_DIR)
