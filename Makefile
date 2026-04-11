UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
else
$(error This Makefile is Linux-only)
endif

CC       := cc
CXX      := c++
AR       := ar
BASE_CFLAGS   := -std=gnu11 -Wall -Wextra -Wpedantic
BASE_CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections
OPT_LEVEL ?= 2
CFLAGS   ?= -O$(OPT_LEVEL) $(BASE_CFLAGS)
CXXFLAGS ?= -O$(OPT_LEVEL) $(BASE_CXXFLAGS)
OPTIMIZED_OPT_LEVEL ?= 3
OPTIMIZED_EXTRA_FLAGS ?= -DNDEBUG -march=native -mtune=native -fno-math-errno
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)

ifeq ($(strip $(SDL_LIBS)),)
$(error SDL2 development files were not found. Install libsdl2-dev/pkg-config.)
endif

BUILD_DIR := build
APP       := $(BUILD_DIR)/nanoboyadvance-linux
LIBNBA    := $(BUILD_DIR)/libnba.a
DUMP_TOOL := $(BUILD_DIR)/dump_frames

NBA_SRCS  := $(shell find src/core/nanoboyadvance/nba/src -name '*.cpp')
NBA_OBJS  := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(NBA_SRCS))
NES_SRCS  := \
	src/core/quick_nes/nes_emu/Blip_Buffer.cpp \
	src/core/quick_nes/nes_emu/Multi_Buffer.cpp \
	src/core/quick_nes/nes_emu/Nes_Buffer.cpp \
	src/core/quick_nes/nes_emu/Nes_Blitter.cpp \
	src/core/quick_nes/nes_emu/Nes_Effects_Buffer.cpp \
	src/core/quick_nes/nes_emu/Effects_Buffer.cpp \
	src/core/quick_nes/nes_emu/Nes_Cart.cpp \
	src/core/quick_nes/nes_emu/Nes_Core.cpp \
	src/core/quick_nes/nes_emu/Nes_Cpu.cpp \
	src/core/quick_nes/nes_emu/Nes_Ppu.cpp \
	src/core/quick_nes/nes_emu/Nes_Ppu_Impl.cpp \
	src/core/quick_nes/nes_emu/Nes_Ppu_Rendering.cpp \
	src/core/quick_nes/nes_emu/Nes_Apu.cpp \
	src/core/quick_nes/nes_emu/Nes_Oscs.cpp \
	src/core/quick_nes/nes_emu/Nes_Mapper.cpp \
	src/core/quick_nes/nes_emu/nes_data.cpp \
	src/core/quick_nes/nes_emu/nes_mappers.cpp \
	src/core/quick_nes/nes_emu/misc_mappers.cpp \
	src/core/quick_nes/nes_emu/Nes_Mmc1.cpp \
	src/core/quick_nes/nes_emu/Nes_Mmc3.cpp \
	src/core/quick_nes/nes_emu/Mapper_Mmc5.cpp \
	src/core/quick_nes/nes_emu/Mapper_Fme7.cpp \
	src/core/quick_nes/nes_emu/Nes_Fme7_Apu.cpp \
	src/core/quick_nes/nes_emu/Mapper_Namco106.cpp \
	src/core/quick_nes/nes_emu/Nes_Namco_Apu.cpp \
	src/core/quick_nes/nes_emu/Nes_Emu.cpp \
	src/core/quick_nes/nes_emu/Nes_State.cpp \
	src/core/quick_nes/nes_emu/Nes_Apu_State.cpp \
	src/core/quick_nes/nes_emu/apu_state.cpp \
	src/core/quick_nes/nes_emu/abstract_file.cpp
