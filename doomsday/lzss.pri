# Build configuration for lzss.
DENG_LZSS_DIR = $$PWD/external/lzss

INCLUDEPATH += $$DENG_LZSS_DIR/portable/include

HEADERS += \
    $$DENG_LZSS_DIR/portable/include/lzss.h

win32 {
    LIBS += -L$$DENG_LZSS_DIR/win32 -llzss
}
else {
    SOURCES += \
        $$DENG_LZSS_DIR/unix/src/lzss.c
}
