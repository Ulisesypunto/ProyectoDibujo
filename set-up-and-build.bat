@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo [1/4] Limpiando build previo para evitar errores de cache...
echo ===================================================

if exist "build\CMakeCache.txt" (
    rmdir /s /q build
)
if not exist "build" mkdir build
if not exist ".deps" mkdir .deps

echo.
echo ===================================================
echo [2/4] Comprobando e Instalando CMake y Compilador C++...
echo ===================================================

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo CMake no detectado. Instalando...
    winget install --id Kitware.CMake -e --source winget --silent --accept-source-agreements --accept-package-agreements
    set "PATH=%PATH%;C:\Program Files\CMake\bin"
)

where g++ >nul 2>nul
if %errorlevel% neq 0 (
    echo Compilador C++ no detectado. Instalando MinGW...
    winget install --id BrechtSanders.WinLibs.GCC.MinGW.w64 -e --source winget --silent --accept-source-agreements --accept-package-agreements
)

echo.
echo ===================================================
echo [3/4] Descargando kit de Qt6 local mediante aqtinstall...
echo ===================================================

if not exist ".deps\Qt" (
    echo Instalando aqtinstall vía pip...
    pip install aqtinstall
    
    echo Descargando Qt 6.5.3 (binarios precompilados)...
    python -m aqt install-qt --outputdir %CD%\.deps\Qt windows desktop 6.5.3 win64_mingw
)

:: Definir la ruta relativa de Qt6 para CMake
set "QTDIR=%CD%\.deps\Qt\6.11.2\mingw_64"
if not exist "!QTDIR!" (
    :: Fallback por si la máquina ya tiene Qt6 instalado en el sistema
    set "QTDIR=C:\Qt\6.11.2\mingw_64"
)

echo Usando Qt6 desde: !QTDIR!

echo.
echo ===================================================
echo [4/4] Configurando CMake y Compilando el Proyecto...
echo ===================================================

cd build
cmake .. -DCMAKE_PREFIX_PATH="!QTDIR!"
cmake --build . --config Release

cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/mingw_64"
cmake --build build

echo.
echo ===================================================
echo Compilacion finalizada. El ejecutable esta en build/
echo ===================================================
pause