find_package (PkgConfig)

add_pkgconfig_interface_library (glib glib-2.0 gthread-2.0)

get_property (GLIB_LIBRARIES TARGET glib 
    PROPERTY INTERFACE_LINK_LIBRARIES
)
