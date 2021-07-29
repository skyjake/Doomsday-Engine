# The Doomsday Engine Project: Server Shell -- GUI Interface
# Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

cmake_minimum_required (VERSION 3.1)
project (DE_SHELL)
include (../../cmake/Config.cmake)

find_package (the_Foundation REQUIRED)
find_package (glbinding REQUIRED)

add_definitions (
    -DSHELL_VERSION="${DE_VERSION}"
)

file (GLOB SOURCES src/*.cpp src/*.h)
set (MAC_RES
    res/macx/shell.icns,Resources
    res/macx/English.lproj,Resources
)
set (MACOSX_BUNDLE_ICON_FILE "shell.icns")
if (WIN32 OR MSYS)
    list (APPEND SOURCES res/windows/shell.rc)
endif ()

list (APPEND DE_REQUIRED_PACKAGES
    net.dengine.stdlib
    net.dengine.stdlib.gui
)
deng_add_package (net.dengine.shell)

deng_add_application (shell ${SOURCES} EXTRA_RESOURCES ${MAC_RES})

if (APPLE OR WIN32)
    set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "Doomsday Shell")
    set_target_properties (shell PROPERTIES
        OUTPUT_NAME "Doomsday Shell"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER net.dengine.shell
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_LIST_DIR}/res/macx/Info.plist
    )
else ()
    set_property (TARGET shell PROPERTY OUTPUT_NAME "doomsday-shell")
endif ()

target_link_sdl2 (shell PUBLIC)
deng_link_libraries (shell PUBLIC DengCore DengDoomsday DengGui)
target_link_libraries (shell PUBLIC doomsdaygui)

# Deployment.
deng_install_bundle_deps (shell
    Deng::libcore
    Deng::libdoomsday
    Deng::libgui
    the_Foundation::the_Foundation
    glbinding::glbinding
)
deng_deploy_target (shell)

if (UNIX_LINUX)
    configure_file (doomsday-shell.desktop.in net.dengine.Shell.desktop)
    install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/net.dengine.Shell.desktop
        DESTINATION share/applications)
    install (FILES res/linux/deng-shell-logo-256.png
        DESTINATION ${DE_INSTALL_DATA_DIR})
endif ()

if (WIN32)
    # WIX generator requires setting this manually.
    set_property (INSTALL "bin/$<TARGET_FILE_NAME:shell>"
        PROPERTY CPACK_START_MENU_SHORTCUTS "Server Shell"
    )
endif ()
