#**************************************************************************************************
#
#   raylib makefile for Desktop platforms, Raspberry Pi, Android and HTML5
#
#   Copyright (c) 2013-2019 Ramon Santamaria (@raysan5)
#
#   This software is provided "as-is", without any express or implied warranty. In no event
#   will the authors be held liable for any damages arising from the use of this software.
#
#   Permission is granted to anyone to use this software for any purpose, including commercial
#   applications, and to alter it and redistribute it freely, subject to the following restrictions:
#
#     1. The origin of this software must not be misrepresented; you must not claim that you
#     wrote the original software. If you use this software in a product, an acknowledgment
#     in the product documentation would be appreciated but is not required.
#
#     2. Altered source versions must be plainly marked as such, and must not be misrepresented
#     as being the original software.
#
#     3. This notice may not be removed or altered from any source distribution.
#
#**************************************************************************************************

.PHONY: all clean raylib_build test test-floor-render test-wall-render test-prop-render test-wall-prop-render test-reward-pouch-render

# Define required raylib variables
PROJECT_NAME       ?= game
TEST_BIN           = tests/inventory_rules_tests$(EXT)
FLOOR_TEST_BIN     = tests/floor_rules_tests$(EXT)
FLOOR_RENDER_TEST_BIN = tests/floor_render_tests$(EXT)
WALL_TEST_BIN      = tests/wall_rules_tests$(EXT)
WALL_RENDER_TEST_BIN = tests/wall_render_tests$(EXT)
ROOM_GENERATION_TEST_BIN = tests/room_generation_tests$(EXT)
ROOM_COLLISION_TEST_BIN = tests/room_collision_rules_tests$(EXT)
PROP_RULES_TEST_BIN = tests/prop_rules_tests$(EXT)
PROP_COMBAT_TEST_BIN = tests/prop_combat_tests$(EXT)
PROP_RENDER_TEST_BIN = tests/prop_render_tests$(EXT)
PROP_GAMEPLAY_TEST_BIN = tests/prop_gameplay_tests$(EXT)
WALL_PROP_RULES_TEST_BIN = tests/wall_prop_rules_tests$(EXT)
WALL_PROP_RENDER_TEST_BIN = tests/wall_prop_render_tests$(EXT)
REWARD_POUCH_RULES_TEST_BIN = tests/reward_pouch_rules_tests$(EXT)
REWARD_POUCH_GAMEPLAY_TEST_BIN = tests/reward_pouch_gameplay_tests$(EXT)
REWARD_POUCH_RENDER_TEST_BIN = tests/reward_pouch_render_tests$(EXT)
RAYLIB_VERSION     := 5.5.0
RAYLIB_API_VERSION := 550
RAYLIB_PATH        := vendor/raylib
RAYLIB_SRC_PATH    := $(RAYLIB_PATH)/src
RAYLIB_RELEASE_PATH := $(RAYLIB_SRC_PATH)
RAYLIB_CC          ?= gcc
RAYLIB_AR          ?= ar

# Optional directory added to PATH for the external MinGW toolchain.
COMPILER_PATH      ?=

ifneq ($(strip $(COMPILER_PATH)),)
    ifeq ($(OS),Windows_NT)
        export PATH := $(COMPILER_PATH);$(PATH)
    else
        export PATH := $(COMPILER_PATH):$(PATH)
    endif
endif

# Define default options
# One of PLATFORM_DESKTOP, PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
PLATFORM           ?= PLATFORM_DESKTOP

# Locations of your newly installed library and associated headers. See ../src/Makefile
# On Linux, if you have installed raylib but cannot compile the examples, check that
# the *_INSTALL_PATH values here are the same as those in src/Makefile or point to known locations.
# To enable system-wide compile-time and runtime linking to libraylib.so, run ../src/$ sudo make install RAYLIB_LIBTYPE_SHARED.
# To enable compile-time linking to a special version of libraylib.so, change these variables here.
# To enable runtime linking to a special version of libraylib.so, see EXAMPLE_RUNTIME_PATH below.
# If there is a libraylib in both EXAMPLE_RUNTIME_PATH and RAYLIB_INSTALL_PATH, at runtime,
# the library at EXAMPLE_RUNTIME_PATH, if present, will take precedence over the one at RAYLIB_INSTALL_PATH.
# RAYLIB_INSTALL_PATH should be the desired full path to libraylib. No relative paths.
DESTDIR ?= /usr/local
RAYLIB_INSTALL_PATH ?= $(DESTDIR)/lib
# RAYLIB_H_INSTALL_PATH locates the installed raylib header and associated source files.
RAYLIB_H_INSTALL_PATH ?= $(DESTDIR)/include

