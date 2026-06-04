# POST_BUILD: copy MM2 retail data beside the Amiga binary (cwd = build dir under UAE).
if(NOT MM2_ROOT OR NOT DEST)
  message(FATAL_ERROR "copy_mm2_data.cmake requires MM2_ROOT and DEST")
endif()

string(REGEX REPLACE "^\"|\"$" "" MM2_ROOT "${MM2_ROOT}")
string(REGEX REPLACE "^\"|\"$" "" DEST "${DEST}")
get_filename_component(MM2_ROOT "${MM2_ROOT}" ABSOLUTE)
get_filename_component(DEST "${DEST}" ABSOLUTE)

file(GLOB _dat "${MM2_ROOT}/*.dat")
file(GLOB _32 "${MM2_ROOT}/*.32")
file(GLOB _anm "${MM2_ROOT}/*.anm")
set(_all ${_dat} ${_32} ${_anm})
list(LENGTH _all _n)
if(_n EQUAL 0)
  message(WARNING "[MM2] No .dat/.32/.anm under ${MM2_ROOT} — place game data at repo root")
endif()

foreach(_src ${_all})
  get_filename_component(_name "${_src}" NAME)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_src}" "${DEST}/${_name}"
    RESULT_VARIABLE _r
  )
  if(_r)
    message(WARNING "[MM2] Failed to copy ${_name} -> ${DEST}")
  endif()
endforeach()
