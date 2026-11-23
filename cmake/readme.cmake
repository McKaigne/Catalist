# Puts the command list the built program writes (`catalist __readme`)
# between README.org's markers, or checks that it is there:
#
#   cmake -DPROGRAM=build/catalist -DREADME=README.org -DMODE=write
#         -P cmake/readme.cmake
#
# MODE=write rewrites the README when its list is not the program's
# (`cmake --build build --target readme`); MODE=check fails instead, which
# is the `readme` test. The build itself never writes into the sources.

foreach(required PROGRAM README MODE)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "readme.cmake: ${required} is not set")
  endif()
endforeach()
if(NOT MODE MATCHES "^(write|check)$")
  message(FATAL_ERROR "readme.cmake: MODE is write or check, not ${MODE}")
endif()

# Org comment lines, so they don't show in the rendered README.
set(begin "# commands: written by `cmake --build build --target readme`; do not edit\n")
set(end "# end of commands")

# None of this reads the lists; a directory that is not there makes sure of it.
set(ENV{CATALIST_DATA_DIR} "${CMAKE_CURRENT_BINARY_DIR}/readme-no-data")
execute_process(
  COMMAND "${PROGRAM}" __readme
  OUTPUT_VARIABLE commands
  RESULT_VARIABLE failed
)
if(failed)
  message(FATAL_ERROR "${PROGRAM} __readme failed: ${failed}")
endif()

file(READ "${README}" old)
string(FIND "${old}" "${begin}" at_begin)
string(FIND "${old}" "${end}" at_end)
if(at_begin EQUAL -1 OR at_end EQUAL -1 OR at_end LESS at_begin)
  message(FATAL_ERROR "${README} has lost its command list's markers:\n${begin}${end}")
endif()
string(LENGTH "${begin}" begin_length)
math(EXPR after_begin "${at_begin} + ${begin_length}")
string(SUBSTRING "${old}" 0 ${after_begin} head)
string(SUBSTRING "${old}" ${at_end} -1 tail)
set(new "${head}\n${commands}\n${tail}")

if(new STREQUAL old)
  return()
endif()
if(MODE STREQUAL "check")
  message(FATAL_ERROR
    "README.org's command list is not the program's: "
    "run cmake --build build --target readme")
endif()
file(WRITE "${README}" "${new}")
message(STATUS "Wrote the command list of ${README}")
