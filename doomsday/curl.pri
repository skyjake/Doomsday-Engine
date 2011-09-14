# Build configuration for cURL.
win32 {
    INCLUDEPATH += $$PWD/external/libcurl/portable/include
    LIBS += -L$$PWD/external/libcurl/win32 -lcurllib
}
else:macx {
    LIBS += -lcurl
}
else {
    # Generic Unix.
    QMAKE_CFLAGS += $$system(pkg-config libcurl --cflags)
    LIBS += $$system(pkg-config libcurl --libs)
}
