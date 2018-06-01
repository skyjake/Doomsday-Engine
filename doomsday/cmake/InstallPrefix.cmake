if (IOS)
    # On iOS, we'll install files into the app bundle instead of
    # any user-specified location.
    set (CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/apps/client/\${BUILD_TYPE}\${EFFECTIVE_PLATFORM_NAME})
    return ()
endif ()

set (_oldPrefix ${CMAKE_INSTALL_PREFIX})

# Install destination. PREFIX can be used to set the location manually.
# By default we'll use products/ as the prefix.
if (DEFINED DE_PREFIX OR NOT DEFINED DENG_PREFIX_SET)
    if (DEFINED DE_PREFIX)
        get_filename_component (DE_PREFIX "${DE_PREFIX}" ABSOLUTE)
        set (CMAKE_INSTALL_PREFIX "${DE_PREFIX}" CACHE STRING "Install prefix" FORCE)
    else ()
        #message ("The default install prefix can be overridden with PREFIX.")
        get_filename_component (installPrefix "${DE_DISTRIB_DIR}" REALPATH)
        set (CMAKE_INSTALL_PREFIX "${installPrefix}" CACHE STRING "Install prefix" FORCE)
    endif ()
    if (NOT _oldPrefix STREQUAL CMAKE_INSTALL_PREFIX)
	message (STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
    endif ()
    set (DE_PREFIX_SET YES CACHE STRING "Install prefix applied from the DE_PREFIX variable")
    mark_as_advanced (DE_PREFIX_SET)
endif ()
