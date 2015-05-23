# Amethyst (c) 2002-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required (VERSION 3.1)
project (Amethyst)

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_AUTOMOC ON)

# In a standalone source package, the cmake modules are in a subdirectory.
# Otherwise, use Doomsday's CMake modules.
set (CMAKE_MODULE_PATH 
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake
)

include (Macros)
include (FindQt)
list (APPEND CMAKE_PREFIX_PATH "${QT_PREFIX_DIR}")

file (GLOB SOURCES src/*.cpp src/*.h)
add_executable (amethyst ${SOURCES})
set_property (TARGET amethyst PROPERTY FOLDER Tools)
strict_warnings (amethyst)

find_package (Qt5Core QUIET)
if (Qt5Core_FOUND)
    add_definitions (-DHAVE_QT5)
    target_link_libraries (amethyst Qt5::Core)
else ()
    find_package (Qt4 REQUIRED)
    target_link_libraries (amethyst Qt4::Core)
endif ()

if (UNIX)
    set (LIB_PREFIX "share/amethyst")
    set (DOC_PREFIX "share/doc/amethyst")
    add_definitions (-DAME_LIBDIR="${CMAKE_INSTALL_PREFIX}/${LIB_PREFIX}/lib")
else ()
    set (LIB_PREFIX ".")
    set (DOC_PREFIX "doc")
    add_definitions (-DAME_LIBDIR="${CMAKE_INSTALL_PREFIX}/lib")
endif ()

install (TARGETS amethyst DESTINATION bin)
install (DIRECTORY lib DESTINATION ${LIB_PREFIX})
install (FILES changelog.txt DESTINATION ${DOC_PREFIX})

if (WIN32)
    deng_install_deployqt (amethyst)
endif ()

# install (FILES
#     ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Macros.cmake
#     ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FindQt.cmake
# )
