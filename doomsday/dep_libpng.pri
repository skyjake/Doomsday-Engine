# Build configuration for libpng.
win32 {
    INCLUDEPATH += $$PWD/external/libpng/portable/include
    LIBS += -L$$PWD/external/libpng/win32 -llibpng13

    # Installed shared libraries.
    INSTALLS += pnglibs
    pnglibs.files = $$PWD/external/libpng/win32/libpng13.dll
    pnglibs.path = $$DENG_WIN_PRODUCTS_DIR
}
else:macx {
    INCLUDEPATH += $$QMAKE_MAC_SDK/usr/X11/include
    LIBS += -L$$QMAKE_MAC_SDK/usr/X11/lib -lpng
}
else {
    # Generic Unix.
    QMAKE_CFLAGS += $$system(pkg-config libpng --cflags)
    LIBS += $$system(pkg-config libpng --libs)
}
