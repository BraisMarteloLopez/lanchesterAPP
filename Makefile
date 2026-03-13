# Makefile — Cross-compilacion de la GUI para Windows (.exe)
# Uso: make

# Toolchain
CXX      = x86_64-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

# Directorios de vendor
IMGUI_DIR = vendor/imgui-1.91.7
IMPLOT_DIR = vendor/implot-0.16
SDL2_DIR  = vendor/SDL2-2.30.10/x86_64-w64-mingw32

# Includes
INCLUDES = -Iinclude \
           -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends \
           -I$(IMPLOT_DIR) \
           -I$(SDL2_DIR)/include/SDL2

# Fuentes ImGui
IMGUI_SRC = $(IMGUI_DIR)/imgui.cpp \
            $(IMGUI_DIR)/imgui_draw.cpp \
            $(IMGUI_DIR)/imgui_tables.cpp \
            $(IMGUI_DIR)/imgui_widgets.cpp \
            $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp \
            $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

# Fuentes implot
IMPLOT_SRC = $(IMPLOT_DIR)/implot.cpp \
             $(IMPLOT_DIR)/implot_items.cpp

# Fuentes del proyecto
APP_SRC = gui_main.cpp

# Todos los fuentes
ALL_SRC = $(APP_SRC) $(IMGUI_SRC) $(IMPLOT_SRC)

# Libs
LDFLAGS = -L$(SDL2_DIR)/lib \
          -lmingw32 -lSDL2main -lSDL2 -lopengl32 \
          -static-libgcc -static-libstdc++ \
          -mwindows

# Target
TARGET = release/lanchester_gui.exe

all: $(TARGET)

$(TARGET): $(ALL_SRC)
	@mkdir -p release
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(ALL_SRC) $(LDFLAGS) -o $(TARGET)
	@cp -u $(SDL2_DIR)/bin/SDL2.dll release/ 2>/dev/null || true
	@cp -u model_params.json vehicle_db.json vehicle_db_en.json release/ 2>/dev/null || true
	@echo "============================================="
	@echo " Build completado: $(TARGET)"
	@echo " Contenido de release/:"
	@ls -lh release/
	@echo "============================================="

clean:
	rm -f $(TARGET) release/SDL2.dll

.PHONY: all clean
