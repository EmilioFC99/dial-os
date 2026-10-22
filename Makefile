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
    LIBS   += -framework Cocoa -framework CoreGraphics
endif

SAN_FLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
OPT_FLAGS := -O2

BUILD_DIR := build
CORE_SRCS := src/hal/hal_sdl2.c \
             src/core/dial_polar.c \
             src/core/dial_gestures.c \
             src/core/dial_app.c \
             src/ui/dial_assets.c \
             src/ui/dial_draw.c \
             src/ui/dial_icons.c \
             src/screens/dial_screens_core_np.c \
             src/screens/dial_screens_lib_fil.c

SIM_BIN   := $(BUILD_DIR)/dialos-sim
TEST_POL  := $(BUILD_DIR)/test_polar_gestures
TEST_DRW  := $(BUILD_DIR)/test_draw_primitives
TEST_V030 := $(BUILD_DIR)/test_screens_v030
TEST_V040 := $(BUILD_DIR)/test_screens_v040
TEST_UI   := $(BUILD_DIR)/test_ui_interaction

.PHONY: all sim assets test clean

all: sim

assets: tools/bake_fjall_assets.m | $(BUILD_DIR)
	$(CC) -O2 -Wall -Wextra -Werror tools/bake_fjall_assets.m -framework Foundation -framework CoreGraphics -framework CoreText -o $(BUILD_DIR)/bake_fjall_assets
	./$(BUILD_DIR)/bake_fjall_assets

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

sim: $(SIM_BIN)

$(SIM_BIN): $(CORE_SRCS) src/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OPT_FLAGS) $(CORE_SRCS) src/main.c -o $(SIM_BIN) $(LIBS)

$(TEST_POL): $(CORE_SRCS) tests/test_polar_gestures.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_polar_gestures.c -o $(TEST_POL) $(LIBS)

$(TEST_DRW): $(CORE_SRCS) tests/test_draw_primitives.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_draw_primitives.c -o $(TEST_DRW) $(LIBS)

$(TEST_V030): $(CORE_SRCS) tests/test_screens_v030.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_screens_v030.c -o $(TEST_V030) $(LIBS)

$(TEST_V040): $(CORE_SRCS) tests/test_screens_v040.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_screens_v040.c -o $(TEST_V040) $(LIBS)

$(TEST_UI): $(CORE_SRCS) tests/test_ui_interaction.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) tests/test_ui_interaction.c -o $(TEST_UI) $(LIBS)

test: $(TEST_POL) $(TEST_DRW) $(TEST_V030) $(TEST_V040) $(TEST_UI) $(SIM_BIN)
	./$(TEST_POL)
	./$(TEST_DRW)
	./$(TEST_V030)
	./$(TEST_V040)
	./$(TEST_UI)
	./$(SIM_BIN) --headless --screen SYS-00 --mode night --out /tmp/dialos_v030_sys00.png
	./$(SIM_BIN) --headless --screen LCK-01 --mode night --out /tmp/dialos_v030_lck01.png
	./$(SIM_BIN) --headless --screen LCK-02 --mode night --out /tmp/dialos_v030_lck02.png
	./$(SIM_BIN) --headless --screen HOM-01 --mode night --out /tmp/dialos_v030_hom01.png
	./$(SIM_BIN) --headless --screen HOM-02 --mode day   --out /tmp/dialos_v030_hom02_day.png
	./$(SIM_BIN) --headless --screen HOM-03 --mode night --out /tmp/dialos_v030_hom03.png
	./$(SIM_BIN) --headless --screen CC-01  --mode night --out /tmp/dialos_v030_cc01.png
	./$(SIM_BIN) --headless --screen CC-02  --mode night --out /tmp/dialos_v030_cc02.png
	./$(SIM_BIN) --headless --screen NP-01  --mode night --out /tmp/dialos_v030_np01.png
	./$(SIM_BIN) --headless --screen NP-02  --mode night --out /tmp/dialos_v030_np02.png
	./$(SIM_BIN) --headless --screen NP-03  --mode night --out /tmp/dialos_v030_np03.png
	./$(SIM_BIN) --headless --screen NP-04  --mode night --out /tmp/dialos_v030_np04.png
	./$(SIM_BIN) --headless --screen NP-05  --mode night --out /tmp/dialos_v030_np05.png
	./$(SIM_BIN) --headless --screen LIB-01 --mode night --out /tmp/dialos_v040_lib01.png
	./$(SIM_BIN) --headless --screen LIB-02 --mode night --out /tmp/dialos_v040_lib02.png
	./$(SIM_BIN) --headless --screen LIB-03 --mode night --out /tmp/dialos_v040_lib03.png
	./$(SIM_BIN) --headless --screen LIB-04 --mode night --out /tmp/dialos_v040_lib04.png
	./$(SIM_BIN) --headless --screen FIL-01 --mode night --out /tmp/dialos_v040_fil01.png
	./$(SIM_BIN) --headless --screen FIL-02 --mode night --out /tmp/dialos_v040_fil02.png
	./$(SIM_BIN) --headless --screen FIL-03 --mode night --out /tmp/dialos_v040_fil03.png
	./$(SIM_BIN) --headless --screen FIL-04 --mode day   --out /tmp/dialos_v040_fil04_day.png

clean:
	rm -rf $(BUILD_DIR)
