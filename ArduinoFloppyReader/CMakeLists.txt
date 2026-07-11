cmake_minimum_required(VERSION 3.21)

project(DrawBridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Threads REQUIRED)

add_library(drawbridge-core STATIC
    lib/ADFWriter.cpp
    lib/ArduinoInterface.cpp
    lib/RotationExtractor.cpp
    lib/SerialIO.cpp
    lib/ftdi.cpp
    lib/ibm_sectors.cpp
    lib/pll.cpp
    lib/capsapi/CapsPlug.cpp
)
target_include_directories(drawbridge-core PUBLIC lib)
target_link_libraries(drawbridge-core PUBLIC ${CMAKE_DL_LIBS} Threads::Threads)

add_subdirectory(ArduinoFloppyReaderQt)
