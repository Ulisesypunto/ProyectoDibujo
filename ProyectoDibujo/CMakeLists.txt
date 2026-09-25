cmake_minimum_required(VERSION 3.16)
project(ProyectoDibujo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# AGREGAR ESTA LÍNEA: Cambia la ruta por la ubicación real donde instalaste Qt en tu equipo
set(CMAKE_PREFIX_PATH "C:/Qt/6.11.2/mingw_64") 

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(ProyectoDibujo
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(ProyectoDibujo PRIVATE Qt6::Widgets)