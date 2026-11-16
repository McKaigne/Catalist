# Writes what the built program says about itself: the manual page and the
# completion scripts for zsh, fish and bash. Run by the build:
#
#   cmake -DPROGRAM=build/catalist -DTEMPLATE=docs/catalist.1.in
#         -DOUT_DIR=build/share -P cmake/shellSupport.cmake
#
# The program writes them from its own table of commands, so the page and
# the scripts always know the commands it has.

foreach(required PROGRAM TEMPLATE OUT_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "shellSupport.cmake: ${required} is not set")
  endif()
endforeach()

# None of these read the lists; an empty directory makes sure of it.
set(ENV{CATALIST_DATA_DIR} "${OUT_DIR}/no-data")
file(MAKE_DIRECTORY "${OUT_DIR}/man/man1" "${OUT_DIR}/zsh" "${OUT_DIR}/fish" "${OUT_DIR}/bash")

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

execute_process(
  COMMAND "${PROGRAM}" __man
  INPUT_FILE "${TEMPLATE}"
  OUTPUT_FILE "${OUT_DIR}/man/man1/catalist.1"
  RESULT_VARIABLE failed
)
if(failed)
  message(FATAL_ERROR "${PROGRAM} __man failed: ${failed}")
endif()
catalist_write("${OUT_DIR}/zsh/_catalist" completion zsh)
catalist_write("${OUT_DIR}/fish/catalist.fish" completion fish)
# `cl` is catalist under another name: fish completes it the same way.
file(WRITE "${OUT_DIR}/fish/cl.fish" "complete -c cl -w catalist\n")
# bash-completion loads a program's file when that program is first
# completed, so `cl` needs a file of its own; the script serves both.
catalist_write("${OUT_DIR}/bash/catalist" completion bash)
file(READ "${OUT_DIR}/bash/catalist" bashScript)
file(WRITE "${OUT_DIR}/bash/cl" "${bashScript}")
