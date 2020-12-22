if (IOS)
    return ()
endif ()

# This is the version of LibOVR that is required.
set (LIBOVR_REQUIRED_VERSION 0.5.0.1)

option (DE_ENABLE_OCULUS "Enable/disable Oculus Rift support (if LibOVR was found)" ON)

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
        string (REGEX MATCH ".*#define OVR_PRODUCT_VERSION[ \t]+([0-9\\.-]+).*" _match ${_ovrVersionHeader})
        set (_product ${CMAKE_MATCH_1})
        string (REGEX MATCH ".*#define OVR_MAJOR_VERSION[ \t]+([0-9\\.-]+).*" _match ${_ovrVersionHeader})
        set (_major ${CMAKE_MATCH_1})
        string (REGEX MATCH ".*#define OVR_MINOR_VERSION[ \t]+([0-9\\.-]+).*" _match ${_ovrVersionHeader})
        set (_minor ${CMAKE_MATCH_1})
        string (REGEX MATCH ".*#define OVR_PATCH_VERSION[ \t]+([0-9\\.-]+).*" _match ${_ovrVersionHeader})
        set (_patch ${CMAKE_MATCH_1})
        set (ovrVersion ${_product}.${_major}.${_minor}.${_patch})
        set (_product)
        set (_major)
        set (_minor)
        set (_patch)
        set (_ovrVersionHeader)
        message (STATUS "Looking for LibOVR - found version ${ovrVersion}")
        if (NOT ovrVersion VERSION_EQUAL LIBOVR_REQUIRED_VERSION)
            message (WARNING "LibOVR ${LIBOVR_REQUIRED_VERSION} is required!")
        endif ()
    else ()
        message (STATUS "Looking for LibOVR - not found (set the LIBOVR_DIR variable)")
    endif ()
endif ()

if (LIBOVR_OVR_H AND DE_ENABLE_OCULUS)
    if (NOT TARGET LibOVR)
        get_filename_component (ovrDir "${LIBOVR_OVR_H}" DIRECTORY)
        get_filename_component (ovrDir "${ovrDir}" DIRECTORY)

        add_library (LibOVR INTERFACE)
        target_compile_definitions (LibOVR INTERFACE -DDE_HAVE_OCULUS_API)
        target_include_directories (LibOVR INTERFACE
            "${ovrDir}/Include"
            "${ovrDir}/../LibOVRKernel/Src"
        )
        if (APPLE)
            target_link_libraries (LibOVR INTERFACE
                debug     "${ovrDir}/Lib/Mac/Debug/LibOVR.framework"
                optimized "${ovrDir}/Lib/Mac/Release/LibOVR.framework"
            )
            link_framework (LibOVR INTERFACE Cocoa)
            link_framework (LibOVR INTERFACE IOKit)
        elseif (MSVC12)
            target_link_libraries (LibOVR INTERFACE
                debug     "${ovrDir}/Lib/Windows/Win32/Debug/VS2013/libovr.lib"
                optimized "${ovrDir}/Lib/Windows/Win32/Release/VS2013/libovr.lib"
                general   winmm shell32 ws2_32
            )
        endif ()
    endif ()
endif ()
