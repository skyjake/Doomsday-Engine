# Build configuration for Oculus Rift SDK

exists($${LIBOVR_DIR}/Include/OVR.h) {
    INCLUDEPATH += $${LIBOVR_DIR}/Include
    # TODO - LIBS statement for Mac, Linux
    win32:LIBS += $${LIBOVR_DIR}/Lib/Win32/libovr.lib
    DEFINES += HAVE_OCULUS_API
    # message("Found Oculus Rift SDK")
} else {
    # message("Oculus Rift SDK not found")
}

