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
set_path (DENG_DISTRIB_DIR "${DENG_SOURCE_DIR}/../distrib/products")
set (DENG_EXTERNAL_SOURCE_DIR "${DENG_SOURCE_DIR}/external")
set (DENG_API_DIR "${DENG_SOURCE_DIR}/apps/api")
set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
set (CMAKE_PREFIX_PATH "${DENG_DISTRIB_DIR}")

include (Macros)
include (Arch)
include (BuildTypes)
include (InstallPrefix)
include (Version)
find_package (Ccache)
include (Options)
include (Packaging)

# Install directories.
set (DENG_INSTALL_DATA_DIR "share/doomsday")
set (DENG_INSTALL_DOC_DIR "share/doc")
set (DENG_INSTALL_MAN_DIR "share/man/man6")
set (DENG_INSTALL_LIB_DIR "lib")
if (ARCH_BITS EQUAL 64)
    if (EXISTS ${CMAKE_INSTALL_PREFIX}/lib64)
        set (DENG_INSTALL_LIB_DIR "lib64")
    elseif (EXISTS ${CMAKE_INSTALL_PREFIX}/lib/x86_64-linux-gnu)
        set (DENG_INSTALL_LIB_DIR "lib/x86_64-linux-gnu")
    endif ()
endif ()    
set (DENG_INSTALL_PLUGIN_DIR "${DENG_INSTALL_LIB_DIR}/doomsday")

set (DENG_BUILD_STAGING_DIR "${CMAKE_BINARY_DIR}/bundle-staging")

set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "client")

# Qt Configuration -----------------------------------------------------------

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set_property (GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER Generated)

find_package (Qt)

# Find Qt5 in all projects to ensure automoc is run.
list (APPEND CMAKE_PREFIX_PATH "${QT_PREFIX_DIR}")

# This will ensure automoc works in all projects.
find_package (Qt5 COMPONENTS Core Network)

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
