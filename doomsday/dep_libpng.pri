# Build configuration for libpng.
win32 {
    INCLUDEPATH += $$PWD/external/libpng/portable/include
    LIBS += -L$$PWD/external/libpng/win32 -llibpng15

    # Installed shared libraries.
    INSTALLS += pnglibs
    pnglibs.files = $$PWD/external/libpng/win32/libpng15.dll
    pnglibs.path = $$DENG_WIN_PRODUCTS_DIR
}
else:macx {
    deng_snowleopard {
        # Use libpng in the SDK.
        INCLUDEPATH += $$QMAKE_MAC_SDK/usr/X11/include
        LIBS += -L$$QMAKE_MAC_SDK/usr/X11/lib -lpng
    } else {
        # Use a static libpng from MacPorts (ppc+i386).
        INCLUDEPATH += /opt/local/include
        LIBS += /opt/local/lib/libpng.a
    }
}
else {
    # Generic Unix.
    QMAKE_CFLAGS += $$system(pkg-config libpng --cflags)
    LIBS += $$system(pkg-config libpng --libs)
}
