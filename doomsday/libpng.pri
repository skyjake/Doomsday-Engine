# Build configuration for libpng.
macx {
    INCLUDEPATH += $$QMAKE_MAC_SDK/usr/X11/include
    LIBS += -L$$QMAKE_MAC_SDK/usr/X11/lib -lpng
}
else:win32 {
}
else {
    # Generic Unix.
    QMAKE_CFLAGS += $$system(pkg-config libpng --cflags)
    LIBS += $$system(pkg-config libpng --libs)
}
