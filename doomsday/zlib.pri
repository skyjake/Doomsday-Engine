# Build configuration for zlib.
win32 {
    INCLUDEPATH += $$PWD/external/zlib/portable/include
    LIBS += -L$$PWD/external/zlib/win32 -lzlib1
}
else:macx {
    LIBS += -lz
}
