CLANG_CC ?= cc
CODEWORK_GIT_COMMON_DIR := $(shell git rev-parse --path-format=absolute --git-common-dir 2>/dev/null)
CODEWORK_WORKSPACE_ROOT := $(if $(CODEWORK_GIT_COMMON_DIR),$(abspath $(CODEWORK_GIT_COMMON_DIR)/../..),$(abspath ..))
FISICS_BIN ?= $(CODEWORK_WORKSPACE_ROOT)/fisiCs/fisics
FISICS_INCLUDE_DIR ?= $(if $(wildcard $(CODEWORK_WORKSPACE_ROOT)/fisiCs/include/fisics/extensions.h),$(CODEWORK_WORKSPACE_ROOT)/fisiCs/include,$(CURDIR)/third_party/fisics/include)
FISICS_OVERLAY ?= physics-units
FISICS_ENV ?= FISICS_MAX_PROCS=0
BUILD_TOOLCHAIN ?= clang
PACKAGE_TOOLCHAIN ?= $(BUILD_TOOLCHAIN)
CC := $(CLANG_CC)
ifeq ($(BUILD_TOOLCHAIN),fisics)
CC := $(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY)
endif
CSTD      := -std=c11
UNAME_S   := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
CSTD      := -std=c2x
endif
SRC_DIR   := src
INC_DIR   := include
RELEASE_ROOT ?= build/release
BUILD_DIR_BASE ?= $(if $(filter build/release,$(RELEASE_ROOT)),build,$(RELEASE_ROOT).work/build)
TARGET    := Ray_anim
.DEFAULT_GOAL := all
DIST_DIR ?= $(if $(filter build,$(BUILD_DIR_BASE)),dist,$(BUILD_DIR_BASE)/dist)
PACKAGE_APP_NAME := optiC.app
PACKAGE_APP_DIR := $(DIST_DIR)/$(PACKAGE_APP_NAME)
PACKAGE_CONTENTS_DIR := $(PACKAGE_APP_DIR)/Contents
PACKAGE_MACOS_DIR := $(PACKAGE_CONTENTS_DIR)/MacOS
PACKAGE_RESOURCES_DIR := $(PACKAGE_CONTENTS_DIR)/Resources
PACKAGE_TOOLS_DIR := $(PACKAGE_RESOURCES_DIR)/bin
PACKAGE_FRAMEWORKS_DIR := $(PACKAGE_CONTENTS_DIR)/Frameworks
PACKAGE_INFO_PLIST_SRC := tools/packaging/macos/Info.plist
PACKAGE_LAUNCHER_SRC := tools/packaging/macos/raytracing-launcher
PACKAGE_DYLIB_BUNDLER := tools/packaging/macos/bundle-dylibs.sh
PACKAGE_LOCAL_ICON_DIR := tools/packaging/macos/local_app_icon
PACKAGE_APP_ICON_NAME := AppIcon
PACKAGE_APP_ICON_FILE := $(PACKAGE_APP_ICON_NAME).icns
PACKAGE_DEFAULT_ICON_SRC := tools/packaging/macos/$(PACKAGE_APP_ICON_FILE)
PACKAGE_APP_ICON_SRC ?= $(if $(wildcard $(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_FILE)),$(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_FILE),$(PACKAGE_DEFAULT_ICON_SRC))
PACKAGE_APP_ICONSET_SRC ?= $(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_NAME).iconset
PACKAGE_BUNDLED_ICON_PATH := $(PACKAGE_RESOURCES_DIR)/$(PACKAGE_APP_ICON_FILE)
PACKAGE_FFMPEG_SRC ?=
PACKAGE_REQUIRE_FFMPEG ?= 0
DESKTOP_APP_DIR ?= $(HOME)/Desktop/$(PACKAGE_APP_NAME)
PACKAGE_ADHOC_SIGN_IDENTITY ?= -

ifneq ($(filter $(BUILD_TOOLCHAIN),clang fisics),$(BUILD_TOOLCHAIN))
$(error Unsupported BUILD_TOOLCHAIN '$(BUILD_TOOLCHAIN)'; expected clang or fisics)
endif

ifneq ($(filter $(PACKAGE_TOOLCHAIN),clang fisics),$(PACKAGE_TOOLCHAIN))
$(error Unsupported PACKAGE_TOOLCHAIN '$(PACKAGE_TOOLCHAIN)'; expected clang or fisics)
endif

define program_build_dir_for
$(BUILD_DIR_BASE)/toolchains/$(1)/$(TARGET_ARCH)
endef

define program_bin_for
$(call program_build_dir_for,$(1))/$(TARGET)
endef

