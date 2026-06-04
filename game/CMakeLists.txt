cmake_minimum_required(VERSION 3.16)
project(MM2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(MM2_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(MM2_DECOMP ${MM2_ROOT}/EXTRACTED/decomp)

# AGA ACE (same fork as LandsOfLore — see ../Amiga/LandsOfLore/CMakeLists.txt)
set(ACE_USE_AGA_FEATURES ON CACHE BOOL "ACE AGA features" FORCE)
set(ACE_USE_ECS_FEATURES ON CACHE BOOL "ACE ECS features" FORCE)

# ---------------------------------------------------------------------------
# Host selection: Amiga (ACE + Bartman) vs desktop (SDL2)
# ---------------------------------------------------------------------------
if(AMIGA)
  message(STATUS "[MM2] Amiga / ACE target (Bartman m68k-amiga-elf)")
  add_compile_definitions(MM2_HOST_AMIGA MM2_FORCE_AMIGA)
  set(MM2_PLATFORM_SOURCES src/platform/amiga/PlatformAmiga.cpp)
  set(MM2_MAIN_SOURCE src/main_amiga.cpp)

  set(FETCHCONTENT_UPDATES_DISCONNECTED OFF)
  include(FetchContent)
  FetchContent_Declare(
    ace_engine
    GIT_REPOSITORY https://github.com/Vairn/ACE.git
    GIT_TAG AGA
    GIT_SHALLOW TRUE
  )
  FetchContent_GetProperties(ace_engine)
  if(NOT ace_engine_POPULATED)
    FetchContent_Populate(ace_engine)
  endif()
  set(_ace_simplebuffer "${ace_engine_SOURCE_DIR}/src/ace/managers/viewport/simplebuffer.c")
  if(EXISTS "${_ace_simplebuffer}")
    file(READ "${_ace_simplebuffer}" _ace_simplebuffer_src)
    string(
      REPLACE "uwDDFStep = simpleBufferGetDDFStep(pManager);"
      "uwDDFStep = simpleBufferGetDDfStep(pManager);"
      _ace_simplebuffer_src "${_ace_simplebuffer_src}"
    )
    file(WRITE "${_ace_simplebuffer}" "${_ace_simplebuffer_src}")
  endif()
  unset(CPM_DIRECTORY CACHE)
  unset(CPM_FILE CACHE)
  unset(CPM_VERSION CACHE)
  unset(CPM_PACKAGES CACHE)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ACE_DEBUG ON CACHE BOOL "ACE debug logging" FORCE)
  else()
    set(ACE_DEBUG OFF CACHE BOOL "ACE debug logging" FORCE)
  endif()
  add_subdirectory(${ace_engine_SOURCE_DIR} ${ace_engine_BINARY_DIR})
else()
  message(STATUS "[MM2] Desktop / SDL2 target")
  add_compile_definitions(MM2_HOST_SDL)
  set(MM2_PLATFORM_SOURCES src/platform/sdl/PlatformSDL.cpp)
  set(MM2_MAIN_SOURCE src/main.cpp)

  include(FetchContent)
  FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.10
  )
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL2)
endif()

# ---------------------------------------------------------------------------
# Shared C codecs (no C++ / STL)
# ---------------------------------------------------------------------------
set(MM2_CODEC_SOURCES
  ${MM2_DECOMP}/mm2_image32_codec.c
  ${MM2_DECOMP}/mm2_anm_codec.c
  ${MM2_DECOMP}/mm2_roster_codec.c
  ${MM2_DECOMP}/mm2_create_character.c
  ${MM2_DECOMP}/mm2_monsters_codec.c
  ${MM2_DECOMP}/mm2_event_codec.c
  ${MM2_DECOMP}/mm2_attrib_codec.c
  ${MM2_DECOMP}/mm2_party_launch.c
)

if(NOT AMIGA)
  add_library(mm2_codecs STATIC ${MM2_CODEC_SOURCES})
  target_include_directories(mm2_codecs PUBLIC ${MM2_DECOMP})
endif()

if(NOT AMIGA)
  add_executable(image32_golden tests/image32_golden.c)
  target_link_libraries(image32_golden PRIVATE mm2_codecs)

  add_executable(font_golden tests/font_golden.cpp src/gfx/ScreenCompositor.cpp)
  target_include_directories(font_golden PRIVATE include)
  target_link_libraries(font_golden PRIVATE mm2_codecs)
endif()

# ---------------------------------------------------------------------------
# Character UI plugin (classic Amiga-faithful vs text stub)
# ---------------------------------------------------------------------------
set(MM2_UI "classic" CACHE STRING "Character UI implementation: classic|stub")
set_property(CACHE MM2_UI PROPERTY STRINGS classic stub)
if(MM2_UI STREQUAL "stub")
  add_compile_definitions(MM2_UI_STUB)
