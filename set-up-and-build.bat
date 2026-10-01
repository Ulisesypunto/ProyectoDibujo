@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

:: ---- Configuracion (cambia solo aqui) ----
set "QT_VERSION=6.11.2"
set "QT_ARCH=win64_mingw"

:: Uso: set-up-and-build.bat [/clean]
if /i "%~1"=="/clean" if exist build rmdir /s /q build
if not exist build mkdir build
if not exist .deps mkdir .deps

echo ===================================================
echo [1/4] Comprobando CMake y compilador C++...
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
echo [2/4] Buscando Qt %QT_VERSION%...
echo ===================================================
set "QTDIR="
if exist ".deps\Qt\%QT_VERSION%\mingw_64" set "QTDIR=%CD%\.deps\Qt\%QT_VERSION%\mingw_64"
if not defined QTDIR if exist "C:\Qt\%QT_VERSION%\mingw_64" set "QTDIR=C:\Qt\%QT_VERSION%\mingw_64"

if not defined QTDIR (
    echo Qt no encontrado. Descargando con aqtinstall...
    pip install aqtinstall
    python -m aqt install-qt --outputdir "%CD%\.deps\Qt" windows desktop %QT_VERSION% %QT_ARCH%
    if exist ".deps\Qt\%QT_VERSION%\mingw_64" set "QTDIR=%CD%\.deps\Qt\%QT_VERSION%\mingw_64"
)
if not defined QTDIR (
    echo ERROR: no se pudo localizar Qt %QT_VERSION%.
    pause
    exit /b 1
)
echo Usando Qt desde: !QTDIR!
set "PATH=!QTDIR!\bin;%PATH%"

echo.
echo ===================================================
echo [3/4] Configurando CMake...
echo ===================================================
cmake -S ProyectoDibujo -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="!QTDIR!"
if %errorlevel% neq 0 ( pause & exit /b 1 )

echo.
echo ===================================================
echo [4/4] Compilando...
echo ===================================================
cmake --build build
if %errorlevel% neq 0 ( pause & exit /b 1 )

:: Copia las DLL de Qt junto al .exe para poder ejecutarlo con doble click
"!QTDIR!\bin\windeployqt.exe" build\ProyectoDibujo.exe

echo.
echo ===================================================
echo Compilacion finalizada: build\ProyectoDibujo.exe
echo ===================================================
pause
