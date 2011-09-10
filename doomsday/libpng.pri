# Build configuration for libpng.
macx {
    INCLUDEPATH += $$QMAKE_MAC_SDK/usr/X11/include
    LIBS += -L$$QMAKE_MAC_SDK/usr/X11/lib -lpng
}