NES_OBJS  := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(NES_SRCS))
SAMEBOY_SRCS := $(filter-out src/core/sameboy/cheat_search.c src/core/sameboy/sm83_disassembler.c src/core/sameboy/symbol_hash.c,$(shell find src/core/sameboy -name '*.c'))
SAMEBOY_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SAMEBOY_SRCS))
CORE_OBJS := \
	$(BUILD_DIR)/src/core/api/emulator_core_c_api.o \
	$(BUILD_DIR)/src/core/api/frontend_utils.o \
	$(BUILD_DIR)/src/core/core_adapter_registry.o \
	$(BUILD_DIR)/src/core/nanoboyadvance/core_adapter.o \
	$(BUILD_DIR)/src/core/quick_nes/core_adapter.o \
	$(BUILD_DIR)/src/core/sameboy/core_adapter.o \
	$(BUILD_DIR)/src/core/nanoboyadvance/runtime.o \
	$(BUILD_DIR)/src/core/nanoboyadvance/gba_core_c_api.o \
	$(BUILD_DIR)/src/core/quick_nes/runtime.o \
	$(BUILD_DIR)/src/core/sameboy/runtime.o \
	$(SAMEBOY_OBJS)
MAIN_OBJ  := $(BUILD_DIR)/main.o
LIBNES    := $(BUILD_DIR)/libquick_nes.a

.PHONY: all clean dump-frames desume-core-check optimized

all: $(APP)
dump-frames: $(DUMP_TOOL)
desume-core-check:
	DESUME_CHECK_JOBS=1 bash tools/check_desume_nojit_build.sh
optimized:
	$(MAKE) OPT_LEVEL=$(OPTIMIZED_OPT_LEVEL) CFLAGS="-O$(OPTIMIZED_OPT_LEVEL) $(BASE_CFLAGS) $(OPTIMIZED_EXTRA_FLAGS)" CXXFLAGS="-O$(OPTIMIZED_OPT_LEVEL) $(BASE_CXXFLAGS) $(OPTIMIZED_EXTRA_FLAGS)" all

$(APP): $(MAIN_OBJ) $(CORE_OBJS) $(LIBNBA) $(LIBNES)
	@mkdir -p $(dir $@)
	$(CXX) -Wl,--gc-sections -o $@ $(MAIN_OBJ) $(CORE_OBJS) $(LIBNBA) $(LIBNES) $(SDL_LIBS)

$(DUMP_TOOL): tools/dump_frames.cpp $(CORE_OBJS) $(LIBNBA) $(LIBNES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -Wl,--gc-sections -o $@ tools/dump_frames.cpp $(CORE_OBJS) $(LIBNBA) $(LIBNES) $(SDL_LIBS)

$(LIBNBA): $(NBA_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $(NBA_OBJS)

$(LIBNES): $(NES_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $(NES_OBJS)

$(BUILD_DIR)/main.o: main.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/api/emulator_core_c_api.o: src/core/api/emulator_core_c_api.cpp src/core/api/emulator_core_c_api.h src/core/core_adapter.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/nanoboyadvance/runtime.o: src/core/nanoboyadvance/runtime.cpp src/core/nanoboyadvance/runtime.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/nanoboyadvance/gba_core_c_api.o: src/core/nanoboyadvance/gba_core_c_api.cpp src/core/nanoboyadvance/gba_core_c_api.h src/core/emulator_core_c_api.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/quick_nes/runtime.o: src/core/quick_nes/runtime.cpp src/core/quick_nes/runtime.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/quick_nes/nes_emu/%.o: src/core/quick_nes/nes_emu/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -Wno-register -Wno-deprecated-declarations -Wno-multichar -Wno-deprecated -c $< -o $@

$(BUILD_DIR)/src/core/sameboy/%.o: src/core/sameboy/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -DGB_INTERNAL -DGB_DISABLE_DEBUGGER -DGB_VERSION=\"unknown\" -Wno-unused-function -Wno-unused-parameter -Wno-sign-compare -c $< -o $@

$(BUILD_DIR)/src/core/sameboy/%.o: src/core/sameboy/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -DGB_DISABLE_DEBUGGER -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -Wno-unused-function -Wno-unused-parameter -Wno-sign-compare -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
