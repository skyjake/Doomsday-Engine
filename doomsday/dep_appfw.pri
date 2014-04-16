# Build configuration for using the libdeng_appfw library.
INCLUDEPATH += $$PWD/libappfw/include

# Use the appropriate library path.
!useLibDir($$OUT_PWD/../libappfw) {
    !useLibDir($$OUT_PWD/../../libappfw) {
        !useLibDir($$OUT_PWD/../../../libappfw) {
            useLibDir($$OUT_PWD/../../builddir/libappfw)
        }
    }
}

LIBS += -ldeng_appfw

macx {
    defineTest(linkBinaryToBundledLibdengAppfw) {
        fixInstallName($${1}, libdeng_appfw.1.dylib, ..)
    }
    defineTest(linkToBundledLibdengAppfw) {
        linkBinaryToBundledLibdengAppfw($${1}.bundle/$$1)
    }
}
