# Build configuration for using C/legacy support.
LIBDENG_INCLUDE_DIR = $$PWD/liblegacy/include
INCLUDEPATH += $$LIBDENG_INCLUDE_DIR

win32 {
    # Tell rc where to get the API headers.
    QMAKE_RC = $$QMAKE_RC /I \"$$LIBDENG_INCLUDE_DIR\"
}

# Use the appropriate library path (trying some alternatives).
!useLibDir($$OUT_PWD/../liblegacy) {
    !useLibDir($$OUT_PWD/../../liblegacy) {
        useLibDir($$OUT_PWD/../../builddir/liblegacy)
    }
}

LIBS += -ldeng_legacy

macx {
    defineTest(linkBinaryToBundledLiblegacy) {
        fixInstallName($${1}, libdeng_legacy.1.dylib, ..)
    }
    defineTest(linkToBundledLiblegacy) {
        fixInstallName($${1}.bundle/$$1, libdeng_legacy.1.dylib, ..)
    }
    defineTest(linkDylibToBundledLiblegacy) {
        fixInstallName($${1}.dylib, libdeng_legacy.1.dylib, ..)
    }
}
