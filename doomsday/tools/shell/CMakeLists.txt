# The Doomsday Engine Project: Server Shell -- GUI Interface
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

cmake_minimum_required (VERSION 3.1)
project (DENG_SHELL)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)
find_package (DengShell)

add_definitions (
    -DSHELL_VERSION="${DENG_VERSION}"
)

file (GLOB SOURCES src/*.cpp src/*.h res/shell.qrc)
set (MAC_RES 
    res/macx/shell.icns,Resources
    res/macx/English.lproj,Resources
)
set (MACOSX_BUNDLE_ICON_FILE "shell.icns")
if (WIN32)
    list (APPEND SOURCES res/windows/shell.rc)
endif ()

deng_add_application (shell ${SOURCES} EXTRA_RESOURCES ${MAC_RES})

if (APPLE OR WIN32)
    set_property (TARGET shell PROPERTY OUTPUT_NAME "Doomsday Shell")
    set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "Doomsday Shell")
else ()
    set_property (TARGET shell PROPERTY OUTPUT_NAME "doomsday-shell")
endif ()

target_link_libraries (shell PUBLIC Deng::libcore Deng::libshell)
deng_target_link_qt (shell PUBLIC Widgets)

# Deployment.
deng_install_bundle_deps (shell Deng::libcore Deng::libshell)
deng_install_deployqt (shell)
deng_codesign (shell)
