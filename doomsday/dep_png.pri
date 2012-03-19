# Build configuration for libpng.
win32 {
    INCLUDEPATH += $$PWD/external/libpng/portable/include
    LIBS += -L$$PWD/external/libpng/win32 -llibpng15

    # Installed shared libraries.
    INSTALLS += pnglibs
    pnglibs.files = $$PWD/external/libpng/win32/libpng15.dll
    pnglibs.path = $$DENG_LIB_DIR
}
else:macx {
    deng_macx4u_32bit {
        # Use a static libpng from MacPorts (ppc+i386).
        INCLUDEPATH += /opt/local/include
        LIBS += /opt/local/lib/libpng.a
    } else {
        # Use libpng in the system/SDK.
        INCLUDEPATH += $$QMAKE_MAC_SDK/usr/X11/include
        LIBS += -L$$QMAKE_MAC_SDK/usr/X11/lib -lpng
    }
}
else {
    # Generic Unix.
    QMAKE_CFLAGS += $$system(pkg-config libpng --cflags)
    LIBS += $$system(pkg-config libpng --libs)
}
