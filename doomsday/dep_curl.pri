# Build configuration for cURL.
win32 {
    CURL_DIR = $$PWD/external/libcurl
    
    INCLUDEPATH += $$CURL_DIR/portable/include
    LIBS += -l$$CURL_DIR/win32/libcurl

    # Install the libcurl shared library.
    INSTALLS += curllibs
    curllibs.files = $$CURL_DIR/win32/libcurl.dll
    curllibs.path = $$DENG_LIB_DIR
}
else:macx {
    LIBS += -lcurl
}
else {
    # Generic Unix.
    QMAKE_CFLAGS += $$system(pkg-config libcurl --cflags)
    LIBS += $$system(pkg-config libcurl --libs)
}
