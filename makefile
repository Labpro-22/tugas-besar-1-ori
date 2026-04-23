# Makefile for Nimonspoli — supports 'make' (CLI) and 'make gui' (SFML GUI)

CXX      := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -I include -I .

SRC_DIR  := src
OBJ_DIR  := build
BIN_DIR  := bin
DATA_DIR := data
CONFIG_DIR := config

# ── Raylib (GUI) ──────────────────────────────────────────────────────────────
RAYLIB_INC        := -I/opt/homebrew/include
RAYLIB_LIB        := -L/opt/homebrew/lib -lraylib
RAYLIB_FRAMEWORKS := -framework CoreVideo -framework IOKit -framework Cocoa \
                     -framework GLUT -framework OpenGL
GUI_LDFLAGS       := $(RAYLIB_LIB) $(RAYLIB_FRAMEWORKS)

# ── CLI Build (excludes gui_main.cpp and src/views/gui/) ─────────────────────
CLI_SRCS := $(shell find $(SRC_DIR) -name '*.cpp' \
              ! -name 'gui_main.cpp' \
              ! -path '$(SRC_DIR)/views/gui/*')
CLI_OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/cli/%.o, $(CLI_SRCS))
CLI_DEPS := $(CLI_OBJS:.o=.d)

CLI_TARGET := $(BIN_DIR)/game

# ── GUI Build (all sources + gui_main.cpp, excludes main.cpp) ────────────────
GUI_SRCS := $(shell find $(SRC_DIR) -name '*.cpp' \
              ! -name 'main.cpp')
GUI_OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/gui/%.o, $(GUI_SRCS))
GUI_DEPS := $(GUI_OBJS:.o=.d)

GUI_TARGET := $(BIN_DIR)/game_gui

# ─────────────────────────────────────────────────────────────────────────────

all: directories $(CLI_TARGET)

gui: directories $(GUI_TARGET)

directories:
	@mkdir -p $(OBJ_DIR)/cli $(OBJ_DIR)/gui $(BIN_DIR) $(DATA_DIR) $(CONFIG_DIR)

# ── CLI compile (with auto header dependency tracking via -MMD) ───────────────
$(OBJ_DIR)/cli/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(CLI_TARGET): $(CLI_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "CLI build OK → $(CLI_TARGET)"

# ── GUI compile (with auto header dependency tracking via -MMD) ───────────────
$(OBJ_DIR)/gui/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(RAYLIB_INC) -DGUI_MODE -MMD -MP -c $< -o $@

$(GUI_TARGET): $(GUI_OBJS)
	$(CXX) $(CXXFLAGS) $^ $(GUI_LDFLAGS) -o $@
	@echo "GUI build OK → $(GUI_TARGET)"

# Include auto-generated header deps (ignored if missing on first build)
-include $(CLI_DEPS)
-include $(GUI_DEPS)

run:     all     ; ./$(CLI_TARGET)
run-gui: gui     ; ./$(GUI_TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Cleaned."

rebuild:     clean all
rebuild-gui: clean gui

.PHONY: all gui directories run run-gui clean rebuild rebuild-gui
