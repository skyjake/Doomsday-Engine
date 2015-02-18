set (LIBOVR_DIR "LibOVR" CACHE PATH "Location of the LibOVR library (from Oculus SDK)")

if (NOT LIBOVR_FOUND)
    get_filename_component (fullPath "${LIBOVR_DIR}" ABSOLUTE)
    if (EXISTS "${fullPath}/Include")
        set (LIBOVR_FOUND YES CACHE BOOL "LibOVR found")
        mark_as_advanced (LIBOVR_FOUND)
        message (STATUS "Found LibOVR: ${LIBOVR_DIR}")
    endif ()
endif ()

if (LIBOVR_FOUND AND NOT TARGET LibOVR)
    add_library (LibOVR INTERFACE)
    target_include_directories (LibOVR INTERFACE "${LIBOVR_DIR}/Include" "${LIBOVR_DIR}/Src")
    target_compile_definitions (LibOVR INTERFACE -DDENG_HAVE_OCULUS_API)
    if (APPLE)
        target_link_libraries (LibOVR INTERFACE
            debug     "${LIBOVR_DIR}/Lib/Mac/Debug/libovr.a"
            optimized "${LIBOVR_DIR}/Lib/Mac/Release/libovr.a"
        )
        link_framework (LibOVR INTERFACE Cocoa)
        link_framework (LibOVR INTERFACE IOKit)
    endif ()
endif ()
