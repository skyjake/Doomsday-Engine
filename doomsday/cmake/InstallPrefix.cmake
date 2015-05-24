set (_oldPrefix ${CMAKE_INSTALL_PREFIX})

# Install destination. PREFIX can be used to set the location manually.
# By default we'll use distrib/products as the prefix.
if (DEFINED PREFIX OR NOT DEFINED DENG_PREFIX_SET)
    if (DEFINED PREFIX)
        get_filename_component (PREFIX "${PREFIX}" ABSOLUTE)
        set (CMAKE_INSTALL_PREFIX "${PREFIX}" CACHE "Install prefix" PATH FORCE)
    else ()
        #message ("The default install prefix can be overridden with PREFIX.")
        get_filename_component (installPrefix "${DENG_DISTRIB_DIR}" REALPATH)
        set (CMAKE_INSTALL_PREFIX "${installPrefix}" CACHE "Install prefix" PATH FORCE)
    endif ()
    if (NOT _oldPrefix STREQUAL CMAKE_INSTALL_PREFIX)
	message (STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
    endif ()
    set (DENG_PREFIX_SET YES CACHE STRING "Install prefix applied from the PREFIX variable")
    mark_as_advanced (DENG_PREFIX_SET)
endif ()
