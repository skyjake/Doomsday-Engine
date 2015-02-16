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

set (DENG_INSTALL_DOC_DIR "share/doc")
set (DENG_INSTALL_LIB_DIR "lib/doomsday")
if (ARCH_BITS EQUAL 64)
    if (EXISTS ${CMAKE_INSTALL_PREFIX}/lib64)
        set (DENG_INSTALL_LIB_DIR "lib64/doomsday")
    elseif (EXISTS ${CMAKE_INSTALL_PREFIX}/lib/x86_64-linux-gnu)
        set (DENG_INSTALL_LIB_DIR "lib/x86_64-linux-gnu")
    endif ()
endif ()    
set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "runtime")

# Qt Configuration -----------------------------------------------------------

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)

find_package (Qt)

# Find Qt5 in all projects to ensure automoc is run.
list (APPEND CMAKE_PREFIX_PATH "${QT_PREFIX_DIR}")

# Platform-Specific Configuration --------------------------------------------

if (APPLE)
    include (PlatformMacx)
elseif (WIN32)
    include (PlatformWindows)    
else ()
    include (PlatformUnix)
endif ()

# Helpers --------------------------------------------------------------------

include (cotire)

macro (deng_target_defaults target)
    set_target_properties (${target} PROPERTIES 
        VERSION       ${DENG_VERSION}
        SOVERSION     ${DENG_COMPAT_VERSION}
    )
    if (APPLE)
        set_property (TARGET ${target} PROPERTY INSTALL_RPATH "@loader_path/../Frameworks")
    elseif (UNIX)
        set_property (TARGET ${target} 
            PROPERTY INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_LIB_DIR}
        )
    endif ()        
    enable_cxx11 (${target})
    strict_warnings (${target})    
    #cotire (${target})
endmacro (deng_target_defaults)

macro (deng_library target)
    # Form the list of source files.
    set (_src ${ARGV})    
    list (REMOVE_AT _src 0) # remove target
    deng_filter_platform_sources (_src ${_src})
    # Define the target and namespace alias.
    add_library (${target} SHARED ${_src})
    add_library (Deng::${target} ALIAS ${target})
    # Libraries use the "deng_" prefix.
    string (REGEX REPLACE "lib(.*)" "deng_\\1" _outName ${target})
    set_property (TARGET ${target} PROPERTY OUTPUT_NAME ${_outName})
    set (_outName)
    # Compiler settings.
    deng_target_defaults (${target})
    target_include_directories (${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
        $<INSTALL_INTERFACE:include/>
    )    
    #cotire (${target})
endmacro (deng_library)

macro (deng_deploy_library target name)
    install (TARGETS ${target}
        EXPORT ${name} 
        LIBRARY DESTINATION ${DENG_INSTALL_LIB_DIR}
        INCLUDES DESTINATION include)
    install (EXPORT ${name} DESTINATION lib/cmake/${name} NAMESPACE Deng::)
    install (FILES ${name}Config.cmake DESTINATION lib/cmake/${name})
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include/de)
        install (DIRECTORY include/de DESTINATION include)
    endif ()
endmacro (deng_deploy_library)
