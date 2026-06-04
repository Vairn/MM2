# MM2 wrapper: Bartman toolchain + auto-detect VS Code "Amiga Debug" extension paths.
include(${CMAKE_CURRENT_LIST_DIR}/AmigaCMakeCrossToolchains/m68k-bartman.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/AmigaVscodeBin.cmake)

set(TOOLCHAIN_PATH "${AMIGA_TOOLCHAIN_OPT}" CACHE PATH "Bartman opt prefix" FORCE)
set(CMAKE_C_COMPILER "${AMIGA_GCC}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${AMIGA_GCC}" CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER "${AMIGA_GCC}" CACHE FILEPATH "" FORCE)

set(AMIGA_SYSINCLUDE "${AMIGA_TOOLCHAIN_OPT}/m68k-amiga-elf/sys-include")
file(TO_CMAKE_PATH "${AMIGA_SYSINCLUDE}" AMIGA_SYSINCLUDE)
string(APPEND CMAKE_C_FLAGS_INIT " -isystem=\"${AMIGA_SYSINCLUDE}\"")
string(APPEND CMAKE_CXX_FLAGS_INIT " -isystem=\"${AMIGA_SYSINCLUDE}\"")
