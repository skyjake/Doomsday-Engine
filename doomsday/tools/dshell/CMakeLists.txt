# The Doomsday Engine Project: Server Shell -- Text-Mode Interface
# Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

cmake_minimum_required (VERSION 3.1)
project (DE_DSHELL)
include (../../cmake/Config.cmake)

set (CURSES_NEED_NCURSES TRUE)

# Dependencies.
find_package (Curses REQUIRED)
mark_as_advanced (CURSES_FORM_LIBRARY)
find_package (Amethyst QUIET)

include_directories (${CURSES_INCLUDE_DIRS})
add_definitions (
    -DSHELL_VERSION="${DE_VERSION}"
)

file (GLOB SOURCES src/*.cpp src/*.h)

if (AMETHYST_FOUND)
    if (UNIX)
        set (MAN_PAGE ${CMAKE_CURRENT_BINARY_DIR}/doomsday-dshell.6)
        list (APPEND SOURCES ${MAN_PAGE})
        deng_add_amedoc (MAN ${MAN_PAGE} ${DE_SOURCE_DIR}/doc/dshell dshell.ame)
    endif ()
endif ()

add_executable (dshell ${SOURCES})
deng_target_defaults (dshell)
set_target_properties (dshell PROPERTIES
    OUTPUT_NAME dshell
    FOLDER Tools
)
deng_link_libraries (dshell PRIVATE DengCore DengDoomsday)
target_link_libraries (dshell PRIVATE ${CURSES_LIBRARIES})

deng_install_tool (dshell)

if (CYGWIN OR MSYS)
    # Curses requires terminfo to be available.
    install (FILES /usr/share/terminfo/63/cygwin DESTINATION share/terminfo/63)
    install (FILES /usr/share/terminfo/78/xterm-256color DESTINATION share/terminfo/78)
endif ()