elseif(MM2_UI STREQUAL "classic")
  add_compile_definitions(MM2_UI_CLASSIC)
else()
  message(FATAL_ERROR "MM2_UI must be 'classic' or 'stub' (got '${MM2_UI}')")
endif()

# ---------------------------------------------------------------------------
# Game executable
# ---------------------------------------------------------------------------
set(MM2_GAME_SOURCES
  ${MM2_MAIN_SOURCE}
  src/Game.cpp
  src/GameSession.cpp
  src/TitleScreen.cpp
  src/gfx/ScreenCompositor.cpp
  src/gfx/View3D.cpp
  src/gfx/PlayScreenChrome.cpp
  src/runtime/Mm2Alloc.cpp
  src/ui/CharacterUiKind.cpp
  src/ui/CharacterUiFactory.cpp
  src/ui/CharacterUiController.cpp
  src/ui/StubCharacterUi.cpp
  src/ui/AmigaCharacterUi.cpp
  ${MM2_PLATFORM_SOURCES}
)

if(AMIGA)
  # Bartman -flto -fwhole-program: codecs in the same link unit as ACE/bartman, not a .a.
  list(APPEND MM2_GAME_SOURCES
    ${MM2_CODEC_SOURCES}
    src/platform/amiga/mm2_amiga_display.c
    src/platform/amiga/mm2_amiga_planar.c
  )
endif()

if(AMIGA AND ELF2HUNK)
  set(MM2_LINKED mm2.elf)
  set(MM2_OUTPUT mm2.exe)
  set(ELF2HUNK_EXTRA_OPTS "")
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(ELF2HUNK_EXTRA_OPTS "-s")
  endif()
  add_executable(${MM2_LINKED} ${MM2_GAME_SOURCES})
  set(_mm2_target ${MM2_LINKED})
  add_custom_command(
    TARGET ${MM2_LINKED} POST_BUILD
    COMMAND ${ELF2HUNK} ${MM2_LINKED} ${MM2_OUTPUT} ${ELF2HUNK_EXTRA_OPTS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "ELF2HUNK ${MM2_LINKED} -> ${MM2_OUTPUT}"
  )
else()
  add_executable(mm2 ${MM2_GAME_SOURCES})
  set(_mm2_target mm2)
  if(AMIGA)
    set(MM2_OUTPUT mm2)
  endif()
endif()

target_include_directories(${_mm2_target} PRIVATE include)

if(AMIGA)
  target_compile_options(${_mm2_target} PRIVATE -ffreestanding -fno-exceptions -fno-rtti)
  target_compile_options(${_mm2_target} PUBLIC -Wall -Wextra)
  target_include_directories(${_mm2_target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/freestanding_stl
    ${MM2_DECOMP}
    ${ace_engine_SOURCE_DIR}/include/mini_std
    ${ace_engine_SOURCE_DIR}/include
  )
  target_compile_definitions(${_mm2_target} PRIVATE
    AMIGA
    MM2_CODEC_AMIGA=1
    ACE_USE_AGA_FEATURES
    ACE_USE_ECS_FEATURES
    MM2_AGA_SCREEN_BPP=6
    MM2_AGA_SCREEN_WIDTH=320
    MM2_AGA_SCREEN_HEIGHT=200
  )
  # LoL links only `ace` (mini_std + bartman_gcc_support are inside it).
  target_link_libraries(${_mm2_target} PRIVATE ace)
  # -fwhole-program lets GCC inline and DCE memset/memcpy from gcc8_c_support before
  # the C++ lowering pass re-emits calls to them. -fno-whole-program at link time
  # (last flag wins) keeps those symbols externally visible through the LTO unit.
  target_link_options(${_mm2_target} PRIVATE -fno-whole-program)
  target_link_options(${_mm2_target} PUBLIC -Wl,-Map=mm2.map)
else()
  target_link_libraries(${_mm2_target} PRIVATE mm2_codecs SDL2::SDL2main SDL2::SDL2)
endif()

target_compile_definitions(${_mm2_target} PRIVATE
  $<$<CONFIG:Debug>:MM2_DEBUG ACE_DEBUG GAME_DEBUG>
)

message(STATUS "[MM2] Data files expected beside binary or passed as argv[1] (MM2 install dir)")
if(AMIGA AND ELF2HUNK)
  message(STATUS "[MM2] Amiga output: ${CMAKE_CURRENT_BINARY_DIR}/${MM2_OUTPUT}")
endif()
