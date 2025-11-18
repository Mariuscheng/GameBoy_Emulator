cmake_minimum_required(VERSION 3.24)

project(GameBoy)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

set(SDL3_DIR "C:/vcpkg/installed/x64-windows/share/sdl3")

find_package(SDL3 CONFIG REQUIRED)

include_directories(include src/mmu)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(GameBoy main.cpp ${SOURCES})

target_link_libraries(GameBoy PRIVATE SDL3::SDL3)

# Use UTF-8 for MSVC to avoid C4819 warnings with non-ASCII comments/strings
if (MSVC)
	target_compile_options(GameBoy PRIVATE /utf-8)
endif()