# The Doomsday Engine Project: Server Shell -- Text-Mode Interface
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

cmake_minimum_required (VERSION 3.0)
project (DENG_SHELLTEXT)
include (../../cmake/Config.cmake)

set (CURSES_NEED_NCURSES TRUE)

# Dependencies.
find_package (DengCore QUIET)
find_package (DengShell QUIET)
find_package (Curses REQUIRED)

include_directories (${CURSES_INCLUDE_DIRS})
add_definitions (
    -DSHELL_VERSION="${DENG_VERSION}"
)

file (GLOB SOURCES src/*.cpp src/*.h)

set (_tgt doomsday-shell-text)
add_executable (${_tgt} ${SOURCES})
target_link_libraries (${_tgt} Deng::deng_core Deng::deng_shell ${CURSES_LIBRARIES})
deng_target_defaults (${_tgt})

install (TARGETS ${_tgt} DESTINATION bin)

# unix {
#     INSTALLS += readme
#     readme.files = ../../../doc/output/doomsday-shell-text.6
#     readme.path  = $$PREFIX/share/man/man6
# }
