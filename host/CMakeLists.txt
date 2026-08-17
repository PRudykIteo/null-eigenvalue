# The app: one window over the C++ synthesis engine.
#
# The engine sources are compiled straight into this target. There is no FFI
# boundary any more and no flat C surface to cross - `nulleig.h` is kept as the
# engine's public API because it is a clean line, not because anything is
# marshalled across it.
#
#   cmake -S host -B host/build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build host/build
cmake_minimum_required(VERSION 3.20)
project(null_eigenvalue CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../packages/nulleig/src")

# SDL3 is not packaged by any distribution this ships to yet, so a system copy
# is used when there is one (the dev container pre-builds it) and fetched at
# the pinned tag otherwise. Static on purpose: it removes an SDL3 shared
# library from all three packaging paths.
find_package(SDL3 QUIET CONFIG)
if(NOT SDL3_FOUND)
  include(FetchContent)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.14
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

add_executable(null_eigenvalue
  main.cpp
  palette.cpp
  textures.cpp
  nebula.cpp
  render.cpp
  hud.cpp
  panel.cpp
  net.cpp
  updater.cpp
  app.cpp
  piece.cpp
  text.cpp
  "${NE_SRC}/api.cpp"
  "${NE_SRC}/engine.cpp")

target_include_directories(null_eigenvalue PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}" "${NE_SRC}" "${NE_SRC}/third_party")

target_compile_definitions(null_eigenvalue PRIVATE NE_WITH_MINIAUDIO)

# The version the updater compares against the newest release tag. CI passes
# it; a build made on a developer machine has none and never offers an update,
# which is what you want while working on the app.
set(NE_VERSION "" CACHE STRING "version this build reports")
if(NE_VERSION)
  target_compile_definitions(null_eigenvalue PRIVATE NE_VERSION="${NE_VERSION}")
endif()

# Not interchangeable, and the reason is in the DSP rather than in taste.
# -fno-finite-math-only is mandatory alongside -ffast-math: without it the
# compiler may assume no infinities, and a feedback delay network handed a NaN
# never recovers. MSVC takes the default FP model precisely because /fp:fast
# has no way to say "fast, but infinities are real".
if(MSVC)
  target_compile_options(null_eigenvalue PRIVATE /O2 /EHsc)
  # miniaudio pulls in windows.h, which would otherwise rewrite std::min.
  target_compile_definitions(null_eigenvalue PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(null_eigenvalue PRIVATE -O3 -ffast-math -fno-finite-math-only)
endif()

target_link_libraries(null_eigenvalue PRIVATE SDL3::SDL3)

find_package(Threads REQUIRED)
target_link_libraries(null_eigenvalue PRIVATE Threads::Threads)

# The updater's two HTTPS calls. Windows uses WinHTTP, which is already in the
# OS; the others use libcurl, which is already on any machine this ships to.
if(NOT WIN32)
  find_package(CURL REQUIRED)
  target_link_libraries(null_eigenvalue PRIVATE CURL::libcurl)
endif()
if(WIN32)
  target_link_libraries(null_eigenvalue PRIVATE ole32 winmm winhttp)
elseif(NOT APPLE)
  # miniaudio finds ALSA, PulseAudio, PipeWire and JACK through dlopen, which
  # is what lets one binary run on a machine with any of them and dev packages
  # for none.
  target_link_libraries(null_eigenvalue PRIVATE ${CMAKE_DL_LIBS} m)
endif()

if(APPLE)
  target_link_libraries(null_eigenvalue PRIVATE
    "-framework AudioToolbox" "-framework CoreAudio" "-framework CoreFoundation")
endif()

# The token format is the one thing here that has to stay byte-identical
# forever, so it gets a test of its own rather than being checked by eye.
add_executable(piece_test tests/piece_test.cpp piece.cpp palette.cpp)
target_include_directories(piece_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${NE_SRC}")
add_executable(updater_test tests/updater_test.cpp updater.cpp net.cpp)
target_include_directories(updater_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(updater_test PRIVATE SDL3::SDL3)
if(WIN32)
  target_link_libraries(updater_test PRIVATE winhttp)
else()
  find_package(CURL REQUIRED)
  target_link_libraries(updater_test PRIVATE CURL::libcurl)
endif()

enable_testing()
add_test(NAME piece COMMAND piece_test)
add_test(NAME updater COMMAND updater_test)
