CC        := cc
CSTD      := -std=c11
SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build
TARGET    := Ray_anim

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)
SDL_PREFIX := $(shell sdl2-config --prefix 2>/dev/null)
SDL_EXTRA_INC := $(if $(SDL_PREFIX),-I$(SDL_PREFIX)/include,)

CFLAGS  := $(CSTD) -Wall -Wextra -Wpedantic -g $(SDL_CFLAGS) $(SDL_EXTRA_INC) -I$(INC_DIR) -DMAIN_DRIVER
LDFLAGS := $(SDL_LIBS) -lSDL2_ttf -ljson-c -lm

CFLAGS_RELEASE := $(CSTD) -Wall -Wextra -Wpedantic -O3 $(SDL_CFLAGS) $(SDL_EXTRA_INC) -I$(INC_DIR) -DMAIN_DRIVER -DNDEBUG \
	-ffast-math -fno-math-errno -march=native
REL_BUILD_DIR := build_release
REL_TARGET := Ray_anim_release

VIDEO_FRAMES_DIR ?= Animations/default
VIDEO_OUTPUT ?= Animations/Vids/output.mp4
VIDEO_FPS ?= 30

SRC := $(shell find $(SRC_DIR) -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

.PHONY: all clean run debug format video release relrun

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

debug: CFLAGS += -O0 -g3
debug: clean all

format:
	@command -v clang-format >/dev/null 2>&1 && clang-format -i $(SRC) $(shell find $(INC_DIR) -name '*.h') || echo "clang-format not found"

video:
	@python3 tools/make_video.py --frames "$(VIDEO_FRAMES_DIR)" --output "$(VIDEO_OUTPUT)" --fps $(VIDEO_FPS)

release:
	$(MAKE) BUILD_DIR=$(REL_BUILD_DIR) TARGET=$(REL_TARGET) CFLAGS="$(CFLAGS_RELEASE)" LDFLAGS="$(LDFLAGS)" all

relrun: release
	./$(REL_TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(REL_BUILD_DIR) $(REL_TARGET)

-include $(DEP)
