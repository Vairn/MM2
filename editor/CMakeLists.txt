cmake_minimum_required(VERSION 3.16)
project(MM2ED LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)

# ---------------------------------------------------------------------------
# Dependencies (fetched at configure time into build/_deps)
# ---------------------------------------------------------------------------
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        docking
)

FetchContent_Declare(
  portable_file_dialogs
  GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
  GIT_TAG        main
)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
  SOURCE_SUBDIR  not-a-real-subdir
)

# imnodes (node graph UI for the event editor). Zoom support from Auburn PR #192
# (https://github.com/Nelarius/imnodes/pull/192) — not yet merged upstream.
FetchContent_Declare(
  imnodes
  GIT_REPOSITORY https://github.com/Auburn/imnodes.git
  GIT_TAG        f802c80357f3fcd19b741da8ba7f82fd4349da39
  SOURCE_SUBDIR  not-a-real-subdir
)

FetchContent_MakeAvailable(glfw imgui portable_file_dialogs imnodes stb)

# Auburn zoom fork still references removed ImGuiIO::SetPlatformImeDataFn (imgui 1.91+).
set(_imnodes_cpp "${imnodes_SOURCE_DIR}/imnodes.cpp")
if(EXISTS "${_imnodes_cpp}")
  file(READ "${_imnodes_cpp}" _imnodes_src)
  string(REPLACE "SetPlatformImeDataFn" "BackendLanguageUserData" _imnodes_patched "${_imnodes_src}")
  if(NOT _imnodes_src STREQUAL _imnodes_patched)
    file(WRITE "${_imnodes_cpp}" "${_imnodes_patched}")
  endif()
  unset(_imnodes_src)
  unset(_imnodes_patched)
endif()
unset(_imnodes_cpp)

find_package(OpenGL REQUIRED)

# ---------------------------------------------------------------------------
# ImGui (compiled directly into the editor; docking branch)
# ---------------------------------------------------------------------------
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_DOCKING)

# ---------------------------------------------------------------------------
# imnodes (compiled directly; depends on imgui)
# ---------------------------------------------------------------------------
add_library(imnodes STATIC
  ${imnodes_SOURCE_DIR}/imnodes.cpp
)
target_include_directories(imnodes PUBLIC ${imnodes_SOURCE_DIR})
target_link_libraries(imnodes PUBLIC imgui)

# ---------------------------------------------------------------------------
# Editor sources
# ---------------------------------------------------------------------------
file(GLOB_RECURSE MM2ED_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_executable(MM2ED ${MM2ED_SRC})

target_include_directories(MM2ED PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${portable_file_dialogs_SOURCE_DIR}
  ${stb_SOURCE_DIR}
)

target_link_libraries(MM2ED PRIVATE imgui imnodes glfw OpenGL::GL)

if(MSVC)
  target_compile_options(MM2ED PRIVATE /W3)
  set_target_properties(MM2ED PROPERTIES VS_DPI_AWARE "PerMonitorV2")
else()
  target_compile_options(MM2ED PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(WIN32 AND MINGW)
  enable_language(RC)
  target_sources(MM2ED PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/MM2ED.rc")
endif()
