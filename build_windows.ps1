# build_windows.ps1 — Compilar Lanchester-CIO en Windows (PowerShell)
#
# Prerequisitos:
#   1. Instalar MSYS2: https://www.msys2.org/
#   2. Ejecutar en la terminal MSYS2 MinGW64:
#        pacman -Syu
#        pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make
#   3. Anadir C:\msys64\mingw64\bin al PATH de Windows
#
# Uso: .\build_windows.ps1

$ErrorActionPreference = "Stop"

Write-Host "=== Lanchester-CIO Build Script ===" -ForegroundColor Cyan
Write-Host ""

# --- Verificar que MinGW esta en el PATH ---
$gpp = Get-Command "g++.exe" -ErrorAction SilentlyContinue
if (-not $gpp) {
    Write-Host "ERROR: g++ no encontrado en PATH." -ForegroundColor Red
    Write-Host "Asegurate de que C:\msys64\mingw64\bin esta en el PATH." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Para instalar MSYS2 + MinGW:" -ForegroundColor Yellow
    Write-Host "  1. Descargar e instalar https://www.msys2.org/" -ForegroundColor White
    Write-Host "  2. Abrir 'MSYS2 MinGW64' y ejecutar:" -ForegroundColor White
    Write-Host "       pacman -Syu" -ForegroundColor White
    Write-Host "       pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make" -ForegroundColor White
    Write-Host "  3. Anadir C:\msys64\mingw64\bin al PATH de Windows" -ForegroundColor White
    exit 1
}
Write-Host "[OK] g++ encontrado: $($gpp.Source)" -ForegroundColor Green

$cmake = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host "ERROR: cmake no encontrado en PATH." -ForegroundColor Red
    exit 1
}
Write-Host "[OK] cmake encontrado: $($cmake.Source)" -ForegroundColor Green

# --- Descargar dependencias si no existen ---
$vendorDir = "vendor"
if (-not (Test-Path $vendorDir)) {
    Write-Host ""
    Write-Host "Descargando dependencias..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Path $vendorDir | Out-Null

    # SDL2
    Write-Host "  SDL2 2.30.10..." -ForegroundColor White
    $sdl2Url = "https://github.com/libsdl-org/SDL/releases/download/release-2.30.10/SDL2-devel-2.30.10-mingw.tar.gz"
    $sdl2File = "$vendorDir\sdl2.tar.gz"
    Invoke-WebRequest -Uri $sdl2Url -OutFile $sdl2File
    tar xzf $sdl2File -C $vendorDir
    Remove-Item $sdl2File

    # Dear ImGui
    Write-Host "  Dear ImGui 1.91.7..." -ForegroundColor White
    $imguiUrl = "https://github.com/ocornut/imgui/archive/refs/tags/v1.91.7.tar.gz"
    $imguiFile = "$vendorDir\imgui.tar.gz"
    Invoke-WebRequest -Uri $imguiUrl -OutFile $imguiFile
    tar xzf $imguiFile -C $vendorDir
    Remove-Item $imguiFile

    # implot
    Write-Host "  implot 0.16..." -ForegroundColor White
    $implotUrl = "https://github.com/epezent/implot/archive/refs/tags/v0.16.tar.gz"
    $implotFile = "$vendorDir\implot.tar.gz"
    Invoke-WebRequest -Uri $implotUrl -OutFile $implotFile
    tar xzf $implotFile -C $vendorDir
    Remove-Item $implotFile

    Write-Host "[OK] Dependencias descargadas en vendor/" -ForegroundColor Green
} else {
    Write-Host "[OK] vendor/ ya existe, saltando descarga" -ForegroundColor Green
}

# --- Compilar ---
Write-Host ""
Write-Host "Compilando..." -ForegroundColor Cyan

$buildDir = "build-win"
if (Test-Path $buildDir) {
    Remove-Item -Recurse -Force $buildDir
}

cmake -B $buildDir -G "MinGW Makefiles" `
    -DLANCHESTER_BUILD_GUI=ON `
    -DLANCHESTER_BUILD_TESTS=OFF

cmake --build $buildDir --parallel

# --- Verificar resultado ---
Write-Host ""
if (Test-Path "release\lanchester_gui.exe") {
    Write-Host "=== BUILD EXITOSO ===" -ForegroundColor Green
    Write-Host ""
    Write-Host "Ejecutable: release\lanchester_gui.exe" -ForegroundColor White
    Write-Host "Datos:      release\model_params.json" -ForegroundColor White
    Write-Host "            release\vehicle_db.json" -ForegroundColor White
    Write-Host "            release\vehicle_db_en.json" -ForegroundColor White
    Write-Host "            release\gui_config.json" -ForegroundColor White
    Write-Host "            release\SDL2.dll" -ForegroundColor White
    Write-Host ""
    Write-Host "Para ejecutar: cd release && .\lanchester_gui.exe" -ForegroundColor Cyan
} else {
    Write-Host "ERROR: No se genero el ejecutable." -ForegroundColor Red
    exit 1
}