# Library type used for raylib: STATIC (.a) or SHARED (.so/.dll)
RAYLIB_LIBTYPE        ?= STATIC

# Build mode for project: DEBUG or RELEASE
BUILD_MODE            ?= RELEASE

# Use external GLFW library instead of rglfw module
# TODO: Review usage on Linux. Target version of choice. Switch on -lglfw or -lglfw3
USE_EXTERNAL_GLFW     ?= FALSE

# Use Wayland display server protocol on Linux desktop
# by default it uses X11 windowing system
USE_WAYLAND_DISPLAY   ?= FALSE

# Determine PLATFORM_OS in case PLATFORM_DESKTOP selected
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    # No uname.exe on MinGW!, but OS=Windows_NT on Windows!
    # ifeq ($(UNAME),Msys) -> Windows
    ifeq ($(OS),Windows_NT)
        PLATFORM_OS=WINDOWS
        EXT = .exe
    else
        UNAMEOS=$(shell uname)
        ifeq ($(UNAMEOS),Linux)
            PLATFORM_OS=LINUX
        endif
        ifeq ($(UNAMEOS),FreeBSD)
            PLATFORM_OS=BSD
        endif
        ifeq ($(UNAMEOS),OpenBSD)
            PLATFORM_OS=BSD
        endif
        ifeq ($(UNAMEOS),NetBSD)
            PLATFORM_OS=BSD
        endif
        ifeq ($(UNAMEOS),DragonFly)
            PLATFORM_OS=BSD
        endif
        ifeq ($(UNAMEOS),Darwin)
            PLATFORM_OS=OSX
        endif
    endif
endif
ifeq ($(PLATFORM),PLATFORM_RPI)
    UNAMEOS=$(shell uname)
    ifeq ($(UNAMEOS),Linux)
        PLATFORM_OS=LINUX
    endif
endif

ifeq ($(PLATFORM),PLATFORM_WEB)
    # Emscripten required variables
    EMSDK_PATH          ?= C:/emsdk
    EMSCRIPTEN_VERSION  ?= 1.38.31
    CLANG_VERSION       = e$(EMSCRIPTEN_VERSION)_64bit
    PYTHON_VERSION      = 2.7.13.1_64bit\python-2.7.13.amd64
    NODE_VERSION        = 8.9.1_64bit
    export PATH         := $(EMSDK_PATH);$(EMSDK_PATH)\clang\$(CLANG_VERSION);$(EMSDK_PATH)\node\$(NODE_VERSION)\bin;$(EMSDK_PATH)\python\$(PYTHON_VERSION);$(EMSDK_PATH)\emscripten\$(EMSCRIPTEN_VERSION);$(PATH)
    EMSCRIPTEN          = $(EMSDK_PATH)\emscripten\$(EMSCRIPTEN_VERSION)
endif

# Define raylib release directory for compiled library.
# RAYLIB_RELEASE_PATH points to provided binaries or your freshly built version
# EXAMPLE_RUNTIME_PATH embeds a custom runtime location of libraylib.so or other desired libraries
# into each example binary compiled with RAYLIB_LIBTYPE=SHARED. It defaults to RAYLIB_RELEASE_PATH
# so that these examples link at runtime with your version of libraylib.so in ../release/libs/linux
# without formal installation from ../src/Makefile. It aids portability and is useful if you have
# multiple versions of raylib, have raylib installed to a non-standard location, or want to
# bundle libraylib.so with your game. Change it to your liking.
# NOTE: If, at runtime, there is a libraylib.so at both EXAMPLE_RUNTIME_PATH and RAYLIB_INSTALL_PATH,
# The library at EXAMPLE_RUNTIME_PATH, if present, will take precedence over RAYLIB_INSTALL_PATH,
# Implemented for LINUX below with CFLAGS += -Wl,-rpath,$(EXAMPLE_RUNTIME_PATH)
# To see the result, run readelf -d core/core_basic_window; looking at the RPATH or RUNPATH attribute.
# To see which libraries a built example is linking to, ldd core/core_basic_window;
# Look for libraylib.so.1 => $(RAYLIB_INSTALL_PATH)/libraylib.so.1 or similar listing.
EXAMPLE_RUNTIME_PATH   ?= $(RAYLIB_RELEASE_PATH)

