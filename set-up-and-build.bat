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

:: ---- CMake ----
where cmake >nul 2>nul && goto :cmake_ok
if exist "C:\Qt\Tools\CMake_64\bin\cmake.exe" set "PATH=C:\Qt\Tools\CMake_64\bin;%PATH%"
where cmake >nul 2>nul && goto :cmake_ok
if exist "C:\Program Files\CMake\bin\cmake.exe" set "PATH=C:\Program Files\CMake\bin;%PATH%"
where cmake >nul 2>nul && goto :cmake_ok
echo CMake no detectado. Instalando...
winget install --id Kitware.CMake -e --source winget --silent --accept-source-agreements --accept-package-agreements
set "PATH=%PATH%;C:\Program Files\CMake\bin"
where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake instalado, pero hay que cerrar y abrir esta consola.
    pause
    exit /b 1
)
:cmake_ok

:: ---- Compilador MinGW (el mismo que usa Qt) ----
where g++ >nul 2>nul && where mingw32-make >nul 2>nul && goto :gcc_ok

set "MINGW_BIN="
call :find_mingw
if defined MINGW_BIN goto :gcc_found

echo MinGW no encontrado. Descargando con aqtinstall...
pip install aqtinstall
python -m aqt install-tool --outputdir "%CD%\.deps\Qt" windows desktop tools_mingw1310 qt.tools.win64_mingw1310
call :find_mingw
if not defined MINGW_BIN (
    echo ERROR: no se pudo obtener MinGW.
    pause
    exit /b 1
)

:gcc_found
set "PATH=!MINGW_BIN!;%PATH%"
:gcc_ok
echo Compilador: 
where g++
goto :step2

:find_mingw
for /d %%D in ("%CD%\.deps\Qt\Tools\mingw*_64" "C:\Qt\Tools\mingw*_64") do (
    if exist "%%~D\bin\g++.exe" set "MINGW_BIN=%%~D\bin"
)
exit /b 0

:step2
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
