cmake_minimum_required(VERSION 3.12)
project(GameBoy)

# Use C++20 (vcxproj already uses /std:c++20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set SDL3 paths manually
set(SDL3_DIR "C:/SDL3/cmake")
set(SDL3_INCLUDE_DIR "C:/SDL3/include")
set(SDL3_LIB_DIR "C:/SDL3/lib")

# Find SDL3
find_package(SDL3 REQUIRED PATHS "C:/SDL3/cmake" NO_DEFAULT_PATH)

# Add source files
set(SOURCES
    src/cpu.cpp
    src/cpu_alu.cpp
    src/cpu_bits.cpp
    src/cpu_cb.cpp
    src/cpu_flags.cpp
    src/cpu_instructions.cpp
    src/cpu_rotate.cpp
    src/emulator.cpp
    src/mmu.cpp
    src/ppu.cpp
    src/apu.cpp
    main.cpp
)

# Add executable
add_executable(GameBoy ${SOURCES})

# Include directories
target_include_directories(GameBoy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(GameBoy PRIVATE "${SDL3_INCLUDE_DIR}")

# Link SDL3
target_link_libraries(GameBoy PRIVATE SDL3::SDL3)

# On Windows copy the SDL3 runtime DLL next to the executable so launching outside VS works.
if (WIN32)
    add_custom_command(TARGET GameBoy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "[PostBuild] Copying SDL3 runtime DLL..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:GameBoy>/SDL3.dll"
    )
endif()

# Lightweight ROM analysis tool (no full emulator linkage needed)
add_executable(analyze_rom analyze_rom.cpp)
target_include_directories(analyze_rom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
