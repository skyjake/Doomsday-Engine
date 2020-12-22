find_package (PkgConfig QUIET)

macro (target_link_sdl2 target vis)
    if (TARGET SDL2)
        if (MINGW)
            target_link_libraries (${target} ${vis} mingw32) # for SDL_main handling
        endif ()
        target_link_libraries (${target} ${vis} SDL2)
    endif ()
    if (TARGET SDL2_mixer)
        target_link_libraries (${target} ${vis} SDL2_mixer)
    endif ()
endmacro ()

if (TARGET SDL2)
    return ()
endif ()

if (PKG_CONFIG_FOUND AND (NOT MSYS OR CYGWIN OR IOS))
    # The Unix Way: use pkg-config to find the SDL2 libs installed on system.
    if (NOT TARGET SDL2)
        add_pkgconfig_interface_library (SDL2 OPTIONAL sdl2)
        add_pkgconfig_interface_library (SDL2_mixer OPTIONAL SDL2_mixer)
    endif ()

elseif (WIN32 OR CYGWIN OR MSYS)
    deng_clean_path (sdlRoot ${SDL2_DIR})
    # This is Windows, so we'll use the Windows SDL2 libraries that the user has 
    # installed in SDL2_DIR. Note that Cygwin also uses the native SDL2 libraries
    # and *not* the Cygwin ones, which would presumably have an X11 dependency.
    set (_oldPath ${SDL2_LIBRARY})
    if (CYGWIN OR MSYS)
       if (SDL2_DIR STREQUAL "")
           message (FATAL_ERROR "SDL2_DIR must be set in MSYS/Cygwin")
       endif ()
       # Assume it has been set manually.
       set (SDL2_LIBRARY ${sdlRoot}/lib/${DE_ARCH}/SDL2.lib)
    else ()
        file (GLOB _hints ${sdlRoot}/SDL2* $ENV{DENG_DEPEND_PATH}/SDL2*)
        find_library (SDL2_LIBRARY SDL2
            PATHS ${sdlRoot}
            HINTS ${_hints} ENV DENG_DEPEND_PATH
            PATH_SUFFIXES lib/${DE_ARCH} lib
        )
    endif ()
    mark_as_advanced (SDL2_LIBRARY)
    if (NOT SDL2_LIBRARY)
        message (FATAL_ERROR "SDL2 not found. Set the SDL2_DIR variable to help locate it.\n")
    endif ()
    if (NOT _oldPath STREQUAL SDL2_LIBRARY)
        message (STATUS "Found SDL2: ${SDL2_LIBRARY}")
    endif ()

    # Define the target.
    add_library (SDL2 INTERFACE)
    target_link_libraries (SDL2 INTERFACE ${SDL2_LIBRARY})

    # Deduce the include directory.
    get_filename_component (_libDir ${SDL2_LIBRARY} DIRECTORY)
    get_filename_component (_incDir ${_libDir}/../../include REALPATH)

    # message (STATUS "SDL2 include dir: ${_incDir}")
    # message (STATUS "SDL2 library dir: ${_libDir}")

    target_include_directories (SDL2 INTERFACE ${_incDir})
    deng_install_library (${_libDir}/SDL2.dll)

    # Also attempt to locate SLD2_mixer.
    deng_clean_path (sdlMixerDir ${SDL2_MIXER_DIR})
    set (_oldPath ${SDL_MIXER_LIBRARY})
    if (MSYS OR CYGWIN)
        if (SDL2_MIXER_DIR STREQUAL "")
            message (FATAL_ERROR "SDL2_MIXER_DIR must be set in MSYS/Cygwin")
        endif ()
        set (SDL2_MIXER_LIBRARY ${sdlMixerDir}/lib/${DE_ARCH}/SDL2_mixer.lib)
    else ()
        file (GLOB _hints ${sdlRoot}/SDL2_mixer* ${sdlMixerDir}/SDL2_mixer*
              $ENV{DENG_DEPEND_PATH}/SDL2_mixer*
        )
        find_library (SDL2_MIXER_LIBRARY SDL2_mixer
            PATHS ${sdlRoot} ${sdlMixerDir}
            HINTS ${_hints} ENV DENG_DEPEND_PATH
            PATH_SUFFIXES lib/${DE_ARCH} lib
        )
    endif ()
    mark_as_advanced (SDL2_MIXER_LIBRARY)
    if (NOT _oldPath STREQUAL SDL2_MIXER_LIBRARY)
        message (STATUS "Found SDL2_mixer: ${SDL2_MIXER_LIBRARY}")
    endif ()

    # Define the target.
    add_library (SDL2_mixer INTERFACE)
    target_link_libraries (SDL2_mixer INTERFACE ${SDL2_MIXER_LIBRARY})

    # Deduce the include directory.
    get_filename_component (_libDir ${SDL2_MIXER_LIBRARY} DIRECTORY)
    get_filename_component (_incDir ${_libDir}/../../include REALPATH)

    target_include_directories (SDL2_mixer INTERFACE ${_incDir})

    # There are multiple DLLs needed for deployment.
    file (GLOB _mixLibs ${_libDir}/*.dll)
    foreach (_lib IN LISTS _mixLibs)
        deng_install_library (${_lib})
    endforeach (_lib)
endif ()

if (NOT TARGET SDL2)
    add_definitions (-DDE_NO_SDL=1)
    message (STATUS "SDL2 disabled (not found).")
endif ()
if (NOT TARGET SDL2_mixer)
    add_definitions (-DDE_DISABLE_SDLMIXER=1)
    message (STATUS "SDL2_mixer disabled (not found).")
endif ()