# RL0 release contract.
RELEASE_VERSION_FILE ?= VERSION
RELEASE_VERSION ?= $(strip $(shell cat "$(RELEASE_VERSION_FILE)" 2>/dev/null))
ifeq ($(RELEASE_VERSION),)
RELEASE_VERSION := 0.1.0
endif
WORKER_VERSION_FILE ?= WORKER_VERSION
WORKER_VERSION ?= $(strip $(shell cat "$(WORKER_VERSION_FILE)" 2>/dev/null))
ifeq ($(WORKER_VERSION),)
$(error Missing canonical worker version in $(WORKER_VERSION_FILE))
endif
WORKER_VERSION_GENERATED_DIR ?= $(BUILD_DIR_BASE)/generated
WORKER_VERSION_HEADER := $(WORKER_VERSION_GENERATED_DIR)/app/ray_tracing_worker_version.h
RELEASE_CHANNEL ?= stable
RELEASE_PRODUCT_NAME := optiC
RELEASE_PROGRAM_KEY := ray_tracing
RELEASE_BUNDLE_ID := com.cosm.optic
PACKAGE_PROFILE ?= standard
PACKAGE_BUNDLE_ID ?= $(RELEASE_BUNDLE_ID)
PACKAGE_DISPLAY_NAME ?= $(RELEASE_PRODUCT_NAME)
PACKAGE_RUNTIME_NAMESPACE ?= RayTracing
PACKAGE_LOG_NAMESPACE ?= RayTracing
PACKAGE_BUILD_LABEL ?= $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)
PACKAGE_EMBED_BUILD_IDENTITY ?= 0
PACKAGE_SOURCE_BRANCH ?= unknown
PACKAGE_SOURCE_COMMIT ?= unknown
PACKAGE_SOURCE_DIRTY ?= unknown
PACKAGE_SOURCE_FINGERPRINT ?= unknown
PACKAGE_SOURCE_FINGERPRINT_TOOL := tools/packaging/macos/source-state-fingerprint.py
PACKAGE_BUILD_IDENTITY_TOOL := tools/packaging/macos/write-build-identity.py
MAIN_EDIT_PACKAGE_PROFILE := main-edit
MAIN_EDIT_PACKAGE_APP_NAME := optiC Main Edit.app
MAIN_EDIT_PACKAGE_DISPLAY_NAME := optiC Main Edit
MAIN_EDIT_PACKAGE_BUNDLE_ID := com.cosm.optic.main-edit
MAIN_EDIT_PACKAGE_RUNTIME_NAMESPACE := RayTracing-Main-Edit
MAIN_EDIT_PACKAGE_LOG_NAMESPACE := RayTracing-Main-Edit
MAIN_EDIT_PACKAGE_DIST_DIR := dist/dev/main-edit
MAIN_EDIT_PACKAGE_APP_DIR := $(MAIN_EDIT_PACKAGE_DIST_DIR)/$(MAIN_EDIT_PACKAGE_APP_NAME)
MAIN_EDIT_DESKTOP_APP_DIR ?= $(HOME)/Desktop/$(MAIN_EDIT_PACKAGE_APP_NAME)
RELEASE_ARTIFACT_BASENAME = $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-$(RELEASE_PLATFORM)-$(RELEASE_ARCH)-$(RELEASE_CHANNEL)
RELEASE_DIR := $(RELEASE_ROOT)
RELEASE_APP_ZIP = $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).zip
RELEASE_MANIFEST = $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).manifest.txt
RELEASE_ARCHIVE_STAGING = $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).archive
RELEASE_CODESIGN_IDENTITY ?= $(if $(strip $(APPLE_SIGN_IDENTITY)),$(APPLE_SIGN_IDENTITY),$(PACKAGE_ADHOC_SIGN_IDENTITY))
APPLE_SIGN_IDENTITY ?=
APPLE_NOTARY_PROFILE ?=
APPLE_TEAM_ID ?=
STAPLE_MAX_ATTEMPTS ?= 6
STAPLE_RETRY_DELAY_SEC ?= 15

# Fixed preparation gates refuse canonical output defaults before any recipe.
# Existing ordinary app builds retain their defaults; selected isolated roots
# also contain generated headers, packaging and integration-fixture outputs.
RAY_TRACING_TEST_WORK_ROOT ?= $(BUILD_DIR_BASE)/agent_runs/ray_tracing
export BUILD_DIR_BASE WORKER_VERSION_GENERATED_DIR RAY_TRACING_TEST_WORK_ROOT
RAY_TRACING_PREPARATION_GOALS := run-headless-smoke package-desktop-self-test release-output-root-conformance
ifneq ($(filter $(RAY_TRACING_PREPARATION_GOALS),$(MAKECMDGOALS)),)
ifeq ($(BUILD_DIR_BASE),build)
$(error Fixed preparation gates require an isolated BUILD_DIR_BASE or RELEASE_ROOT)
endif
endif
ifneq ($(BUILD_DIR_BASE),build)
RAY_TRACING_PATH_ADMISSION := $(shell python3 tools/preparation_paths.py --repository "$(CURDIR)" --build-root "$(BUILD_DIR_BASE)" --generated-root "$(WORKER_VERSION_GENERATED_DIR)" --dist-root "$(DIST_DIR)" --test-root "$(RAY_TRACING_TEST_WORK_ROOT)" $(if $(filter build/release,$(RELEASE_ROOT)),,--release-root "$(RELEASE_ROOT)") 2>&1)
ifneq ($(RAY_TRACING_PATH_ADMISSION),ok)
$(error $(RAY_TRACING_PATH_ADMISSION))
endif
endif
