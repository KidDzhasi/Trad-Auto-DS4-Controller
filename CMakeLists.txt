cmake_minimum_required(VERSION 3.16)

project(trad-auto-ds4-controller VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(trad-auto-ds4-controller
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    resources.qrc
)

target_link_libraries(trad-auto-ds4-controller PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

install(TARGETS trad-auto-ds4-controller
    RUNTIME DESTINATION bin
)
