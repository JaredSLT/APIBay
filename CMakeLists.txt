cmake_minimum_required(VERSION 3.12)
project(TPBSearch C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -mtune=native -funroll-loops -fomit-frame-pointer -fno-stack-protector -ffast-math")

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

find_package(CURL CONFIG REQUIRED)

add_executable(TPBSearch main.c)

target_link_libraries(TPBSearch PRIVATE CURL::libcurl)

if(WIN32)
    target_link_libraries(TPBSearch PRIVATE ws2_32 mswsock)
endif()

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