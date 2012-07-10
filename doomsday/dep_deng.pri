# Build configuration for using libdeng.
INCLUDEPATH += $$PWD/libdeng/include

# Use the appropriate library path.
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
