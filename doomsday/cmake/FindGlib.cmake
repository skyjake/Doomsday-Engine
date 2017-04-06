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
else ()
    # Find GLib via pkg-config.
    find_package (PkgConfig)

    add_pkgconfig_interface_library (glib glib-2.0 gthread-2.0)

    get_property (GLIB_LIBRARIES TARGET glib
        PROPERTY INTERFACE_LINK_LIBRARIES
    )
endif ()