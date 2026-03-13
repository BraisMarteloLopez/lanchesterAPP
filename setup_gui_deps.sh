#!/bin/bash
# Descargar dependencias para compilar la GUI de Windows
# Uso: bash setup_gui_deps.sh

set -e
mkdir -p vendor && cd vendor

echo "Descargando SDL2 (MinGW dev)..."
curl -sL -o sdl2.tar.gz "https://github.com/libsdl-org/SDL/releases/download/release-2.30.10/SDL2-devel-2.30.10-mingw.tar.gz"
tar xzf sdl2.tar.gz && rm sdl2.tar.gz

echo "Descargando Dear ImGui v1.91.7..."
curl -sL -o imgui.tar.gz "https://github.com/ocornut/imgui/archive/refs/tags/v1.91.7.tar.gz"
tar xzf imgui.tar.gz && rm imgui.tar.gz

echo "Descargando implot v0.16..."
curl -sL -o implot.tar.gz "https://github.com/epezent/implot/archive/refs/tags/v0.16.tar.gz"
tar xzf implot.tar.gz && rm implot.tar.gz

echo ""
echo "Dependencias listas en vendor/"
echo "Para compilar: make -f Makefile.gui"
