# Build configuration for zlib.
win32 {
    INCLUDEPATH += $$PWD/external/zlib/portable/include
    LIBS += -L$$PWD/external/zlib/win32 -lzlib1

    # Installed shared libraries.
    INSTALLS += zlibs
    zlibs.files = $$PWD/external/zlib/win32/zlib1.dll
    zlibs.path = $$DENG_WIN_PRODUCTS_DIR
}
else:macx {
    LIBS += -lz
}
