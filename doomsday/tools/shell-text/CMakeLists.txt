# The Doomsday Engine Project: Server Shell -- Text-Mode Interface
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

cmake_minimum_required (VERSION 3.1)
project (DENG_SHELLTEXT)
include (../../cmake/Config.cmake)

set (CURSES_NEED_NCURSES TRUE)

# Dependencies.
find_package (DengCore)
find_package (DengShell)
find_package (Curses REQUIRED)
mark_as_advanced (CURSES_FORM_LIBRARY)
find_package (Amethyst QUIET)

include_directories (${CURSES_INCLUDE_DIRS})
add_definitions (
    -DSHELL_VERSION="${DENG_VERSION}"
)

file (GLOB SOURCES src/*.cpp src/*.h)

if (AMETHYST_FOUND)
    if (UNIX)
        set (MAN_PAGE ${CMAKE_CURRENT_BINARY_DIR}/doomsday-shell-text.6)
        list (APPEND SOURCES ${MAN_PAGE})
        deng_add_amedoc (MAN ${MAN_PAGE} ${DENG_SOURCE_DIR}/doc/shell-text shell-text.ame)
    endif ()
endif ()

add_executable (shell-text ${SOURCES})
set_target_properties (shell-text PROPERTIES
    OUTPUT_NAME doomsday-shell-text
    FOLDER Tools
)
target_link_libraries (shell-text Deng::libcore Deng::libshell ${CURSES_LIBRARIES})
deng_target_defaults (shell-text)

deng_install_tool (shell-text)
