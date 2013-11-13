# Build configuration for Oculus Rift SDK

exists($${LIBOVR_DIR}/Include/OVR.h) {
    INCLUDEPATH += $${LIBOVR_DIR}/Include
    # TODO - LIBS statement for Linux
    win32:LIBS += $${LIBOVR_DIR}/Lib/Win32/libovr.lib
    macx {
        # ACK! Must rebuild libovr with RTTI
        debug: LIBS += $${LIBOVR_DIR}/Lib/MacOS/Debug/libovr.a
        release: LIBS += $${LIBOVR_DIR}/Lib/MacOS/Release/libovr.a
        LIBS += -framework IOKit
    }
    # Additional windows libraries needed to avoid link errors when using Rift
    win32:LIBS += shell32.lib winmm.lib
    DEFINES += DENG_HAVE_OCULUS_API
    # message("Found Oculus Rift SDK")
} else {
    # message("Oculus Rift SDK not found")
}
