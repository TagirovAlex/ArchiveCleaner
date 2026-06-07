cmake_minimum_required(VERSION 3.15)
project(ArchiveCleaner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "MSVC runtime library")

add_definitions(-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -DNOMINMAX)

add_executable(ArchiveCleaner
    src/main.cpp
    src/Config.h
    src/Config.cpp
    src/Mailer.h
    src/Mailer.cpp
)

target_link_libraries(ArchiveCleaner PRIVATE
    ws2_32
)
