if (APPLE AND GLIB_STATIC_DIR AND GETTEXT_STATIC_DIR)
    # Use the static versions of GLib and its dependencies.
    add_library (glib INTERFACE)
    target_link_libraries (glib INTERFACE
        ${GLIB_STATIC_DIR}/lib/libglib-2.0.a
        ${GLIB_STATIC_DIR}/lib/libgthread-2.0.a
        ${GETTEXT_STATIC_DIR}/lib/libintl.a
        iconv
    )
    target_include_directories (glib INTERFACE
        ${GLIB_STATIC_DIR}/include/glib-2.0
        ${GLIB_STATIC_DIR}/lib/glib-2.0/include
        ${GETTEXT_STATIC_DIR}/include
    )
    link_framework (glib INTERFACE CoreFoundation)
    link_framework (glib INTERFACE CoreServices)
elseif (MSYS2_LIBS_DIR)
    add_library (glib INTERFACE)
    set (_glibDir ${MSYS2_LIBS_DIR}/${DE_ARCH}/glib-2.0)
    target_link_libraries (glib INTERFACE
        ${_glibDir}/lib/libglib-2.0-0.lib
        ${_glibDir}/lib/libgthread-2.0-0.lib
        ws2_32.lib
    )
    target_include_directories (glib INTERFACE
        ${_glibDir}/include
        ${_glibDir}/lib/glib-2.0/include
    )
    file (GLOB _bins ${_glibDir}/lib/*.dll)
    foreach (_bin ${_bins})
        deng_install_library (${_bin})
    endforeach (_bin)
else ()
    # Find GLib via pkg-config.
    find_package (PkgConfig)

    add_pkgconfig_interface_library (glib glib-2.0 gthread-2.0)

    get_property (GLIB_LIBRARIES TARGET glib
        PROPERTY INTERFACE_LINK_LIBRARIES
    )
endif ()