# Define default C compiler: gcc
# NOTE: define g++ compiler if using C++
CC = g++

ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),OSX)
        # OSX default compiler
        CC = clang++
    endif
    ifeq ($(PLATFORM_OS),BSD)
        # FreeBSD, OpenBSD, NetBSD, DragonFly default compiler
        CC = clang
    endif
endif
ifeq ($(PLATFORM),PLATFORM_RPI)
    ifeq ($(USE_RPI_CROSS_COMPILER),TRUE)
        # Define RPI cross-compiler
        #CC = armv6j-hardfloat-linux-gnueabi-gcc
        CC = $(RPI_TOOLCHAIN)/bin/arm-linux-gnueabihf-gcc
    endif
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
    # HTML5 emscripten compiler
    # WARNING: To compile to HTML5, code must be redesigned 
    # to use emscripten.h and emscripten_set_main_loop()
    CC = emcc
endif

# Define default make program: Mingw32-make
MAKE = mingw32-make

ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),LINUX)
        MAKE = make
    endif
    ifeq ($(PLATFORM_OS),OSX)
        MAKE = make
    endif
endif

# Define compiler flags:
#  -O0                  defines optimization level (no optimization, better for debugging)
#  -O1                  defines optimization level
#  -g                   include debug information on compilation
#  -s                   strip unnecessary data from build -> do not use in debug builds
#  -Wall                turns on most, but not all, compiler warnings
#  -std=c99             defines C language mode (standard C from 1999 revision)
#  -std=gnu99           defines C language mode (GNU C from 1999 revision)
#  -Wno-missing-braces  ignore invalid warning (GCC bug 53119)
#  -D_DEFAULT_SOURCE    use with -std=c99 on Linux and PLATFORM_WEB, required for timespec
CFLAGS += -Wall -std=c++17 -D_DEFAULT_SOURCE -Wno-missing-braces
WINDOWS_RESOURCE =

ifeq ($(BUILD_MODE),DEBUG)
    CFLAGS += -g -O0
else
    CFLAGS += -s -O1
endif

# Additional flags for compiler (if desired)
#CFLAGS += -Wextra -Wmissing-prototypes -Wstrict-prototypes
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),WINDOWS)
        # resource file contains windows executable icon and properties
        # -Wl,--subsystem,windows hides the console window
        WINDOWS_RESOURCE = $(RAYLIB_PATH)/src/raylib.rc.data
    endif
    ifeq ($(PLATFORM_OS),LINUX)
        ifeq ($(RAYLIB_LIBTYPE),STATIC)
            CFLAGS += -D_DEFAULT_SOURCE
        endif
        ifeq ($(RAYLIB_LIBTYPE),SHARED)
            # Explicitly enable runtime link to libraylib.so
            CFLAGS += -Wl,-rpath,$(EXAMPLE_RUNTIME_PATH)
        endif
    endif
