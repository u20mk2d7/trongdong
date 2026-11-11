cmake_minimum_required(VERSION 3.30)
project(stock LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    message(STATUS "[${PROJECT_NAME}] clang-tidy enabled: ${CLANG_TIDY_EXE}")
endif()

# ---------------------------------------------------------------------------
# Base Compiler Flags Target
# ---------------------------------------------------------------------------
set(CMAKE_CXX_FLAGS_RELEASE "" CACHE STRING "" FORCE)

add_library(bs_compiler_flags INTERFACE)

# Universal compilation flags (work on both macOS and Linux Clang)
target_compile_options(bs_compiler_flags INTERFACE
    $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native>
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast
    -ffunction-sections
    -fdata-sections
)

# Platform-specific compilation options
if(APPLE)
    # macOS (Apple Silicon M4) specific flags
    target_compile_options(bs_compiler_flags INTERFACE
        $<$<COMPILE_LANG_AND_ID:CXX,Clang>:-stdlib=libc++>
    )
    target_link_options(bs_compiler_flags INTERFACE
        $<$<CXX_COMPILER_ID:Clang>:-stdlib=libc++>
        "LINKER:-dead_strip"          # macOS equivalent of --gc-sections
        "LINKER:-bind_at_load"        # Similar security intent to -z,now on Linux
    )
endif()

if (NOT APPLE)

    # ---------------------------------------------------------------------------
    # Linux / Debian (Debian 13 / AMD Ryzen / Clang / LLD)
    # ---------------------------------------------------------------------------

    # 1. Enforce LLVM LLD linker (and optionally check for 'mold' if you want blazing-fast link times)
    target_link_options(bs_compiler_flags INTERFACE -fuse-ld=lld)

    # 2. Hot-Path & Hardening Linker Flags
    target_link_options(bs_compiler_flags INTERFACE
        "LINKER:-z,now"           # Immediate symbol binding: eliminates lazy PLT lookup
                                  # latency on the first call of a dynamic function.
        "LINKER:-z,relro"         # Read-only relocations after ELF loading for security.
        "LINKER:-z,combreloc"     # Sort relocations so internal data references are grouped.
        "LINKER:--gc-sections"    # Drop unreferenced code/data sections.
        "LINKER:--icf=safe"       # Identical Code Folding: merges identical functions
                                  # to drastically improve instruction cache (i-cache) efficiency.
    )

    # 3. Linux-Specific Compiler Optimizations
    target_compile_options(bs_compiler_flags INTERFACE
        -fno-plt                  # Avoid Procedure Linkage Table (PLT) entirely for
                                  # external calls where possible, forcing direct jumps.
        $<$<CONFIG:Release>:-march=native -mtune=native> # Target your exact AMD Ryzen CPU features (AVX2/AVX-512)
    )

    # 4. Optional: Concurrency Debugging (ThreadSanitizer)
    # HFT multi-threaded lock-free code is prone to data races.
    # Create a separate build profile for TSan: cmake -B build -DENABLE_TSAN=ON
    option(ENABLE_TSAN "Enable ThreadSanitizer for lock-free queue debugging" OFF)
    if(ENABLE_TSAN)
        message(STATUS "[${PROJECT_NAME}] ThreadSanitizer (TSan) ENABLED")
        target_compile_options(bs_compiler_flags INTERFACE -fsanitize=thread -g)
        target_link_options(bs_compiler_flags INTERFACE -fsanitize=thread)
    endif()
endif()


# ---------------------------------------------------------------------------
# Dependencies (Conan + PCH)
# ---------------------------------------------------------------------------
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)
find_package(jemalloc REQUIRED)
find_package(flatbuffers REQUIRED)
find_package(absl CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Platform / Linker / LTO
# ---------------------------------------------------------------------------
#if(NOT APPLE)
#    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#        target_link_options(bs_compiler_flags INTERFACE "LINKER:--icf=safe")
#    endif()

#    target_link_libraries(${PROJECT_NAME} PRIVATE jemalloc::jemalloc)

#endif()

# Single source of truth for LTO / IPO.
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    message(STATUS "[${PROJECT_NAME}] Enabling ThinLTO via CMake interprocedural optimization")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
else()
    message(WARNING "[${PROJECT_NAME}] IPO/LTO not supported: ${ipo_error}")
endif()

# ---------------------------------------------------------------------------
# Hot-path Isolation Target
# ---------------------------------------------------------------------------
add_library(bs_hot_path_base INTERFACE)
target_link_libraries(bs_hot_path_base INTERFACE bs_compiler_flags)

add_subdirectory(kimngwu)

if(TARGET ${PROJECT_NAME})
	set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON) # Recommended OFF for HFT
endif()

# ---------------------------------------------------------------------------
# Toolchain Information Printer
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "========== [Toolchain Summary] ==========")
message(STATUS "Host System          : ${CMAKE_SYSTEM_NAME}")
message(STATUS "C++ Compiler ID      : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ Compiler Binary  : ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ Compiler Version : ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Compile Commands     : ${CMAKE_EXPORT_COMPILE_COMMANDS}")
message(STATUS "IPO / ThinLTO        : ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
message(STATUS "==========================================")
