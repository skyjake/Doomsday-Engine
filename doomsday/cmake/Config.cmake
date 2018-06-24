# Doomsday Engine -- General configuration
#
# All CMakeLists should include this file to gain access to the overall
# project configuration.

if (POLICY CMP0068)
    cmake_policy (SET CMP0068 OLD)  # macOS: RPATH affects install_name
endif ()
if (POLICY CMP0058)
    cmake_policy (SET CMP0058 OLD)  # Resource file generation / dependencies.
endif ()

get_filename_component (_where "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
message (STATUS "Configuring ${_where}...")

include (${CMAKE_CURRENT_LIST_DIR}/Directories.cmake)
include (Macros)
include (Arch)
include (BuildTypes)
include (InstallPrefix)
include (Version)
find_package (Ccache)
include (Options)
include (Packaging)

set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "client")

# Prefix path is used for finding CMake config packages.
if (NOT DE_SDK_DIR STREQUAL "")
    list (APPEND CMAKE_PREFIX_PATH "${DE_SDK_DIR}/${DE_INSTALL_LIB_DIR}")
endif ()

find_package (CPlus REQUIRED)

# Qt Configuration -----------------------------------------------------------

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_AUTOMOC OFF)
set (CMAKE_AUTORCC OFF)
set_property (GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER Generated)

find_package (Qt)

# Find Qt5 in all projects to ensure automoc is run.
list (APPEND CMAKE_PREFIX_PATH "${QT_PREFIX_DIR}")

# This will ensure automoc works in all projects.
if (QT_MODULE STREQUAL Qt5)
    find_package (Qt5 COMPONENTS Core Network)
else ()
    find_package (Qt4 REQUIRED)
endif ()

# Check for mobile platforms.
if (NOT QMAKE_XSPEC_CHECKED)
    qmake_query (xspec "QMAKE_XSPEC")
    set (QMAKE_XSPEC ${xspec} CACHE STRING "Value of QMAKE_XSPEC")
    set (QMAKE_XSPEC_CHECKED YES CACHE BOOL "QMAKE_XSPEC has been checked")
    mark_as_advanced (QMAKE_XSPEC)
    mark_as_advanced (QMAKE_XSPEC_CHECKED)
    if (QMAKE_XSPEC STREQUAL "macx-ios-clang")
        set (IOS YES CACHE BOOL "Building for iOS platform")
    endif ()
endif ()

# Platform-Specific Configuration --------------------------------------------

if (IOS)
    include (PlatformiOS)
elseif (APPLE)
    include (PlatformMacx)
elseif (WIN32)
    include (PlatformWindows)
else ()
    include (PlatformUnix)
endif ()

# Helpers --------------------------------------------------------------------

set (Python_ADDITIONAL_VERSIONS 2.7)
find_package (PythonInterp REQUIRED)

if (DE_ENABLE_COTIRE)
    include (cotire)
endif ()
include (LegacyPK3s)
