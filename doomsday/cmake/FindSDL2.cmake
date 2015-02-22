find_package (PkgConfig QUIET)

if (PKG_CONFIG_FOUND)
    add_pkgconfig_interface_library (SDL2 OPTIONAL sdl2)
    add_pkgconfig_interface_library (SDL2_mixer OPTIONAL SDL2_mixer)
elseif (WIN32)
    # Try to locate SDL2 from the local system (assuming Windows).
    set (_oldPath ${SDL2_LIBRARY})
    find_library (SDL2_LIBRARY SDL2 HINTS ${SDL2_DIR} PATH_SUFFIXES lib/x86 lib)    
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
    
    target_include_directories (SDL2 INTERFACE ${_incDir})
    deng_install_library (${_libDir}/SDL2.dll)
    
    # Also attempt to locate SLD2_mixer.
    set (_oldPath ${SDL_MIXER_LIBRARY})
    find_library (SDL2_MIXER_LIBRARY SDL2_mixer
        HINTS ${SDL2_DIR} ${SDL2_MIXER_DIR}
        PATH_SUFFIXES lib/x86 lib
    )    
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
    add_definitions (-DDENG_NO_SDL)
    message (STATUS "SDL2 disabled (not found).")
endif ()
if (NOT TARGET SDL2_mixer)
    add_definitions (-DDENG_DISABLE_SDLMIXER)
    message (STATUS "SDL2_mixer disabled (not found).")
endif ()