endif
ifeq ($(PLATFORM),PLATFORM_RPI)
    CFLAGS += -std=gnu99
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
    # -Os                        # size optimization
    # -O2                        # optimization level 2, if used, also set --memory-init-file 0
    # -s USE_GLFW=3              # Use glfw3 library (context/input management)
    # -s ALLOW_MEMORY_GROWTH=1   # to allow memory resizing -> WARNING: Audio buffers could FAIL!
    # -s TOTAL_MEMORY=16777216   # to specify heap memory size (default = 16MB)
    # -s USE_PTHREADS=1          # multithreading support
    # -s WASM=0                  # disable Web Assembly, emitted by default
    # -s EMTERPRETIFY=1          # enable emscripten code interpreter (very slow)
    # -s EMTERPRETIFY_ASYNC=1    # support synchronous loops by emterpreter
    # -s FORCE_FILESYSTEM=1      # force filesystem to load/save files data
    # -s ASSERTIONS=1            # enable runtime checks for common memory allocation errors (-O1 and above turn it off)
    # --profiling                # include information for code profiling
    # --memory-init-file 0       # to avoid an external memory initialization code file (.mem)
    # --preload-file resources   # specify a resources folder for data compilation
    CFLAGS += -Os -s USE_GLFW=3 -s TOTAL_MEMORY=16777216 --preload-file resources
    ifeq ($(BUILD_MODE), DEBUG)
        CFLAGS += -s ASSERTIONS=1 --profiling
    endif

    # Define a custom shell .html and output extension
    CFLAGS += --shell-file $(RAYLIB_PATH)/src/shell.html
    EXT = .html
endif

# Define include paths for required headers
# NOTE: Several external required libraries (stb and others)
INCLUDE_PATHS = -I./inc -I./inc/rlcpp -I$(RAYLIB_PATH)/src -I$(RAYLIB_PATH)/src/external
ifneq ($(wildcard /opt/homebrew/include/.*),)
    INCLUDE_PATHS += -I/opt/homebrew/include
endif

# Define additional directories containing required header files
ifeq ($(PLATFORM),PLATFORM_RPI)
    # RPI required libraries
    INCLUDE_PATHS += -I/opt/vc/include
    INCLUDE_PATHS += -I/opt/vc/include/interface/vmcs_host/linux
    INCLUDE_PATHS += -I/opt/vc/include/interface/vcos/pthreads
endif
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),BSD)
        # Consider -L$(RAYLIB_H_INSTALL_PATH)
        INCLUDE_PATHS += -I/usr/local/include
    endif
endif

# Define library paths containing required libs.
LDFLAGS = -L.

ifneq ($(wildcard $(RAYLIB_RELEASE_PATH)/.*),)
    LDFLAGS += -L$(RAYLIB_RELEASE_PATH)
endif
ifneq ($(wildcard $(RAYLIB_PATH)/src/.*),)
    LDFLAGS += -L$(RAYLIB_PATH)/src
endif
ifneq ($(wildcard /opt/homebrew/lib/.*),)
    LDFLAGS += -L/opt/homebrew/lib
endif

ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),BSD)
        # Consider -L$(RAYLIB_INSTALL_PATH)
        LDFLAGS += -L. -Lsrc -L/usr/local/lib
    endif
endif

ifeq ($(PLATFORM),PLATFORM_RPI)
    LDFLAGS += -L/opt/vc/lib
endif

# Define any libraries required on linking
# if you want to link libraries (libname.so or libname.a), use the -lname
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),WINDOWS)
        # Libraries for Windows desktop compilation
        # NOTE: WinMM library required to set high-res timer resolution
        LDLIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
        # Required for physac examples
        #LDLIBS += -static -lpthread
    endif
    ifeq ($(PLATFORM_OS),LINUX)
        # Libraries for Debian GNU/Linux desktop compiling
        # NOTE: Required packages: libegl1-mesa-dev
        LDLIBS = -lraylib -lGL -lm -lpthread -ldl -lrt
        
        # On X11 requires also below libraries
        LDLIBS += -lX11
        # NOTE: It seems additional libraries are not required any more, latest GLFW just dlopen them
        #LDLIBS += -lXrandr -lXinerama -lXi -lXxf86vm -lXcursor
        
        # On Wayland windowing system, additional libraries requires
        ifeq ($(USE_WAYLAND_DISPLAY),TRUE)
            LDLIBS += -lwayland-client -lwayland-cursor -lwayland-egl -lxkbcommon
        endif
        # Explicit link to libc
        ifeq ($(RAYLIB_LIBTYPE),SHARED)
            LDLIBS += -lc
        endif
    endif
    ifeq ($(PLATFORM_OS),OSX)
        # Libraries for OSX 10.9 desktop compiling
        # NOTE: Required packages: libopenal-dev libegl1-mesa-dev
        LDLIBS = -lraylib -framework OpenGL -framework OpenAL -framework Cocoa -framework IOKit
    endif
    ifeq ($(PLATFORM_OS),BSD)
        # Libraries for FreeBSD, OpenBSD, NetBSD, DragonFly desktop compiling
        # NOTE: Required packages: mesa-libs
        LDLIBS = -lraylib -lGL -lpthread -lm

        # On XWindow requires also below libraries
        LDLIBS += -lX11 -lXrandr -lXinerama -lXi -lXxf86vm -lXcursor
    endif
    ifeq ($(USE_EXTERNAL_GLFW),TRUE)
        # NOTE: It could require additional packages installed: libglfw3-dev
        LDLIBS += -lglfw
    endif
