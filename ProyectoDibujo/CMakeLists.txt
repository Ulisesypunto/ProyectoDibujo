cmake_minimum_required(VERSION 3.16)
project(ProyectoDibujo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Habilitar el procesamiento automatico de Qt para CMake
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 2. Rutas automaticas de busqueda para Qt6 (relativas a la raiz del proyecto y rutas por defecto)
list(APPEND CMAKE_PREFIX_PATH 
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/Qt/6.5.3/mingw_64"
    "${CMAKE_CURRENT_SOURCE_DIR}/../.deps/Qt/6.5.3/mingw_64"
    "${CMAKE_CURRENT_SOURCE_DIR}/../.deps/Qt/6.11.2/mingw_64"
    "C:/Qt/6.5.3/mingw_64"
    "C:/Qt/6.6.0/mingw_64"
    "C:/Qt/6.7.0/mingw_64"
    "C:/Qt/6.11.2/mingw_64"
)

# 3. Buscar los componentes necesarios de Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

# 4. Incluir los archivos fuente desde la raiz del proyecto
file(GLOB SOURCES "*.cpp" "*.h")

add_executable(${PROJECT_NAME} ${SOURCES})

# 5. Vincular las librerias de Qt al ejecutable
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core)