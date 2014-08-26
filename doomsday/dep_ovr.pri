# Build configuration for LibOVR 0.4 (Oculus Rift SDK)

exists($${LIBOVR_DIR}/Include/OVR.h) {
    INCLUDEPATH += $${LIBOVR_DIR}/Include
    win32 {
        include(dep_atl.pri)

        deng_debug: LIBS += $${LIBOVR_DIR}/Lib/Win32/VS2013/libovrd.lib
              else: LIBS += $${LIBOVR_DIR}/Lib/Win32/VS2013/libovr.lib

        # Additional windows libraries needed to avoid link errors when using Rift
        LIBS += shell32.lib winmm.lib ws2_32.lib
    }
    macx {
        deng_debug: LIBS += $${LIBOVR_DIR}/Lib/Mac/Debug/libovr.a
              else: LIBS += $${LIBOVR_DIR}/Lib/Mac/Release/libovr.a
        useFramework(Cocoa)
        useFramework(IOKit)
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
} else {
    # message("Oculus Rift SDK not found")
}