endif
ifeq ($(PLATFORM),PLATFORM_RPI)
    # Libraries for Raspberry Pi compiling
    # NOTE: Required packages: libasound2-dev (ALSA)
    LDLIBS = -lraylib -lbrcmGLESv2 -lbrcmEGL -lpthread -lrt -lm -lbcm_host -ldl
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
    # Libraries for web (HTML5) compiling
    LDLIBS = $(RAYLIB_RELEASE_PATH)/libraylib.bc
endif

# Define a recursive wildcard function
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

# Define all source files required
SRC_DIR = src
OBJ_DIR = obj

# Define all source and object files (C++)
SRC = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Build the pinned Raylib source copy before linking the game.
raylib_build:
	$(MAKE) -C $(RAYLIB_SRC_PATH) PLATFORM=$(PLATFORM) CC=$(RAYLIB_CC) AR=$(RAYLIB_AR) RAYLIB_BUILD_MODE=$(BUILD_MODE)

# Default target entry
all: $(PROJECT_NAME)

test: $(TEST_BIN) $(FLOOR_TEST_BIN) $(WALL_TEST_BIN) $(ROOM_GENERATION_TEST_BIN) $(ROOM_COLLISION_TEST_BIN) $(PROP_RULES_TEST_BIN) $(PROP_COMBAT_TEST_BIN) $(PROP_GAMEPLAY_TEST_BIN) $(REWARD_POUCH_RULES_TEST_BIN) $(REWARD_POUCH_GAMEPLAY_TEST_BIN) $(WALL_PROP_RULES_TEST_BIN)
	$(TEST_BIN)
	$(FLOOR_TEST_BIN)
	$(WALL_TEST_BIN)
	$(ROOM_GENERATION_TEST_BIN)
	$(ROOM_COLLISION_TEST_BIN)
	$(PROP_RULES_TEST_BIN)
	$(PROP_COMBAT_TEST_BIN)
	$(PROP_GAMEPLAY_TEST_BIN)
	$(REWARD_POUCH_RULES_TEST_BIN)
	$(REWARD_POUCH_GAMEPLAY_TEST_BIN)
	$(WALL_PROP_RULES_TEST_BIN)

$(REWARD_POUCH_RULES_TEST_BIN): tests/reward_pouch_rules_tests.cpp src/reward_pouch.cpp src/reward_pouch.h
	$(CC) -o $@ tests/reward_pouch_rules_tests.cpp src/reward_pouch.cpp $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) -D$(PLATFORM)

