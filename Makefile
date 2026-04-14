CC       := cc
CXX      := c++
AR       := ar
CFLAGS   := -O3 -flto -ffast-math -std=c11 -Wall -Wextra -Wpedantic
CXXFLAGS := -O3 -flto -ffast-math -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -DMELONDS_VERSION=\"nba\" -Isrc/core/melonds/teakra/include
NBA_INTERPRETER_CXXFLAGS := -O3 -flto -ffast-math -fomit-frame-pointer -DNBA_ENABLE_MGBA_LOG=0
CORE_DEFINES ?= -DCORE_ENABLE_GBA=1 -DCORE_ENABLE_NES=1 -DCORE_ENABLE_GB=1 -DCORE_ENABLE_NDS=1 -DCORE_ENABLE_3DS=1 -DCITRA_LITE_DISABLE_STATE_CHEATS=1
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)

NEEDS_SDL := $(filter all $(APP) dump-frames,$(MAKECMDGOALS))
ifeq ($(strip $(MAKECMDGOALS)),)
NEEDS_SDL := all
endif
ifeq ($(strip $(SDL_LIBS)),)
ifneq ($(filter all dump-frames,$(NEEDS_SDL)),)
$(error SDL2 development files were not found. Install libsdl2-dev/pkg-config.)
endif
endif

BUILD_DIR := build
APP       := $(BUILD_DIR)/nanoboyadvance-linux
LIBNBA    := $(BUILD_DIR)/libnba.a
DUMP_TOOL := $(BUILD_DIR)/dump_frames

NBA_SRCS_ALL := $(shell find src/core/nanoboyadvance/nba/src -name '*.cpp')
NBA_SRCS_EXCLUDED := $(shell find src/core/nanoboyadvance/nba/src/arm/tablegen -name '*.cpp')
NBA_SRCS  := $(filter-out $(NBA_SRCS_EXCLUDED),$(NBA_SRCS_ALL))
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
	src/core/quick_nes/nes_emu/apu_state.cpp \
	src/core/quick_nes/nes_emu/Nes_Mapper.cpp \
	src/core/quick_nes/nes_emu/Nes_State.cpp \
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
	src/core/quick_nes/nes_emu/abstract_file.cpp
NES_OBJS  := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(NES_SRCS))
SAMEBOY_SRCS := $(filter-out src/core/sameboy/cheat_search.c src/core/sameboy/sm83_disassembler.c src/core/sameboy/symbol_hash.c,$(shell find src/core/sameboy -name '*.c'))
SAMEBOY_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SAMEBOY_SRCS))
MELONDS_CPP_ALL := $(shell find src/core/melonds -name '*.cpp')
MELONDS_CPP_EXCLUDED := \
	src/core/melonds/core_adapter.cpp \
	src/core/melonds/runtime.cpp \
	src/core/melonds/teakra/src/teakra_c.cpp \
	src/core/melonds/teakra/src/disassembler_c.cpp \
	$(shell find src/core/melonds -path '*/main.cpp')
MELONDS_CPP_SRCS := $(filter-out $(MELONDS_CPP_EXCLUDED),$(MELONDS_CPP_ALL))
MELONDS_C_SRCS := $(shell find src/core/melonds -name '*.c')
MELONDS_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(MELONDS_CPP_SRCS)) $(patsubst %.c,$(BUILD_DIR)/%.o,$(MELONDS_C_SRCS))
CORE_OBJS := \
	$(BUILD_DIR)/src/core/api/emulator_core_c_api.o \
	$(BUILD_DIR)/src/core/api/frontend_utils.o \
	$(BUILD_DIR)/src/core/core_adapter_registry.o \
	$(BUILD_DIR)/src/core/nanoboyadvance/core_adapter.o \
	$(BUILD_DIR)/src/core/citra/core_adapter.o \
	$(BUILD_DIR)/src/core/quick_nes/core_adapter.o \
	$(BUILD_DIR)/src/core/sameboy/core_adapter.o \
	$(BUILD_DIR)/src/core/melonds/core_adapter.o \
	$(BUILD_DIR)/src/core/nanoboyadvance/runtime.o \
	$(BUILD_DIR)/src/core/nanoboyadvance/gba_core_c_api.o \
	$(BUILD_DIR)/src/core/quick_nes/runtime.o \
	$(BUILD_DIR)/src/core/sameboy/runtime.o \
	$(BUILD_DIR)/src/core/melonds/runtime.o \
	$(MELONDS_OBJS) \
	$(SAMEBOY_OBJS)
MAIN_OBJ  := $(BUILD_DIR)/main.o
LIBNES    := $(BUILD_DIR)/libquick_nes.a

.PHONY: all clean dump-frames ios-libnba

all: $(APP)
dump-frames: $(DUMP_TOOL)
ios-libnba: $(LIBNBA)

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
	$(CXX) $(CXXFLAGS) $(CORE_DEFINES) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/nanoboyadvance/runtime.o: src/core/nanoboyadvance/runtime.cpp src/core/nanoboyadvance/runtime.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CORE_DEFINES) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/nanoboyadvance/gba_core_c_api.o: src/core/nanoboyadvance/gba_core_c_api.cpp src/core/nanoboyadvance/gba_core_c_api.h src/core/emulator_core_c_api.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CORE_DEFINES) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/quick_nes/runtime.o: src/core/quick_nes/runtime.cpp src/core/quick_nes/runtime.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CORE_DEFINES) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/quick_nes/nes_emu/%.o: src/core/quick_nes/nes_emu/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CORE_DEFINES) $(SDL_CFLAGS) -Wno-register -Wno-deprecated-declarations -Wno-multichar -Wno-deprecated -c $< -o $@

$(BUILD_DIR)/src/core/sameboy/%.o: src/core/sameboy/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -DGB_INTERNAL -DGB_DISABLE_DEBUGGER -DGB_VERSION=\"unknown\" -Wno-unused-function -Wno-unused-parameter -Wno-sign-compare -c $< -o $@

$(BUILD_DIR)/src/core/sameboy/%.o: src/core/sameboy/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CORE_DEFINES) $(SDL_CFLAGS) -DGB_DISABLE_DEBUGGER -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CORE_DEFINES) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -Wno-unused-function -Wno-unused-parameter -Wno-sign-compare -c $< -o $@

$(BUILD_DIR)/src/core/nanoboyadvance/nba/src/%.o: src/core/nanoboyadvance/nba/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(NBA_INTERPRETER_CXXFLAGS) $(CORE_DEFINES) $(SDL_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
