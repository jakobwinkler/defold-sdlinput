set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# GCC-style Clang driver (match extender)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-win32-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-win32-msvc)

set(CMAKE_AR llvm-ar)
set(CMAKE_RANLIB llvm-ranlib)

# Critical: prevent CMake from trying to link executables during configure
# We don't have CRT import libs available for linking test programs
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Match extender's system includes, adapted for xwin layout
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-resource-dir
    OUTPUT_VARIABLE CLANG_RESOURCE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# NOTE: winrt/ intentionally excluded. It contains windows.gaming.input.h which
# enables SDL's WGI joystick backend (needs -lsetupapi at link time). The extender
# doesn't link setupapi, so we disable WGI. XInput + DirectInput cover joysticks.
# Use single-line strings to avoid newlines in flags (breaks makefile generation)
set(CMAKE_C_FLAGS_INIT "-isystem ${CLANG_RESOURCE_DIR}/include -isystem /xwin/crt/include -isystem /xwin/sdk/include/ucrt -isystem /xwin/sdk/include/um -isystem /xwin/sdk/include/shared -static-libgcc -D_MT")

set(CMAKE_CXX_FLAGS_INIT "-nostdinc++ -isystem ${CLANG_RESOURCE_DIR}/include -isystem /xwin/crt/include -isystem /xwin/sdk/include/ucrt -isystem /xwin/sdk/include/um -isystem /xwin/sdk/include/shared -static-libgcc -static-libstdc++ -D_MT")

set(CMAKE_FIND_ROOT_PATH /xwin/crt /xwin/sdk)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
