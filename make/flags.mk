SDL_CFLAGS :=
SDL_LIBS :=
SDL_PREFIX :=
SDL_EXTRA_INC :=
JSON_CFLAGS :=
JSON_LIBS :=
PNG_CFLAGS :=
PNG_LIBS :=

VULKAN_CFLAGS :=
VULKAN_LIBS :=

ifneq ($(UNAME_S),Darwin)
SDL_CONFIG := $(shell command -v sdl2-config 2>/dev/null)
SDL_CFLAGS := $(shell $(SDL_CONFIG) --cflags 2>/dev/null)
SDL_LIBS := $(shell $(SDL_CONFIG) --libs 2>/dev/null)
SDL_PREFIX := $(shell $(SDL_CONFIG) --prefix 2>/dev/null)
SDL_EXTRA_INC := $(if $(SDL_PREFIX),-I$(SDL_PREFIX)/include,)
JSON_CFLAGS := $(shell pkg-config --cflags json-c 2>/dev/null)
JSON_LIBS := $(shell pkg-config --libs json-c 2>/dev/null)
ifeq ($(strip $(JSON_LIBS)),)
JSON_LIBS := -ljson-c
endif
PNG_CFLAGS := $(shell pkg-config --cflags libpng 2>/dev/null)
PNG_LIBS := $(shell pkg-config --libs libpng 2>/dev/null)
ifeq ($(strip $(PNG_LIBS)),)
PNG_LIBS := -lpng
endif
endif

ifeq ($(UNAME_S),Linux)
    VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
    VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
    ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
        VULKAN_CFLAGS := -I/usr/include
        VULKAN_LIBS := -lvulkan
    endif
endif

ifeq ($(UNAME_S),Darwin)
    SDL_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
    SDL_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs sdl2 2>/dev/null)
    SDL_PREFIX := $(TARGET_HOMEBREW_PREFIX)
    SDL_EXTRA_INC := $(if $(SDL_PREFIX),-I$(SDL_PREFIX)/include,)
    JSON_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags json-c 2>/dev/null)
    JSON_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs json-c 2>/dev/null)
    ifeq ($(strip $(JSON_LIBS)),)
        JSON_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -ljson-c
    endif
    ifeq ($(strip $(JSON_CFLAGS)),)
        JSON_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
    endif
    PNG_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags libpng 2>/dev/null)
    PNG_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs libpng 2>/dev/null)
    ifeq ($(strip $(PNG_CFLAGS)),)
        ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include/png.h),)
            PNG_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
        else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include/png.h),)
            PNG_CFLAGS := -I$(TARGET_ALT_HOMEBREW_PREFIX)/include
        endif
    endif
    ifeq ($(strip $(PNG_LIBS)),)
        ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libpng.dylib),)
            PNG_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lpng
        else ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/lib/libpng16.dylib),)
            PNG_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lpng16
        else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libpng.dylib),)
            PNG_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -lpng
        else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/lib/libpng16.dylib),)
            PNG_LIBS := -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib -lpng16
        endif
    endif
    VULKAN_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags vulkan 2>/dev/null)
    VULKAN_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs vulkan 2>/dev/null)
    ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
        VULKAN_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
        VULKAN_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lvulkan
    endif
    VULKAN_LIBS += -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo
    CFLAGS += -DVK_USE_PLATFORM_METAL_EXT
endif

CFLAGS  := $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g $(ARCH_FLAGS) $(SDL_CFLAGS) $(SDL_EXTRA_INC) $(JSON_CFLAGS) $(PNG_CFLAGS) -I$(INC_DIR) -Isrc -Isrc/tools -Isrc/tools/ShapeLib -DMAIN_DRIVER
LDFLAGS := $(ARCH_FLAGS) $(SDL_LIBS) -lSDL2_ttf $(JSON_LIBS) $(PNG_LIBS) -lm

CFLAGS_RELEASE := $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -O3 $(ARCH_FLAGS) $(SDL_CFLAGS) $(SDL_EXTRA_INC) $(JSON_CFLAGS) $(PNG_CFLAGS) -I$(INC_DIR) -Isrc -Isrc/tools -Isrc/tools/ShapeLib -DMAIN_DRIVER -DNDEBUG \
	-ffast-math -fno-math-errno -march=native
ifeq ($(UNAME_S),Darwin)
CFLAGS += -DVK_USE_PLATFORM_METAL_EXT
CFLAGS_RELEASE += -DVK_USE_PLATFORM_METAL_EXT
endif

CFLAGS += $(TIMER_HUD_INCLUDE) -I$(VK_RENDERER_DIR)/include $(VULKAN_CFLAGS) \
	-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_QUEUE_DIR)/include -I$(CORE_TIME_DIR)/include -I$(CORE_WORKERS_DIR)/include -I$(CORE_SIM_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_AUTHORED_TEXTURE_DIR)/include -I$(CORE_SCENE_COMPILE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_TRACE_DIR)/include -I$(CORE_SPACE_DIR)/include -I$(CORE_PANE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(KIT_RENDER_DIR)/include -I$(KIT_PANE_DIR)/include -I$(KIT_VIZ_DIR)/include -I$(KIT_RUNTIME_DIAG_DIR)/include -I$(KIT_WORKSPACE_AUTHORING_DIR)/include \
	-I$(CORE_HEADLESS_JOB_DIR)/include \
	-DKIT_RENDER_ENABLE_VK_BACKEND=0 \
	-DUSE_VULKAN=1 -DVK_RENDERER_SHADER_ROOT=\"$(abspath $(VK_RENDERER_DIR))\" \
	-include $(VK_RENDERER_DIR)/include/vk_renderer_sdl.h
LDFLAGS += $(VULKAN_LIBS)
CFLAGS_RELEASE += $(TIMER_HUD_INCLUDE) -I$(VK_RENDERER_DIR)/include $(VULKAN_CFLAGS) \
	-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_QUEUE_DIR)/include -I$(CORE_TIME_DIR)/include -I$(CORE_WORKERS_DIR)/include -I$(CORE_SIM_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_AUTHORED_TEXTURE_DIR)/include -I$(CORE_SCENE_COMPILE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_TRACE_DIR)/include -I$(CORE_SPACE_DIR)/include -I$(CORE_PANE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(KIT_RENDER_DIR)/include -I$(KIT_PANE_DIR)/include -I$(KIT_VIZ_DIR)/include -I$(KIT_RUNTIME_DIAG_DIR)/include -I$(KIT_WORKSPACE_AUTHORING_DIR)/include \
	-I$(CORE_HEADLESS_JOB_DIR)/include \
	-DKIT_RENDER_ENABLE_VK_BACKEND=0 \
	-DUSE_VULKAN=1 -DVK_RENDERER_SHADER_ROOT=\"$(abspath $(VK_RENDERER_DIR))\" \
	-include $(VK_RENDERER_DIR)/include/vk_renderer_sdl.h
