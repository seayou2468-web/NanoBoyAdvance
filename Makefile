UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
else
$(error This Makefile is Linux-only)
endif

CC       := cc
CXX      := c++
AR       := ar
CFLAGS   := -O2 -std=c11 -Wall -Wextra -Wpedantic
CXXFLAGS := -O2 -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections
QUICK_NES_CXXFLAGS := -Isrc/core/cores/quick_nes -Isrc/core/cores/quick_nes/nes_emu

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)

ifeq ($(strip $(SDL_LIBS)),)
$(error SDL2 development files were not found. Install libsdl2-dev/pkg-config.)
endif

BUILD_DIR := build
APP       := $(BUILD_DIR)/nanoboyadvance-linux
LIBNBA    := $(BUILD_DIR)/libnba.a
DUMP_TOOL := $(BUILD_DIR)/dump_frames

NBA_SRCS  := $(shell find src/core/cores/gba/nba/src -name '*.cpp')
NBA_OBJS  := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(NBA_SRCS))
NES_SRCS  := \
	src/core/cores/quick_nes/nes_emu/Blip_Buffer.cpp \
	src/core/cores/quick_nes/nes_emu/Multi_Buffer.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Buffer.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Blitter.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Effects_Buffer.cpp \
	src/core/cores/quick_nes/nes_emu/Effects_Buffer.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Cart.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Core.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Cpu.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Ppu.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Ppu_Impl.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Ppu_Rendering.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Apu.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Oscs.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Mapper.cpp \
	src/core/cores/quick_nes/nes_emu/nes_data.cpp \
	src/core/cores/quick_nes/nes_emu/nes_mappers.cpp \
	src/core/cores/quick_nes/nes_emu/misc_mappers.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Mmc1.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Mmc3.cpp \
	src/core/cores/quick_nes/nes_emu/Mapper_Mmc5.cpp \
	src/core/cores/quick_nes/nes_emu/Mapper_Fme7.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Fme7_Apu.cpp \
	src/core/cores/quick_nes/nes_emu/Mapper_Namco106.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Namco_Apu.cpp \
	src/core/cores/quick_nes/nes_emu/Nes_Emu.cpp \
	src/core/cores/quick_nes/nes_emu/abstract_file.cpp
NES_OBJS  := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(NES_SRCS))
CORE_OBJS := \
	$(BUILD_DIR)/src/core/api/emulator_core_c_api.o \
	$(BUILD_DIR)/src/core/core_adapter_registry.o \
	$(BUILD_DIR)/src/core/cores/gba/core_adapter.o \
	$(BUILD_DIR)/src/core/cores/quick_nes/core_adapter.o \
	$(BUILD_DIR)/src/core/cores/gba/runtime.o \
	$(BUILD_DIR)/src/core/cores/gba/gba_core_c_api.o \
	$(BUILD_DIR)/src/core/cores/quick_nes/runtime.o
MAIN_OBJ  := $(BUILD_DIR)/main.o
LIBNES    := $(BUILD_DIR)/libquick_nes.a

.PHONY: all clean dump-frames

all: $(APP)
dump-frames: $(DUMP_TOOL)

$(APP): $(MAIN_OBJ) $(CORE_OBJS) $(LIBNBA) $(LIBNES)
	@mkdir -p $(dir $@)
	$(CXX) -Wl,--gc-sections -o $@ $(MAIN_OBJ) $(CORE_OBJS) $(LIBNBA) $(LIBNES) $(SDL_LIBS)

$(DUMP_TOOL): tools/dump_frames.cpp $(CORE_OBJS) $(LIBNBA) $(LIBNES)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -Wl,--gc-sections -I. -o $@ tools/dump_frames.cpp $(CORE_OBJS) $(LIBNBA) $(LIBNES) $(SDL_LIBS)

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
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(QUICK_NES_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/cores/gba/runtime.o: src/core/cores/gba/runtime.cpp src/core/cores/gba/runtime.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/cores/gba/gba_core_c_api.o: src/core/cores/gba/gba_core_c_api.cpp src/core/cores/gba/gba_core_c_api.h src/core/emulator_core_c_api.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/cores/quick_nes/runtime.o: src/core/cores/quick_nes/runtime.cpp src/core/cores/quick_nes/runtime.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(QUICK_NES_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/cores/quick_nes/nes_emu/%.o: src/core/cores/quick_nes/nes_emu/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(QUICK_NES_CXXFLAGS) -Wno-register -Wno-deprecated-declarations -Wno-multichar -Wno-deprecated -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