PROP_COMBAT_SOURCES = src/projectile.cpp src/projectile_melee.cpp src/projectile_ranged.cpp src/combat_geometry.cpp
PROP_GAMEPLAY_SOURCES = $(PROP_COMBAT_SOURCES) src/enemy.cpp src/enemy_common.cpp src/enemy_spawner.cpp src/weapon_blueprints.cpp src/player.cpp src/prop.cpp src/prop_rules.cpp src/room.cpp src/room_collision_rules.cpp
WALL_PROP_RULES_SOURCES = src/wall_prop.cpp src/wall_rules.cpp src/floor_rules.cpp
REWARD_POUCH_GAMEPLAY_SOURCES = $(PROP_GAMEPLAY_SOURCES) src/reward_pouch.cpp src/reward_pouch_operations.cpp src/inventory_operations.cpp src/inventory_rules.cpp src/chest.cpp src/room_manager.cpp src/room_features.cpp src/biome_config.cpp $(WALL_PROP_RULES_SOURCES)
$(REWARD_POUCH_GAMEPLAY_TEST_BIN): tests/reward_pouch_gameplay_tests.cpp $(REWARD_POUCH_GAMEPLAY_SOURCES) $(wildcard src/*.h) raylib_build
	$(CC) -o $@ tests/reward_pouch_gameplay_tests.cpp $(REWARD_POUCH_GAMEPLAY_SOURCES) $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

# Optional graphics test, using a hidden window and offscreen pixel assertions.
test-reward-pouch-render: $(REWARD_POUCH_RENDER_TEST_BIN)
	$(REWARD_POUCH_RENDER_TEST_BIN)

REWARD_POUCH_RENDER_SOURCES = src/reward_pouch.cpp src/reward_pouch_renderer.cpp src/prop_renderer.cpp src/prop.cpp src/room.cpp src/floor_rules.cpp
$(REWARD_POUCH_RENDER_TEST_BIN): tests/reward_pouch_render_tests.cpp $(REWARD_POUCH_RENDER_SOURCES) $(wildcard src/*.h) raylib_build
	$(CC) -o $@ tests/reward_pouch_render_tests.cpp $(REWARD_POUCH_RENDER_SOURCES) $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

PROP_RULES_SOURCES = src/prop.cpp src/prop_rules.cpp src/room.cpp src/room_collision_rules.cpp src/room_manager.cpp src/room_features.cpp src/chest.cpp $(WALL_PROP_RULES_SOURCES)

$(WALL_PROP_RULES_TEST_BIN): tests/wall_prop_rules_tests.cpp $(WALL_PROP_RULES_SOURCES) src/room.cpp $(wildcard src/*.h)
	$(CC) -o $@ tests/wall_prop_rules_tests.cpp $(WALL_PROP_RULES_SOURCES) src/room.cpp $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) -D$(PLATFORM)

test-wall-prop-render: $(WALL_PROP_RENDER_TEST_BIN)
	$(WALL_PROP_RENDER_TEST_BIN)

WALL_PROP_RENDER_SOURCES = $(WALL_PROP_RULES_SOURCES) src/wall_prop_renderer.cpp src/wall_tiles.cpp src/prop_renderer.cpp src/prop.cpp src/room.cpp
$(WALL_PROP_RENDER_TEST_BIN): tests/wall_prop_render_tests.cpp $(WALL_PROP_RENDER_SOURCES) $(wildcard src/*.h) raylib_build
	$(CC) -o $@ tests/wall_prop_render_tests.cpp $(WALL_PROP_RENDER_SOURCES) $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

$(PROP_RULES_TEST_BIN): tests/prop_rules_tests.cpp $(PROP_RULES_SOURCES) $(wildcard src/*.h)
	$(CC) -o $@ tests/prop_rules_tests.cpp $(PROP_RULES_SOURCES) $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) -D$(PLATFORM)

$(PROP_COMBAT_TEST_BIN): tests/prop_combat_tests.cpp $(PROP_COMBAT_SOURCES) $(wildcard src/*.h) raylib_build
	$(CC) -o $@ tests/prop_combat_tests.cpp $(PROP_COMBAT_SOURCES) $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

$(PROP_GAMEPLAY_TEST_BIN): tests/prop_gameplay_tests.cpp $(PROP_GAMEPLAY_SOURCES) $(wildcard src/*.h) raylib_build
	$(CC) -o $@ tests/prop_gameplay_tests.cpp $(PROP_GAMEPLAY_SOURCES) $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

test-prop-render: $(PROP_RENDER_TEST_BIN)
	$(PROP_RENDER_TEST_BIN)

$(PROP_RENDER_TEST_BIN): tests/prop_render_tests.cpp src/prop_renderer.cpp src/prop.cpp src/room.cpp src/floor_rules.cpp $(wildcard src/*.h) raylib_build
	$(CC) -o $@ tests/prop_render_tests.cpp src/prop_renderer.cpp src/prop.cpp src/room.cpp src/floor_rules.cpp $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

$(FLOOR_TEST_BIN): tests/floor_rules_tests.cpp src/floor_rules.cpp src/floor_rules.h src/room_types.h
	$(CC) -o $@ tests/floor_rules_tests.cpp src/floor_rules.cpp $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) -D$(PLATFORM)

$(WALL_TEST_BIN): tests/wall_rules_tests.cpp src/wall_rules.cpp src/wall_rules.h src/floor_rules.cpp src/floor_rules.h src/room.cpp src/room.h src/door_visual_rules.h
	$(CC) -o $@ tests/wall_rules_tests.cpp src/wall_rules.cpp src/floor_rules.cpp src/room.cpp $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) -D$(PLATFORM)

$(ROOM_GENERATION_TEST_BIN): tests/room_generation_tests.cpp src/room_manager.cpp src/room_features.cpp src/room.cpp src/chest.cpp src/prop.cpp src/prop_rules.cpp src/room_collision_rules.cpp src/biome_config.cpp $(WALL_PROP_RULES_SOURCES) $(wildcard src/*.h)
	$(CC) -o $@ tests/room_generation_tests.cpp src/room_manager.cpp src/room_features.cpp src/room.cpp src/chest.cpp src/prop.cpp src/prop_rules.cpp src/room_collision_rules.cpp src/biome_config.cpp $(WALL_PROP_RULES_SOURCES) $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) -D$(PLATFORM)

$(ROOM_COLLISION_TEST_BIN): tests/room_collision_rules_tests.cpp src/room_collision_rules.cpp src/room_collision_rules.h src/room_types.h
	$(CC) -o $@ tests/room_collision_rules_tests.cpp src/room_collision_rules.cpp $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) -D$(PLATFORM)

# Optional integration test: requires a working graphics context (hidden window).
test-floor-render: $(FLOOR_RENDER_TEST_BIN)
	$(FLOOR_RENDER_TEST_BIN)

$(FLOOR_RENDER_TEST_BIN): tests/floor_render_tests.cpp src/floor_tiles.cpp src/floor_rules.cpp src/room.cpp $(wildcard src/*.h) raylib_build
	$(CC) -o $@ tests/floor_render_tests.cpp src/floor_tiles.cpp src/floor_rules.cpp src/room.cpp $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

# Optional integration test: requires a working graphics context (hidden window).
test-wall-render: $(WALL_RENDER_TEST_BIN)
	$(WALL_RENDER_TEST_BIN)

$(WALL_RENDER_TEST_BIN): tests/wall_render_tests.cpp src/wall_tiles.cpp src/wall_rules.cpp src/floor_rules.cpp src/room.cpp $(wildcard src/*.h) raylib_build
	$(CC) -o $@ tests/wall_render_tests.cpp src/wall_tiles.cpp src/wall_rules.cpp src/floor_rules.cpp src/room.cpp $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

$(TEST_BIN): tests/inventory_rules_tests.cpp src/inventory_rules.cpp src/inventory_rules.h src/ui_inventory.h
	$(CC) -o $@ tests/inventory_rules_tests.cpp src/inventory_rules.cpp $(CFLAGS) $(INCLUDE_PATHS) -I$(SRC_DIR) -D$(PLATFORM)

# Project target defined by PROJECT_NAME
$(PROJECT_NAME): $(OBJS) raylib_build
	$(CC) -o $(PROJECT_NAME)$(EXT) $(OBJS) $(WINDOWS_RESOURCE) $(CFLAGS) $(INCLUDE_PATHS) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

# Compile source files
# NOTE: This pattern will compile every module defined on $(OBJS)
#%.o: %.c
ifeq ($(OS),Windows_NT)
$(OBJ_DIR):
	@cmd /C "if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)"
else
$(OBJ_DIR):
	@mkdir -p "$(OBJ_DIR)"
endif

$(OBJS): $(wildcard $(RAYLIB_SRC_PATH)/*.h) $(wildcard $(SRC_DIR)/*.h) $(wildcard ./inc/*.h) $(wildcard ./inc/rlcpp/*.h)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE_PATHS) -D$(PLATFORM)

# Clean everything
ifeq ($(OS),Windows_NT)
clean:
	@cmd /C "if exist $(OBJ_DIR)\*.o del /Q $(OBJ_DIR)\*.o"
	@cmd /C "if exist $(PROJECT_NAME)$(EXT) del /Q $(PROJECT_NAME)$(EXT)"
	@cmd /C "if exist tests\inventory_rules_tests$(EXT) del /Q tests\inventory_rules_tests$(EXT)"
	@cmd /C "if exist tests\floor_rules_tests$(EXT) del /Q tests\floor_rules_tests$(EXT)"
	@cmd /C "if exist tests\floor_render_tests$(EXT) del /Q tests\floor_render_tests$(EXT)"
	@cmd /C "if exist tests\wall_rules_tests$(EXT) del /Q tests\wall_rules_tests$(EXT)"
	@cmd /C "if exist tests\wall_render_tests$(EXT) del /Q tests\wall_render_tests$(EXT)"
	@cmd /C "if exist tests\room_generation_tests$(EXT) del /Q tests\room_generation_tests$(EXT)"
	@cmd /C "if exist tests\room_collision_rules_tests$(EXT) del /Q tests\room_collision_rules_tests$(EXT)"
	@cmd /C "if exist vendor\raylib\src\*.o del /Q vendor\raylib\src\*.o"
	@cmd /C "if exist vendor\raylib\src\libraylib.a del /Q vendor\raylib\src\libraylib.a"
	@cmd /C "if exist vendor\raylib\src\libraylib.web.a del /Q vendor\raylib\src\libraylib.web.a"
	@cmd /C "if exist vendor\raylib\src\raygui.c del /Q vendor\raylib\src\raygui.c"
	@cmd /C "if exist vendor\raylib\src\*-protocol.h del /Q vendor\raylib\src\*-protocol.h"
	@cmd /C "if exist vendor\raylib\src\*-protocol-code.h del /Q vendor\raylib\src\*-protocol-code.h"
	@cmd /C "if exist tests\prop_rules_tests$(EXT) del /Q tests\prop_rules_tests$(EXT)"
	@cmd /C "if exist tests\prop_combat_tests$(EXT) del /Q tests\prop_combat_tests$(EXT)"
	@cmd /C "if exist tests\prop_gameplay_tests$(EXT) del /Q tests\prop_gameplay_tests$(EXT)"
	@cmd /C "if exist tests\prop_render_tests$(EXT) del /Q tests\prop_render_tests$(EXT)"
	@cmd /C "if exist tests\reward_pouch_rules_tests$(EXT) del /Q tests\reward_pouch_rules_tests$(EXT)"
	@cmd /C "if exist tests\reward_pouch_gameplay_tests$(EXT) del /Q tests\reward_pouch_gameplay_tests$(EXT)"
	@cmd /C "if exist tests\reward_pouch_render_tests$(EXT) del /Q tests\reward_pouch_render_tests$(EXT)"
	@cmd /C "if exist tests\wall_prop_rules_tests$(EXT) del /Q tests\wall_prop_rules_tests$(EXT)"
	@cmd /C "if exist tests\wall_prop_render_tests$(EXT) del /Q tests\wall_prop_render_tests$(EXT)"
	@echo Cleaning project build outputs
else
clean:
	$(RM) -f $(WALL_PROP_RULES_TEST_BIN) $(WALL_PROP_RENDER_TEST_BIN)
	$(RM) -f $(OBJ_DIR)/*.o $(PROJECT_NAME)$(EXT) $(TEST_BIN) $(FLOOR_TEST_BIN) $(FLOOR_RENDER_TEST_BIN) $(WALL_TEST_BIN) $(WALL_RENDER_TEST_BIN) $(ROOM_GENERATION_TEST_BIN) $(ROOM_COLLISION_TEST_BIN) $(PROP_RULES_TEST_BIN) $(PROP_COMBAT_TEST_BIN) $(PROP_RENDER_TEST_BIN) $(PROP_GAMEPLAY_TEST_BIN) $(REWARD_POUCH_RULES_TEST_BIN) $(REWARD_POUCH_GAMEPLAY_TEST_BIN) $(REWARD_POUCH_RENDER_TEST_BIN) $(RAYLIB_SRC_PATH)/*.o $(RAYLIB_SRC_PATH)/libraylib.a $(RAYLIB_SRC_PATH)/libraylib.web.a $(RAYLIB_SRC_PATH)/raygui.c $(RAYLIB_SRC_PATH)/*-protocol.h $(RAYLIB_SRC_PATH)/*-protocol-code.h
	@echo Cleaning project build outputs
endif

