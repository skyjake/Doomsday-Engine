# Doomsday Engine -- General configuration
#
# All CMakeLists should include this file to gain access to the overall
# project configuration.

get_filename_component (_where "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
message (STATUS "Configuring ${_where}...")

macro (set_path var path)
    get_filename_component (${var} "${path}" REALPATH)
endmacro (set_path)

# Project Options & Paths ----------------------------------------------------

set_path (DENG_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set (DENG_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")
if (APPLE OR WIN32)
    set_path (DENG_DISTRIB_DIR "${DENG_SOURCE_DIR}/../distrib/products")
else ()
    set_path (DENG_DISTRIB_DIR /usr)
endif ()
set (DENG_EXTERNAL_SOURCE_DIR "${DENG_SOURCE_DIR}/external")
set (DENG_API_DIR "${DENG_SOURCE_DIR}/apps/api")
set (CMAKE_MODULE_PATH "${DENG_CMAKE_DIR}")
set (DENG_SDK_DIR "" CACHE PATH "Location of the Doomsday SDK to use for compiling")

include (Macros)
include (Arch)
include (BuildTypes)
include (InstallPrefix)
include (Version)
find_package (Ccache)
include (Options)
include (Packaging)

if (UNIX AND NOT APPLE)
    include (GNUInstallDirs)
endif()

# Install directories.
set (DENG_INSTALL_DATA_DIR "share/doomsday")
set (DENG_INSTALL_DOC_DIR "share/doc")
set (DENG_INSTALL_MAN_DIR "share/man/man6")
if (DEFINED CMAKE_INSTALL_LIBDIR)
    set (DENG_INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR})
else ()
    set (DENG_INSTALL_LIB_DIR "lib")
endif () 
set (DENG_INSTALL_PLUGIN_DIR "${DENG_INSTALL_LIB_DIR}/doomsday")

set (DENG_BUILD_STAGING_DIR "${CMAKE_BINARY_DIR}/bundle-staging")

set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "client")

# Prefix path is used for finding CMake config packages.
if (NOT DENG_SDK_DIR STREQUAL "")
    set (CMAKE_PREFIX_PATH "${DENG_SDK_DIR}/${DENG_INSTALL_LIB_DIR}")
else ()
    set (CMAKE_PREFIX_PATH "${DENG_CMAKE_DIR}/config")
endif ()

# Qt Configuration -----------------------------------------------------------

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
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

# Platform-Specific Configuration --------------------------------------------

if (APPLE)
    include (PlatformMacx)
elseif (WIN32)
    include (PlatformWindows)    
else ()
    include (PlatformUnix)
endif ()

# Helpers --------------------------------------------------------------------

set (Python_ADDITIONAL_VERSIONS 2.7)
find_package (PythonInterp REQUIRED)

if (DENG_ENABLE_COTIRE)
    include (cotire)
endif ()
include (LegacyPK3s)
