find_package (PkgConfig)

add_pkgconfig_interface_library (glib glib-2.0 gthread-2.0)

get_property (GLIB_LIBRARIES TARGET glib 
    PROPERTY INTERFACE_LINK_LIBRARIES
)

#--------
#
# pkg_check_modules (GLIB REQUIRED glib-2.0)
# pkg_check_modules (GTHREAD REQUIRED gthread-2.0)
#
# # Locate full paths of the required shared libraries.
# set (libNames ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})
# list (REMOVE_DUPLICATES libNames)
# foreach (lib ${libNames})
#     find_library (path ${lib} HINTS ${GLIB_LIBRARY_DIRS};${GTHREAD_LIBRARY_DIRS})
#     list (APPEND libs ${path})
#     unset (path CACHE)
# endforeach (lib)
#
# # set (incDirs ${GLIB_INCLUDE_DIRS} ${GTHREAD_INCLUDE_DIRS})
# # list (REMOVE_DUPLICATES incDirs)
#
# # Define an interface library for GLib.
# add_library (glib INTERFACE)
# target_compile_options (glib INTERFACE ${GLIB_CFLAGS} ${GTHREAD_CFLAGS})
# target_link_libraries (glib INTERFACE ${libs})
#
# unset (GLIB_LIBRARIES CACHE)
# foreach (lib ${libs})
#     get_filename_component (p ${lib} REALPATH)
#     list (APPEND GLIB_LIBRARIES ${p})
# endforeach (lib)
# #message ("GLIB_LIBRARIES: ${GLIB_LIBRARIES}")
#
# # unset (incDirs)
# unset (libNames)
# unset (lib)
# unset (libs)
