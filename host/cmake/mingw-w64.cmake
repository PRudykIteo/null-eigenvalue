# Cross-compile to a native Windows .exe from Linux.
#
# This is how the Windows build is actually produced, not a convenience: the
# app links SDL3 and the synthesis engine statically and reaches the network
# through WinHTTP, so nothing MSVC-specific is left, and one Linux job is
# cheaper than a second runner image.
#
#   cmake -S host -B host/build -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=host/cmake/mingw-w64.cmake \
#     -DCMAKE_PREFIX_PATH=/path/to/sdl3-for-windows
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
# The posix thread model is not optional: the engine's watchdog is a
# std::thread and the win32 model has no std::thread at all.
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# One file to hand over, so the installer has nothing to carry beside it.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")
