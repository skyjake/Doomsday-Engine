# Build configuration for using libdeng.
LIBDENG_INCLUDE_DIR = $$PWD/libdeng/include
INCLUDEPATH += $$LIBDENG_INCLUDE_DIR

win32 {
    # Tell rc where to get the API headers.
    QMAKE_RC = $$QMAKE_RC /I \"$$LIBDENG_INCLUDE_DIR\"
}

# Use the appropriate library path (trying some alternatives).
!useLibDir($$OUT_PWD/../libdeng) {
    !useLibDir($$OUT_PWD/../../libdeng) {
        useLibDir($$OUT_PWD/../../builddir/libdeng)
    }
}

LIBS += -ldeng1

macx {
    defineTest(linkToBundledLibdeng) {
        fixInstallName($${1}.bundle/$$1, libdeng1.1.dylib, ..)
    }
}
