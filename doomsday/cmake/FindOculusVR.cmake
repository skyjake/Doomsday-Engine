# This is the version of LibOVR that is required.
set (LIBOVR_REQUIRED_VERSION 0.4.4)

set (LIBOVR_DIR "" CACHE PATH "Location of the LibOVR library (in the Oculus SDK)")

set (_oldPath ${LIBOVR_OVR_H})

find_file (LIBOVR_OVR_H Include/OVR.h
    PATHS "${LIBOVR_DIR}" 
    HINTS ENV DENG_DEPEND_PATH
    PATH_SUFFIXES LibOVR OculusSDK/LibOVR
    NO_DEFAULT_PATH
)
mark_as_advanced (LIBOVR_OVR_H)

if (NOT _oldPath STREQUAL LIBOVR_OVR_H)
    if (LIBOVR_OVR_H)
        # Check the version.
        get_filename_component (ovrDir "${LIBOVR_OVR_H}" DIRECTORY)
        file (READ ${ovrDir}/OVR_Version.h _ovrVersionHeader)
        string (REGEX MATCH ".*#define OVR_VERSION_STRING \"([0-9\\.-]+)\".*" _match 
            ${_ovrVersionHeader}
        )
        set (ovrVersion ${CMAKE_MATCH_1})
        set (_match)
        set (_ovrVersionHeader)
        message (STATUS "Looking for LibOVR - found version ${ovrVersion}")
        if (NOT ovrVersion VERSION_EQUAL LIBOVR_REQUIRED_VERSION)
            message (WARNING "LibOVR ${LIBOVR_REQUIRED_VERSION} is required!")
        endif ()
    else ()
        message (STATUS "Looking for LibOVR - not found (set the LIBOVR_DIR variable)")
    endif ()
endif ()

if (LIBOVR_OVR_H)
    if (NOT TARGET LibOVR)
        get_filename_component (ovrDir "${LIBOVR_OVR_H}" DIRECTORY)
        get_filename_component (ovrDir "${ovrDir}" DIRECTORY)

        add_library (LibOVR INTERFACE)
        target_include_directories (LibOVR INTERFACE "${ovrDir}/Include" "${ovrDir}/Src")
        target_compile_definitions (LibOVR INTERFACE -DDENG_HAVE_OCULUS_API)
        if (APPLE)
            target_link_libraries (LibOVR INTERFACE
                debug     "${ovrDir}/Lib/Mac/Debug/libovr.a"
                optimized "${ovrDir}/Lib/Mac/Release/libovr.a"
            )
            link_framework (LibOVR INTERFACE Cocoa)
            link_framework (LibOVR INTERFACE IOKit)
        elseif (MSVC12)
            target_link_libraries (LibOVR INTERFACE
                debug     "${ovrDir}/Lib/Win32/VS2013/libovrd.lib"
                optimized "${ovrDir}/Lib/Win32/VS2013/libovr.lib"
                general   winmm shell32 ws2_32
            )
        endif ()
    endif ()
endif ()
