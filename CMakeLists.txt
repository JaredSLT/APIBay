cmake_minimum_required(VERSION 3.12)
project(TPBSearch C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

# Universal release flags
set(UNIVERSAL_FLAGS "-O3 -funroll-loops -fomit-frame-pointer -ffast-math")

# Platform-specific flags
if(WIN32)
    set(CMAKE_C_FLAGS_RELEASE "${UNIVERSAL_FLAGS} -march=native")
    set(CMAKE_C_FLAGS_DEBUG "${UNIVERSAL_FLAGS} -march=native")
    # Only add LTCG for MSVC, not for Clang
    if(MSVC)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
    endif()
else()
    set(CMAKE_C_FLAGS_RELEASE "${UNIVERSAL_FLAGS} -march=native -mtune=generic")
    set(CMAKE_C_FLAGS_DEBUG "${UNIVERSAL_FLAGS} -march=native")
endif()

# Compiler-specific flags
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    if(NOT WIN32)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto=auto -ffat-lto-objects")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -flto=auto")
    endif()
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-stack-protector")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-stack-protector")
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto=thin -Wno-gcc-compat")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -flto=thin -Wno-gcc-compat")
endif()

# MSVC specific flags
if(MSVC)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /GL /arch:AVX2")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /arch:AVX2")
endif()

# PGO support
option(PGO_GENERATE "Enable PGO instrumentation" OFF)
option(PGO_USE "Enable PGO optimization" OFF)

set(PGO_GEN_FLAG "")
set(PGO_USE_FLAG "")

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(PGO_GEN_FLAG "-fprofile-generate")
    set(PGO_USE_FLAG "-fprofile-use -fprofile-correction")
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    set(PGO_GEN_FLAG "-fprofile-instr-generate")
    set(PGO_USE_FLAG "-fprofile-instr-use")
endif()

if(PGO_GENERATE)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${PGO_GEN_FLAG}")
endif()

if(PGO_USE)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${PGO_USE_FLAG}")
endif()

# Cross-compilation support
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_BITS "64")
else()
    set(ARCH_BITS "32")
endif()

# Set architecture-specific flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(ARCH_FLAGS "-march=x86-64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|i686")
    set(ARCH_FLAGS "-march=i686")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(ARCH_FLAGS "-march=armv8-a")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    set(ARCH_FLAGS "-march=armv7-a")
endif()

# Only add architecture flags for non-Windows or non-MSVC builds
if(NOT WIN32 OR NOT MSVC)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${ARCH_FLAGS}")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${ARCH_FLAGS}")
endif()

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Use Ninja generator
set(CMAKE_GENERATOR "Ninja")

# Add multiple build targets for different architectures
add_executable(TPBSearch main.c)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(X86_ARCHS "core2" "nehalem" "sandybridge" "haswell" "skylake" "meteorlake")  # core2 for Core 2
    foreach(ARCH ${X86_ARCHS})
        add_executable(TPBSearch_${ARCH} main.c)
        set_target_properties(TPBSearch_${ARCH} PROPERTIES COMPILE_FLAGS "-march=${ARCH}")
        if(WIN32)
            target_link_libraries(TPBSearch_${ARCH} PRIVATE ws2_32 mswsock secur32 shlwapi OpenSSL::SSL OpenSSL::Crypto)
        else()
            target_link_libraries(TPBSearch_${ARCH} PRIVATE pthread OpenSSL::SSL OpenSSL::Crypto)
        endif()
    endforeach()
    # For 32-bit x86
    add_executable(TPBSearch_i686 main.c)
    set_target_properties(TPBSearch_i686 PROPERTIES COMPILE_FLAGS "-m32 -march=core2" LINK_FLAGS "-m32")
    if(WIN32)
        target_link_libraries(TPBSearch_i686 PRIVATE ws2_32 mswsock secur32 shlwapi OpenSSL::SSL OpenSSL::Crypto)
    else()
        target_link_libraries(TPBSearch_i686 PRIVATE pthread OpenSSL::SSL OpenSSL::Crypto)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64*")
    set(POWER_ARCHS "power7" "power8" "power9")
    foreach(ARCH ${POWER_ARCHS})
        add_executable(TPBSearch_${ARCH} main.c)
        set_target_properties(TPBSearch_${ARCH} PROPERTIES COMPILE_FLAGS "-mcpu=${ARCH} -mtune=${ARCH}")
        target_link_libraries(TPBSearch_${ARCH} PRIVATE pthread OpenSSL::SSL OpenSSL::Crypto)
    endforeach()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    set(RISCV_ARCHS "rv64gc" "rv64gcv")
    foreach(ARCH ${RISCV_ARCHS})
        add_executable(TPBSearch_${ARCH} main.c)
        set_target_properties(TPBSearch_${ARCH} PROPERTIES COMPILE_FLAGS "-march=${ARCH}")
        target_link_libraries(TPBSearch_${ARCH} PRIVATE pthread OpenSSL::SSL OpenSSL::Crypto)
    endforeach()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    add_executable(only TPBSearch_armv8 main.c)
    set_target_properties(TPBSearch_armv8 PROPERTIES COMPILE_FLAGS "-march=armv8-a")
    target_link_libraries(TPBSearch_armv8 PRIVATE pthread OpenSSL::SSL OpenSSL::Crypto)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    add_executable(TPBSearch_armv7 main.c)
    set_target_properties(TPBSearch_armv7 PROPERTIES COMPILE_FLAGS "-march=armv7-a -mfpu=neon")
    target_link_libraries(TPBSearch_armv7 PRIVATE pthread OpenSSL::SSL OpenSSL::Crypto)
endif()

# Platform-specific linking for default target
if(WIN32)
    target_link_libraries(TPBSearch PRIVATE ws2_32 mswsock secur32 shlwapi OpenSSL::SSL OpenSSL::Crypto)
else()
    target_link_libraries(TPBSearch PRIVATE pthread OpenSSL::SSL OpenSSL::Crypto)
endif()

# Strip executable in release builds
if(NOT DEFINED CMAKE_STRIP)
    message(WARNING "CMAKE_STRIP not defined")
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET TPBSearch
                POST_BUILD
                COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:TPBSearch>
        )
    endif()
endif()

# Add compile definitions for platform detection
if(WIN32)
    target_compile_definitions(TPBSearch PRIVATE _WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(TPBSearch PRIVATE _WIN64)
    endif()
endif()

# Set CMAKE_POSITION_INDEPENDENT_CODE for shared libraries if needed
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
