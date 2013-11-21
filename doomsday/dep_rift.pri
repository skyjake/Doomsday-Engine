# Build configuration for Oculus Rift SDK

exists($${LIBOVR_DIR}/Include/OVR.h) {
    INCLUDEPATH += $${LIBOVR_DIR}/Include
    # TODO - LIBS statement for Linux
    win32 {
        LIBS += $${LIBOVR_DIR}/Lib/Win32/libovr.lib
        # Additional windows libraries needed to avoid link errors when using Rift
        LIBS += shell32.lib winmm.lib
    }
    macx {
        # ACK! Must rebuild libovr with RTTI (TODO)
        deng_debug: LIBS += $${LIBOVR_DIR}/Lib/MacOS/Debug/libovr.a
              else: LIBS += $${LIBOVR_DIR}/Lib/MacOS/Release/libovr.a
        LIBS += -framework IOKit
    }
    # For linux, you need to install libxinerama-dev and libudev-dev
    linux-g++|linux-g++-32 {
        deng_debug: LIBS += $${LIBOVR_DIR}/Lib/Linux/Debug/i386/libovr.a
              else: LIBS += $${LIBOVR_DIR}/Lib/Linux/Release/i386/libovr.a
        LIBS += -lX11 -ludev -lXinerama
    }
    linux-g++-64 { # 64-bit linux untested
        deng_debug: LIBS += $${LIBOVR_DIR}/Lib/Linux/Debug/x86_64/libovr.a
              else: LIBS += $${LIBOVR_DIR}/Lib/Linux/Release/x86_64/libovr.a
        LIBS += -lX11 -ludev -lXinerama
    }
    DEFINES += DENG_HAVE_OCULUS_API
    # message("Found Oculus Rift SDK")
} else {
    # message("Oculus Rift SDK not found")
}
