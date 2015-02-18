find_package (PkgConfig)

add_pkgconfig_interface_library (SDL2 OPTIONAL sdl2)
add_pkgconfig_interface_library (SDL2_mixer OPTIONAL SDL2_mixer)

if (NOT TARGET SDL2)
    add_definitions (-DDENG_NO_SDL)
endif ()
if (NOT TARGET SDL2_mixer)
    add_definitions (-DDENG_DISABLE_SDLMIXER)
endif ()
