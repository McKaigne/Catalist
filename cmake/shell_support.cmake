# Writes what the built program says about itself: the manual page and the
# completion scripts for zsh and fish. Run by the build:
#
#   cmake -DPROGRAM=build/catalist -DVERSION=0.3.0 -DTEMPLATE=docs/catalist.1.in
#         -DOUT_DIR=build/share -P cmake/shell_support.cmake
#
# The program writes them from its own table of commands, so the page and
# the scripts always know the commands it has.

foreach(required PROGRAM VERSION TEMPLATE OUT_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "shell_support.cmake: ${required} is not set")
  endif()
endforeach()

# None of these read the lists; an empty directory makes sure of it.
set(ENV{CATALIST_DATA_DIR} "${OUT_DIR}/no-data")
file(MAKE_DIRECTORY "${OUT_DIR}/man/man1" "${OUT_DIR}/zsh" "${OUT_DIR}/fish")

function(catalist_write output)
  execute_process(
    COMMAND "${PROGRAM}" ${ARGN}
    OUTPUT_FILE "${output}"
    RESULT_VARIABLE failed
  )
  if(failed)
    message(FATAL_ERROR "${PROGRAM} ${ARGN} failed: ${failed}")
  endif()
endfunction()

# The manual page: the template with the version and the commands put in.
execute_process(
  COMMAND "${PROGRAM}" __man
  OUTPUT_VARIABLE commands
  RESULT_VARIABLE failed
)
if(failed)
  message(FATAL_ERROR "${PROGRAM} __man failed: ${failed}")
endif()
file(READ "${TEMPLATE}" page)
string(REPLACE "@PROJECT_VERSION@" "${VERSION}" page "${page}")
string(REPLACE "@COMMANDS@\n" "${commands}" page "${page}")
file(WRITE "${OUT_DIR}/man/man1/catalist.1" "${page}")
catalist_write("${OUT_DIR}/zsh/_catalist" completion zsh)
catalist_write("${OUT_DIR}/fish/catalist.fish" completion fish)
