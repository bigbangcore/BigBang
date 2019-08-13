# - Try to find readline include dirs and libraries 
#
# Usage of this module as follows:
#
#     find_package(Readline)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  Readline_ROOT_DIR         Set this variable to the root installation of
#                            readline if the module has problems finding the
#                            proper installation path.
#
# Variables defined by this module:
#
#  READLINE_FOUND            System has readline, include and lib dirs found
#  Readline_INCLUDE_DIR      The readline include directories. 
#  Readline_LIBRARY          The readline library.

find_path(Readline_ROOT_DIR
    NAMES include/readline/readline.h
    PATHS /opt/local/ /usr/local/ /usr/
    NO_DEFAULT_PATH
)

find_path(Readline_INCLUDE_DIR
    NAMES readline/readline.h
    HINTS ${Readline_ROOT_DIR}/include
)

find_library(Readline_LIBRARY
    NAMES readline
    HINTS ${Readline_ROOT_DIR}/lib
)

if(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Ncurses_LIBRARY)
  set(READLINE_FOUND TRUE)
else(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Ncurses_LIBRARY)
  FIND_LIBRARY(Readline_LIBRARY NAMES readline)
  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(Readline DEFAULT_MSG Readline_INCLUDE_DIR Readline_LIBRARY )
  MARK_AS_ADVANCED(Readline_INCLUDE_DIR Readline_LIBRARY)
endif(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Ncurses_LIBRARY)

if (EXISTS "${Readline_INCLUDE_DIR}/readline/readline.h")
  file(STRINGS "${Readline_INCLUDE_DIR}/readline/readline.h" readline_h_content
                REGEX "#define RL_READLINE_VERSION" )
  string(REGEX REPLACE ".*0x([0-9][0-9])([0-9][0-9]).*" "\\1.\\2"
                        READLINE_VERSION ${readline_h_content})
  string(REGEX REPLACE "^0" "" READLINE_VERSION ${READLINE_VERSION})
  string(REPLACE ".0" "." READLINE_VERSION ${READLINE_VERSION})
endif ()

# communicate results
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Readline
    REQUIRED_VARS
        Readline_ROOT_DIR
        Readline_INCLUDE_DIR
        Readline_LIBRARY
    VERSION_VAR
        READLINE_VERSION
)

include_directories(${Readline_INCLUDE_DIR})

mark_as_advanced(
    Readline_ROOT_DIR
    Readline_INCLUDE_DIR
    Readline_LIBRARY
)

message(STATUS "Readline info:")
message(STATUS "  Readline_ROOT_DIR                  : ${Readline_ROOT_DIR}")
message(STATUS "  Readline_INCLUDE_DIR               : ${Readline_INCLUDE_DIR}")
message(STATUS "  Readline_LIBRARY                   : ${Readline_LIBRARY}")
