# Build configuration for using the libcore C wrapper.
INCLUDEPATH += $$PWD/libcore/include

# Use the appropriate library path.
!useLibDir($$OUT_PWD/../libcore) {
    !useLibDir($$OUT_PWD/../../libcore) {
        !useLibDir($$OUT_PWD/../../../libcore) {
            useLibDir($$OUT_PWD/../../builddir/libcore)
        }
    }
}

LIBS += -ldeng_core

macx {
    defineTest(linkToBundledLibcore) {
        fixInstallName($${1}.bundle/$$1, libdeng_core.2.dylib, ..)
    }
}
