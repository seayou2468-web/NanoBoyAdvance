UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
else
$(error This Makefile is Linux-only)
endif

CC       := cc
CXX      := c++
AR       := ar
CFLAGS   := -O2 -std=c11 -Wall -Wextra -Wpedantic
CXXFLAGS := -O2 -std=c++20 -Wall -Wextra -Wpedantic

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)

ifeq ($(strip $(SDL_LIBS)),)
$(error SDL2 development files were not found. Install libsdl2-dev/pkg-config.)
endif

BUILD_DIR := build
APP       := $(BUILD_DIR)/nanoboyadvance-linux
LIBNBA    := $(BUILD_DIR)/libnba.a

NBA_SRCS  := $(shell find src/nba/src -name '*.cpp')
NBA_OBJS  := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(NBA_SRCS))
CORE_OBJ  := $(BUILD_DIR)/src/core/gba_core_c_api.o
MAIN_OBJ  := $(BUILD_DIR)/main.o

.PHONY: all clean

all: $(APP)

$(APP): $(MAIN_OBJ) $(CORE_OBJ) $(LIBNBA)
	@mkdir -p $(dir $@)
	$(CXX) -o $@ $(MAIN_OBJ) $(CORE_OBJ) $(LIBNBA) $(SDL_LIBS)

$(LIBNBA): $(NBA_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $(NBA_OBJS)

$(BUILD_DIR)/main.o: main.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/core/gba_core_c_api.o: src/core/gba_core_c_api.cpp src/core/gba_core_c_api.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
