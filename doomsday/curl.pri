# Build configuration for cURL.
macx {
    LIBS += -lcurl
}
else:win32 {
}
else {
    # Generic Unix.
    QMAKE_CFLAGS += $$system(pkg-config libcurl --cflags)
    LIBS += $$system(pkg-config libcurl --libs)
}
