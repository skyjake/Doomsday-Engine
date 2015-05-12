# Build configuration for LibOVR 0.4 (Oculus Rift SDK)

exists($${LIBOVR_DIR}/Include/OVR.h) {
    INCLUDEPATH += $${LIBOVR_DIR}/Include
    win32 {
        exists($${LIBOVR_DIR}/../output) {
            # This is a community build of LibOVR.
            INCLUDEPATH += $${LIBOVR_DIR}/../Bindings/C/Include
            libs = $${LIBOVR_DIR}/../output
            INSTALLS += ovrcbind
            ovrcbind.path = $$DENG_BIN_DIR
            deng_sdk: ovrcbind.path = $$DENG_SDK_LIB_DIR
            deng_debug {
                LIBS += $$libs/OculusVRd.lib $$libs/OVR_Cd.lib
                ovrcbind.files = $$libs/OVR_Cd.dll
            }
            else {
                LIBS += $$libs/OculusVR.lib $$libs/OVR_C.lib
                ovrcbind.files = $$libs/OVR_C.dll
            }
        }
        else {
            deng_debug: LIBS += $${LIBOVR_DIR}/Lib/Win32/VS2013/libovrd.lib
                  else: LIBS += $${LIBOVR_DIR}/Lib/Win32/VS2013/libovr.lib
            INCLUDEPATH += $${LIBOVR_DIR}/Src
        }

        # Additional windows libraries needed to avoid link errors when using Rift
        LIBS += shell32.lib winmm.lib ws2_32.lib
    }
    macx {
        INCLUDEPATH += $${LIBOVR_DIR}/Src
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
