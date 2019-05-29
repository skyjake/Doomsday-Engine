if (IOS)
    # On iOS, we'll install files into the app bundle instead of
    # any user-specified location.
    set (CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/apps/client/\${BUILD_TYPE}\${EFFECTIVE_PLATFORM_NAME})
    return ()
endif ()

set (_oldPrefix ${CMAKE_INSTALL_PREFIX})

# Install destination. DE_PREFIX can be used to set the location manually.
# By default we'll use products/ as the prefix.
if (DEFINED DE_PREFIX OR NOT DEFINED DE_PREFIX_SET)
    if (DEFINED DE_PREFIX)
        get_filename_component (DE_PREFIX "${DE_PREFIX}" ABSOLUTE)
        set (CMAKE_INSTALL_PREFIX "${DE_PREFIX}" CACHE STRING "Install prefix" FORCE)
    else ()
        #message ("The default install prefix can be overridden with DE_PREFIX.")
        get_filename_component (installPrefix "${DE_DISTRIB_DIR}" REALPATH)
        message (FATAL_ERROR "DE_PREFIX is obsolete. Use CMAKE_INSTALL_PREFIX instead.")
    endif ()

# Install destination. DE_PREFIX can be used to set the location manually.
# By default we'll use distrib/products as the prefix.
#if (DEFINED DE_PREFIX OR NOT DEFINED DE_PREFIX_SET)
#    if (DEFINED DE_PREFIX)
#        get_filename_component (PREFIX "${PREFIX}" ABSOLUTE)
#        set (CMAKE_INSTALL_PREFIX "${PREFIX}" CACHE "Install prefix" STRING FORCE)
#    else ()
#        #message ("The default install prefix can be overridden with DE_PREFIX.")

get_filename_component (defaultInstallPrefix "${DE_DISTRIB_DIR}" REALPATH)
set (CMAKE_INSTALL_PREFIX "${defaultInstallPrefix}" CACHE STRING "Install prefix")

#    endif ()
#    if (NOT _oldPrefix STREQUAL CMAKE_INSTALL_PREFIX)
#message (STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
#    endif ()
#    set (DE_PREFIX_SET YES CACHE STRING "Install prefix applied from the DE_PREFIX variable")
#    mark_as_advanced (DE_PREFIX_SET)
#endif ()

# Some CMake targets may be exported to the install directory.
list (APPEND CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
