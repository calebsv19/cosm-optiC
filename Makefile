CC       := cc
CSTD     := -std=c11
SRC_DIR  := src
INC_DIR  := include
BUILD_DIR:= build
TARGET   := Ray_anim

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

CFLAGS  := $(CSTD) -Wall -Wextra -Wpedantic -g $(SDL_CFLAGS) -I$(INC_DIR) -DMAIN_DRIVER
LDFLAGS := $(SDL_LIBS) -lSDL2_ttf -ljson-c -lm

SRC := $(shell find $(SRC_DIR) -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

.PHONY: all clean run debug format

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

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEP)
