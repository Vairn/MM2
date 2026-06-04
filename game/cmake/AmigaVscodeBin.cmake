# Resolves Bartman toolchain root from the VS Code / Cursor "Amiga Debug" extension.
# Override: cmake -DAMIGA_VSCODE_BIN=C:/path/to/extension/bin/win32

if(DEFINED AMIGA_VSCODE_BIN AND AMIGA_VSCODE_BIN)
    set(_amiga_bin_root "${AMIGA_VSCODE_BIN}")
else()
    set(_amiga_ext_names bartmanabyss.amiga-debug-1.8.2 bartmanabyss.amiga-debug-1.6.8)
    set(_amiga_search_roots "")
    if(DEFINED ENV{USERPROFILE})
        list(APPEND _amiga_search_roots "$ENV{USERPROFILE}/.vscode/extensions")
        list(APPEND _amiga_search_roots "$ENV{USERPROFILE}/.cursor/extensions")
    endif()
    set(_amiga_bin_root "")
    foreach(_root IN LISTS _amiga_search_roots)
        foreach(_name IN LISTS _amiga_ext_names)
            set(_candidate "${_root}/${_name}/bin/win32")
            if(EXISTS "${_candidate}/opt/bin/m68k-amiga-elf-gcc.exe")
                set(_amiga_bin_root "${_candidate}")
                break()
            endif()
        endforeach()
        if(_amiga_bin_root)
            break()
        endif()
    endforeach()
endif()

if(NOT _amiga_bin_root OR NOT EXISTS "${_amiga_bin_root}/opt/bin/m68k-amiga-elf-gcc.exe")
    message(FATAL_ERROR
        "Amiga Debug extension toolchain not found. Install BartmanAbyss.amiga-debug in VS Code "
        "or pass -DAMIGA_VSCODE_BIN=<path-to-extension>/bin/win32")
endif()

set(AMIGA_VSCODE_BIN "${_amiga_bin_root}" CACHE PATH "Bartman amiga-debug extension bin/win32 root")
set(AMIGA_TOOLCHAIN_OPT "${AMIGA_VSCODE_BIN}/opt")
set(AMIGA_GCC "${AMIGA_TOOLCHAIN_OPT}/bin/m68k-amiga-elf-gcc.exe")
message(STATUS "[MM2] Amiga toolchain: ${AMIGA_VSCODE_BIN}")
