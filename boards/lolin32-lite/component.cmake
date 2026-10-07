# How ESP-IDF registers a program's sources.
#
# The generated build lists the sources in `RHO_PROGRAM_SOURCES` and their
# include directories in `RHO_PROGRAM_INCLUDE_DIRS`, and includes this file
# from the program's own directory. Registering them is ESP-IDF's call, so the
# name is here, in the platform, rather than in the generator.
#
# The platform library is linked after registering, which is what a component
# does with the components and libraries it needs.
#
# @project RoboHub Platform
# @license MIT

# Only directories that are there. ESP-IDF refuses a build whose include list
# names a directory that does not exist, and a generated list can carry one: a
# dependency names the directory its own configuration is generated into, which
# is a path in the *build* tree and not one a reader from here can see. A
# compiler ignores an include directory that is not there; this framework does
# not, so the list is filtered once, here.
set(RHO_EXISTING_INCLUDE_DIRS "")
foreach(dir ${RHO_PROGRAM_INCLUDE_DIRS})
    if(IS_DIRECTORY "${dir}")
        list(APPEND RHO_EXISTING_INCLUDE_DIRS "${dir}")
    endif()
endforeach()

idf_component_register(
    SRCS ${RHO_PROGRAM_SOURCES} ${CMAKE_CURRENT_LIST_DIR}/app_main.c
    INCLUDE_DIRS ${RHO_EXISTING_INCLUDE_DIRS}
)

# Grouped, because the platform's library and the common one refer to each
# other and to the program: a linker reads a library once, in order, and a
# reference to a symbol defined later in the list stays unresolved. A group
# re-reads them until nothing new is found, emitted as
# `--start-group`/`--end-group` around the two archives on the final link. It
# is written here, at the program, because this is where both libraries are
# known and where the application is linked.
#
# `--undefined=robohub_board_register` makes the board's registration an
# unresolved symbol before either archive is read. Without it the weak default
# in the common library would satisfy the reference first, the linker would
# never reach into the platform archive for the strong definition, and the
# program would link and run against the no-op default — the board would
# register nothing, and say nothing about it.
target_link_libraries(${COMPONENT_LIB} PRIVATE
    -Wl,--start-group
    -Wl,--undefined=robohub_board_register
    robohub-esp32
    robohub-common
    -Wl,--end-group
)
