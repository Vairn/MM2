cmake_minimum_required(VERSION 3.16)
project(MM2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

get_filename_component(MM2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(MM2_DECOMP ${MM2_ROOT}/EXTRACTED/decomp)

# AGA ACE — upstream AmigaPorts/ACE (ACE_USE_AGA_FEATURES on master).
set(ACE_USE_AGA_FEATURES ON CACHE BOOL "ACE AGA features" FORCE)
set(ACE_USE_ECS_FEATURES ON CACHE BOOL "ACE ECS features" FORCE)

# ---------------------------------------------------------------------------
# Host selection: Amiga (ACE + Bartman) vs desktop (SDL2)
# ---------------------------------------------------------------------------
if(AMIGA)
  message(STATUS "[MM2] Amiga / ACE target (Bartman m68k-amiga-elf)")
  add_compile_definitions(MM2_HOST_AMIGA MM2_FORCE_AMIGA)
  set(MM2_PLATFORM_SOURCES
    src/platform/amiga/PlatformAmiga.cpp
    src/platform/amiga/AudioAmiga.cpp
    src/platform/amiga/mm2_amiga_audio.c
  )
  set(MM2_MAIN_SOURCE src/main_amiga.cpp)

  # Presets may set ACE_DEBUG=ON for RelWithDebInfo (optimized + symbols + MM2_DBG spam).
  # Default ON for Debug, OFF otherwise; never FORCE — cacheVariables from presets win.
  if(NOT DEFINED ACE_DEBUG)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(ACE_DEBUG ON CACHE BOOL "ACE debug logging (MM2_DBG / logWrite)")
    else()
      set(ACE_DEBUG OFF CACHE BOOL "ACE debug logging (MM2_DBG / logWrite)")
    endif()
  endif()
  if(NOT DEFINED ACE_DEBUG_UAE)
    set(ACE_DEBUG_UAE ${ACE_DEBUG} CACHE BOOL "ACE logWrite -> WinUAE/Bartman console")
  endif()

  # Stale CPM_DIRECTORY from a prior ACE fork breaks AmigaPorts/ACE's include(cmake/CPM.cmake).
  unset(CPM_DIRECTORY CACHE)
  unset(CPM_FILE CACHE)
  unset(CPM_VERSION CACHE)
  unset(CPM_PACKAGES CACHE)

  set(FETCHCONTENT_UPDATES_DISCONNECTED OFF)
  include(FetchContent)
  FetchContent_Declare(
    amigaports_ace
    GIT_REPOSITORY https://github.com/AmigaPorts/ACE.git
    GIT_TAG d2cc8d2166e403eb7575e03260447852d177e390
  )
  FetchContent_MakeAvailable(amigaports_ace)
  set(ace_engine_SOURCE_DIR "${amigaports_ace_SOURCE_DIR}")
  set(ace_engine_BINARY_DIR "${amigaports_ace_BINARY_DIR}")
  if(ACE_DEBUG)
    # LoL pattern: ACE_DEBUG_ALL enables logWrite; ACE_DEBUG_UAE routes to UAE debugger.
    target_compile_definitions(ace PUBLIC ACE_DEBUG_ALL ACE_DEBUG_UAE)
  endif()
else()
  message(STATUS "[MM2] Desktop / SDL2 target")
  add_compile_definitions(MM2_HOST_SDL)
  set(MM2_PLATFORM_SOURCES
    src/platform/sdl/PlatformSDL.cpp
    src/platform/sdl/AudioSDL.cpp
  )
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
  ${MM2_DECOMP}/mm2_gfx_sheet.c
  ${MM2_DECOMP}/mm2_pc_gfx_codec.c
  ${MM2_DECOMP}/mm2_pc_monsters_codec.c
  ${MM2_DECOMP}/mm2_anm_codec.c
  ${MM2_DECOMP}/mm2_anm_preview.c
  ${MM2_DECOMP}/mm2_roster_codec.c
  ${MM2_DECOMP}/mm2_create_character.c
  ${MM2_DECOMP}/mm2_monsters_codec.c
  ${MM2_DECOMP}/mm2_event_codec.c
  ${MM2_DECOMP}/mm2_attrib_codec.c
  ${MM2_DECOMP}/mm2_map_codec.c
  ${MM2_DECOMP}/mm2_party_launch.c
  ${MM2_DECOMP}/mm2_items_codec.c
  ${MM2_DECOMP}/mm2_found_items.c
  ${MM2_DECOMP}/mm2_town_tables.c
  ${MM2_DECOMP}/mm2_encounter.c
  ${MM2_DECOMP}/mm2_copyprot_codec.c
  ${MM2_DECOMP}/mm2_globe_blob.c
)

if(NOT AMIGA)
  add_library(mm2_codecs STATIC ${MM2_CODEC_SOURCES})
  target_include_directories(mm2_codecs PUBLIC ${MM2_DECOMP})

  # Tests link Movement/EventVmHelpers/CombatSession without SDL audio.
  add_library(mm2_audio_stub STATIC src/platform/AudioStub.cpp)
  target_include_directories(mm2_audio_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

# GameSession + automap_golden share automap implementation (Cartography.h is header-only).
set(MM2_AUTOMAP_SOURCES
  src/gfx/AutomapView.cpp
  src/world/AutomapState.cpp
  src/gameplay/RosterSkills.cpp
)

if(NOT AMIGA)
  add_executable(image32_golden tests/image32_golden.c)
  target_link_libraries(image32_golden PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(roster_tail_test tests/roster_tail_test.c)
  target_link_libraries(roster_tail_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(pc_gfx_rgba_dump tests/pc_gfx_rgba_dump.c)
  target_link_libraries(pc_gfx_rgba_dump PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(pc_monsters_rgba_dump tests/pc_monsters_rgba_dump.c)
  target_link_libraries(pc_monsters_rgba_dump PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(pc_gfx_pixels_dump tests/pc_gfx_pixels_dump.c)
  target_link_libraries(pc_gfx_pixels_dump PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(pc_lzw_dump tests/pc_lzw_dump.c)
  target_link_libraries(pc_lzw_dump PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(font_golden tests/font_golden.cpp src/gfx/ScreenCompositor.cpp src/gfx/mm2_font8x8.c)
  target_include_directories(font_golden PRIVATE include)
  target_link_libraries(font_golden PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(view3d_middlegate_check
    tests/view3d_middlegate_check.cpp
    src/gfx/View3D.cpp
    src/world/MapWorld.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(view3d_middlegate_check PRIVATE include)
  target_link_libraries(view3d_middlegate_check PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(view3d_torch_door_test
    tests/view3d_torch_door_test.cpp
    src/gfx/View3D.cpp
    src/world/MapWorld.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(view3d_torch_door_test PRIVATE include)
  target_link_libraries(view3d_torch_door_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(view3d_outdoor_check
    tests/view3d_outdoor_check.cpp
    src/gfx/OutdoorView3D.cpp
    src/world/MapWorld.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(view3d_outdoor_check PRIVATE include)
  target_link_libraries(view3d_outdoor_check PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(movement_middlegate_test
    tests/movement_middlegate_test.cpp
    src/gameplay/Movement.cpp
    src/gameplay/ExploreActions.cpp
    src/events/EventVmHelpers.cpp
    src/world/MapWorld.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(movement_middlegate_test PRIVATE include)
  target_link_libraries(movement_middlegate_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(calendar_rollover_test
    tests/calendar_rollover_test.cpp
    src/gameplay/Movement.cpp
    src/gameplay/ExploreActions.cpp
    src/events/EventVmHelpers.cpp
    src/world/MapWorld.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(calendar_rollover_test PRIVATE include)
  target_link_libraries(calendar_rollover_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(combat_setup_test
    tests/combat_setup_test.cpp
    src/combat/EncounterPicker.cpp
    src/combat/CombatSession.cpp
    src/events/EventVmHelpers.cpp
    src/events/TownServiceTransactions.cpp
    src/gameplay/ExploreActions.cpp
    src/gameplay/Movement.cpp
    src/gameplay/RosterSkills.cpp
    src/world/MapWorld.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(combat_setup_test PRIVATE include)
  target_link_libraries(combat_setup_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(playsession_input_test
    tests/playsession_input_test.cpp
    src/gameplay/PlaySessionInput.cpp)
  target_include_directories(playsession_input_test PRIVATE include)
  target_link_libraries(playsession_input_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(explore_commands_test
    tests/explore_commands_test.cpp
    src/gameplay/ExploreActions.cpp
    src/gameplay/Movement.cpp
    src/events/EventVmHelpers.cpp
    src/world/MapWorld.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(explore_commands_test PRIVATE include)
  target_link_libraries(explore_commands_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(automap_golden
    tests/automap_golden.cpp
    ${MM2_AUTOMAP_SOURCES}
    src/events/EventVmHelpers.cpp
    src/gameplay/ExploreActions.cpp
    src/gfx/ScreenCompositor.cpp
    src/gfx/mm2_font8x8.c
    src/gfx/EnvAssets.cpp
    src/gfx/GfxBackend.cpp
    src/gfx/OutdoorView3D.cpp
    src/world/MapWorld.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(automap_golden PRIVATE include)
  target_link_libraries(automap_golden PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(character_sheet_input_test
    tests/character_sheet_input_test.cpp
    src/gameplay/InGameCharacterSheet.cpp
    src/gameplay/RosterSkills.cpp
    src/ui/RosterSkillDisplay.cpp
    src/gfx/GfxBackend.cpp
    src/gfx/ScreenCompositor.cpp
    src/gfx/PlayScreenChrome.cpp
    src/gfx/PartyStatusFormat.cpp
    src/gfx/mm2_font8x8.c
    src/runtime/PathScratch.cpp)
  target_include_directories(character_sheet_input_test PRIVATE include)
  target_link_libraries(character_sheet_input_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(party_hp_format_test
    tests/party_hp_format_test.cpp
    src/gfx/PartyStatusFormat.cpp)
  target_include_directories(party_hp_format_test PRIVATE include)
  target_link_libraries(party_hp_format_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(event_middlegate_test
    tests/event_middlegate_test.cpp
    src/events/EventRuntime.cpp
    src/events/EventVmHelpers.cpp
    src/events/EventPartyEffects.cpp
    src/events/EventTownServices.cpp
    src/events/EventSkillBuy.cpp
    src/events/TownServiceTransactions.cpp
    src/events/TownServiceMenu.cpp
    src/events/EventCombatEncounter.cpp
    src/events/EventTextView.cpp
    src/events/ServiceSignResolver.cpp
    src/events/ScriptedSceneEngine.cpp
    src/combat/EncounterPicker.cpp
    src/combat/CombatSession.cpp
    src/gameplay/ExploreActions.cpp
    src/gameplay/Movement.cpp
    src/gameplay/RosterSkills.cpp
    src/ui/PlayTownServiceUi.cpp
    src/gfx/ViewportAnmOverlay.cpp
    src/gfx/GfxBackend.cpp
    src/gfx/ScreenCompositor.cpp
    src/gfx/PlayScreenChrome.cpp
    src/gfx/CombatPanel.cpp
    src/gfx/PartyStatusFormat.cpp
    src/gfx/mm2_font8x8.c
    src/world/MapWorld.cpp
    src/runtime/Mm2Alloc.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(event_middlegate_test PRIVATE include)
  target_link_libraries(event_middlegate_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(event_town_skills_test
    tests/event_town_skills_test.cpp
    src/events/EventRuntime.cpp
    src/events/EventVmHelpers.cpp
    src/events/EventPartyEffects.cpp
    src/events/EventTownServices.cpp
    src/events/EventSkillBuy.cpp
    src/events/TownServiceTransactions.cpp
    src/events/TownServiceMenu.cpp
    src/events/EventCombatEncounter.cpp
    src/events/EventTextView.cpp
    src/events/ServiceSignResolver.cpp
    src/events/ScriptedSceneEngine.cpp
    src/combat/EncounterPicker.cpp
    src/combat/CombatSession.cpp
    src/gameplay/ExploreActions.cpp
    src/gameplay/Movement.cpp
    src/gameplay/RosterSkills.cpp
    src/gfx/ViewportAnmOverlay.cpp
    src/gfx/GfxBackend.cpp
    src/gfx/ScreenCompositor.cpp
    src/gfx/PlayScreenChrome.cpp
    src/gfx/PartyStatusFormat.cpp
    src/gfx/mm2_font8x8.c
    src/world/MapWorld.cpp
    src/runtime/Mm2Alloc.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(event_town_skills_test PRIVATE include)
  target_link_libraries(event_town_skills_test PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(event_op_demo
    tests/event_op_demo.cpp
    src/events/EventTextView.cpp
    src/events/ServiceSignResolver.cpp
    src/events/ScriptedSceneEngine.cpp
    src/gfx/ViewportAnmOverlay.cpp
    src/gfx/GfxBackend.cpp
    src/gfx/ScreenCompositor.cpp
    src/gfx/mm2_font8x8.c
    src/gfx/View3D.cpp
    src/gfx/OutdoorView3D.cpp
    src/gfx/PlayScreenChrome.cpp
    src/gfx/PartyStatusFormat.cpp
  src/gfx/EnvAssets.cpp
  src/gfx/GfxBackend.cpp
  src/world/MapWorld.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(event_op_demo PRIVATE include)
  target_link_libraries(event_op_demo PRIVATE mm2_codecs mm2_audio_stub)

  add_executable(playscreen_golden
    tests/playscreen_golden.cpp
    src/GameSession.cpp
    ${MM2_AUTOMAP_SOURCES}
    ${MM2_PLATFORM_SOURCES}
    src/events/ScriptedSceneEngine.cpp
    src/gameplay/Movement.cpp
    src/gameplay/PlaySessionInput.cpp
    src/gameplay/ExploreActions.cpp
    src/gameplay/InGameCharacterSheet.cpp
    src/ui/RosterSkillDisplay.cpp
    src/gameplay/InGameControlsScreen.cpp
    src/gfx/ScreenCompositor.cpp
    src/gfx/mm2_font8x8.c
    src/gfx/View3D.cpp
    src/gfx/OutdoorView3D.cpp
    src/gfx/PlayScreenChrome.cpp
    src/gfx/CombatPanel.cpp
    src/gfx/PartyStatusFormat.cpp
  src/gfx/EnvAssets.cpp
  src/gfx/GfxBackend.cpp
  src/world/MapWorld.cpp
    src/events/EventRuntime.cpp
    src/events/EventVmHelpers.cpp
    src/events/EventPartyEffects.cpp
    src/events/EventTownServices.cpp
    src/events/EventSkillBuy.cpp
    src/events/TownServiceTransactions.cpp
    src/events/TownServiceMenu.cpp
    src/events/EventCombatEncounter.cpp
    src/events/EventTextView.cpp
    src/events/ServiceSignResolver.cpp
    src/combat/EncounterPicker.cpp
    src/combat/CombatSession.cpp
    src/ui/PlayTownServiceUi.cpp
    src/gfx/ViewportAnmOverlay.cpp
    src/runtime/Mm2Alloc.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(playscreen_golden PRIVATE include)
  target_link_libraries(playscreen_golden PRIVATE mm2_codecs SDL2::SDL2)

  add_executable(combat_golden
    tests/combat_golden.cpp
    src/GameSession.cpp
    ${MM2_AUTOMAP_SOURCES}
    ${MM2_PLATFORM_SOURCES}
    src/events/ScriptedSceneEngine.cpp
    src/gameplay/Movement.cpp
    src/gameplay/PlaySessionInput.cpp
    src/gameplay/ExploreActions.cpp
    src/gameplay/InGameCharacterSheet.cpp
    src/ui/RosterSkillDisplay.cpp
    src/gameplay/InGameControlsScreen.cpp
    src/gfx/ScreenCompositor.cpp
    src/gfx/mm2_font8x8.c
    src/gfx/View3D.cpp
    src/gfx/OutdoorView3D.cpp
    src/gfx/PlayScreenChrome.cpp
    src/gfx/CombatPanel.cpp
    src/gfx/PartyStatusFormat.cpp
    src/gfx/EnvAssets.cpp
    src/gfx/GfxBackend.cpp
    src/world/MapWorld.cpp
    src/events/EventRuntime.cpp
    src/events/EventVmHelpers.cpp
    src/events/EventPartyEffects.cpp
    src/events/EventTownServices.cpp
    src/events/EventSkillBuy.cpp
    src/events/TownServiceTransactions.cpp
    src/events/TownServiceMenu.cpp
    src/events/EventCombatEncounter.cpp
    src/events/EventTextView.cpp
    src/events/ServiceSignResolver.cpp
    src/combat/EncounterPicker.cpp
    src/combat/CombatSession.cpp
    src/ui/PlayTownServiceUi.cpp
    src/gfx/ViewportAnmOverlay.cpp
    src/runtime/Mm2Alloc.cpp
    src/runtime/PathScratch.cpp)
  target_include_directories(combat_golden PRIVATE include)
  target_link_libraries(combat_golden PRIVATE mm2_codecs SDL2::SDL2)
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
  src/GameSession.cpp
  src/gameplay/Movement.cpp
  src/gameplay/PlaySessionInput.cpp
  src/gameplay/ExploreActions.cpp
  src/gameplay/InGameCharacterSheet.cpp
  src/gameplay/InGameControlsScreen.cpp
  src/gameplay/RosterSkills.cpp
  src/TitleScreen.cpp
  src/states/mm2_state.c
  src/states/mm2_app_context.c
  src/states/mm2_goto_trace.c
  src/states/mm2_states.cpp
  src/states/AppHost.cpp
  src/states/mm2_app_run.cpp
  src/gfx/ScreenCompositor.cpp
  src/gfx/mm2_font8x8.c
  src/gfx/View3D.cpp
  src/gfx/OutdoorView3D.cpp
  src/gfx/PlayScreenChrome.cpp
  src/gfx/CombatPanel.cpp
  src/gfx/PartyStatusFormat.cpp
  src/gfx/EnvAssets.cpp
  src/gfx/GfxBackend.cpp
  ${MM2_AUTOMAP_SOURCES}
  src/world/MapWorld.cpp
  src/combat/EncounterPicker.cpp
  src/combat/CombatSession.cpp
  src/events/EventRuntime.cpp
  src/events/EventVmHelpers.cpp
  src/events/EventPartyEffects.cpp
  src/events/EventTownServices.cpp
  src/events/EventSkillBuy.cpp
  src/events/TownServiceTransactions.cpp
  src/events/TownServiceMenu.cpp
  src/events/EventCombatEncounter.cpp
  src/events/EventTextView.cpp
  src/events/ServiceSignResolver.cpp
  src/events/ScriptedSceneEngine.cpp
  src/gfx/ViewportAnmOverlay.cpp
  src/runtime/Mm2Alloc.cpp
  src/runtime/PathScratch.cpp
  src/ui/CharacterUiKind.cpp
  src/ui/CharacterUiFactory.cpp
  src/ui/CharacterUiController.cpp
  src/ui/StubCharacterUi.cpp
  src/ui/AmigaCharacterUi.cpp
  src/ui/RosterSkillDisplay.cpp
  src/ui/PlayTownServiceUi.cpp
  ${MM2_PLATFORM_SOURCES}
)

if(AMIGA)
  # Bartman -flto -fwhole-program: codecs in the same link unit as ACE/bartman, not a .a.
  list(APPEND MM2_GAME_SOURCES
    ${MM2_CODEC_SOURCES}
    src/platform/amiga/mm2_amiga_display.c
    src/platform/amiga/mm2_amiga_planar.c
    src/platform/amiga/mm2_amiga_file.c
    src/platform/amiga/mm2_fade.c
    src/gfx/AnmPlanarPool.cpp
  )
endif()

if(AMIGA AND ELF2HUNK)
  set(MM2_LINKED mm2.elf)
  set(MM2_OUTPUT mm2.exe)
  set(ELF2HUNK_EXTRA_OPTS "")
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
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
  if(M68K_LDFLAGS)
    target_link_options(${_mm2_target} PRIVATE ${M68K_LDFLAGS})
  endif()
else()
  target_link_libraries(${_mm2_target} PRIVATE mm2_codecs SDL2::SDL2main SDL2::SDL2)
  # Pre-rendered Paula exports (tools/export_mm2_amiga_sfx.py) beside the binary.
  add_custom_command(
    TARGET ${_mm2_target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${_mm2_target}>/audio"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${MM2_ROOT}/EXTRACTED/audio"
      "$<TARGET_FILE_DIR:${_mm2_target}>/audio"
    COMMENT "[MM2] Copy EXTRACTED/audio WAVs beside desktop binary"
    VERBATIM
  )
endif()

if(AMIGA AND ACE_DEBUG)
  target_compile_definitions(${_mm2_target} PRIVATE ACE_DEBUG MM2_DEBUG)
endif()
target_compile_definitions(${_mm2_target} PRIVATE
  $<$<CONFIG:Debug>:GAME_DEBUG>
)

if(AMIGA)
  # WinUAE/Bartman cwd is the build output dir; argv data path is often unset.
  add_custom_command(
    TARGET ${_mm2_target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      -DMM2_ROOT=${MM2_ROOT}
      -DDEST=$<TARGET_FILE_DIR:${_mm2_target}>
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_mm2_data.cmake"
    COMMENT "[MM2] Copy .dat/.32/.anm from repo root into Amiga output dir"
    VERBATIM
  )
  message(STATUS "[MM2] Amiga: post-build copies data from ${MM2_ROOT} -> build dir")
endif()

message(STATUS "[MM2] Data: beside binary (Amiga post-build) or argv[1] / ../ on desktop")
if(AMIGA AND ELF2HUNK)
  message(STATUS "[MM2] Amiga output: ${CMAKE_CURRENT_BINARY_DIR}/${MM2_OUTPUT}")
endif()
