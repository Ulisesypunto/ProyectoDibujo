cmake_minimum_required(VERSION 3.16)
project(ProyectoDibujo VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Busqueda automatica de Qt6 (cualquier version instalada en .deps o C:/Qt).
# Si pasas -DCMAKE_PREFIX_PATH=... se respeta tu valor.
file(GLOB _qt_kits
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/Qt/*/mingw_64"
    "${CMAKE_CURRENT_SOURCE_DIR}/../.deps/Qt/*/mingw_64"
    "C:/Qt/*/mingw_64")
list(APPEND CMAKE_PREFIX_PATH ${_qt_kits})

# GuiPrivate: QZipReader/QZipWriter (lectura/escritura de .kra). Ver core/io/KraFormat.cpp
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets GuiPrivate)

# --- Libreria con toda la logica (core + tools + ui). Reutilizable por tests ---
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
list(FILTER LIB_SOURCES EXCLUDE REGEX "/src/main\\.cpp$")

add_library(dibujo_lib STATIC ${LIB_SOURCES})
target_include_directories(dibujo_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(dibujo_lib PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::GuiPrivate)

# --- Ejecutable (solo main.cpp) ---
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE dibujo_lib)

# --- Tests opcionales: cmake -DBUILD_TESTS=ON ---
option(BUILD_TESTS "Compilar tests unitarios" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
