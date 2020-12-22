set (FMOD_DIR "" CACHE PATH "Location of the FMOD Programmer's API SDK")

set (_oldPath ${FMOD_FMOD_H})

# We may need to clean up the provided path.
deng_clean_path (fmodRoot ${FMOD_DIR})

find_file (FMOD_FMOD_H api/lowlevel/inc/fmod.h
    PATHS "${fmodRoot}"
    HINTS ENV DENG_DEPEND_PATH
    PATH_SUFFIXES "FMOD" "FMOD Programmers API" "FMOD Studio API Windows"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)
mark_as_advanced (FMOD_FMOD_H)

if (NOT _oldPath STREQUAL FMOD_FMOD_H)
    if (FMOD_FMOD_H)
        message (STATUS "Looking for FMOD Low Level Programmer API - found")
    else ()
        message (STATUS "Looking for FMOD Low Level Programmer API - not found (set the FMOD_DIR variable)")
    endif ()
endif ()

if (FMOD_FMOD_H AND NOT TARGET fmodex)
    get_filename_component (fmodInc "${FMOD_FMOD_H}" DIRECTORY)
    get_filename_component (fmodApi "${fmodInc}" DIRECTORY)

    add_library (fmodex INTERFACE)
    target_include_directories (fmodex INTERFACE ${fmodInc})
    set (fmodInstLib)
    if (IOS)
        if (IOS_PLATFORM STREQUAL SIMULATOR)
            set (fmodLib "${fmodApi}/lib/libfmod_iphonesimulator.a")
        else ()
            set (fmodLib "${fmodApi}/lib/libfmod_iphoneos.a")
        endif ()
        if (NOT EXISTS ${fmodLib})
            message (FATAL_ERROR "iOS version of FMOD library not found: ${FMOD_FMOD_H}")
        endif ()
        link_framework (fmodex INTERFACE AVFoundation)
        link_framework (fmodex INTERFACE AudioToolbox)
    elseif (APPLE)
        set (fmodLib "${fmodApi}/lib/libfmod.dylib")
        set (fmodInstLib ${fmodLib})
    elseif (WIN32 OR CYGWIN OR MSYS)
        if (ARCH_BITS EQUAL 64)
            set (fmodLib "${fmodApi}/lib/fmod64_vc.lib")
            set (fmodInstLib "${fmodApi}/lib/fmod64.dll")
        else ()
            set (fmodLib "${fmodApi}/lib/fmod_vc.lib")
            set (fmodInstLib "${fmodApi}/lib/fmod.dll")
        endif ()
    elseif (UNIX)
        if (ARCH_BITS EQUAL 64)
            set (fmodLib ${fmodApi}/lib/x86_64/libfmod.so)
        else ()
            set (fmodLib ${fmodApi}/lib/x86/libfmod.so)
        endif ()
        set (fmodInstLib ${fmodLib})
    endif ()
    target_link_libraries (fmodex INTERFACE ${fmodLib})
    install (TARGETS fmodex
        EXPORT fmod
        ARCHIVE DESTINATION ${DE_INSTALL_LIB_DIR} 
        COMPONENT libs
    )
    install (EXPORT fmod
        DESTINATION ${DE_INSTALL_CMAKE_DIR}/fmod
        FILE fmod-config.cmake
        NAMESPACE FMOD::
        COMPONENT sdk
    )
    if (fmodInstLib)
        deng_install_library (${fmodInstLib})
    endif ()
endif ()
