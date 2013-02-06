# Build configuration for using the libdeng_gui library.
INCLUDEPATH += $$PWD/libgui/include

# Use the appropriate library path.
!useLibDir($$OUT_PWD/../libgui) {
    !useLibDir($$OUT_PWD/../../libgui) {
        !useLibDir($$OUT_PWD/../../../libgui) {
            useLibDir($$OUT_PWD/../../builddir/libgui)
        }
    }
}

LIBS += -ldeng_gui

macx {
    defineTest(linkBinaryToBundledLibdengGui) {
        fixInstallName($${1}, libdeng_gui.0.dylib, ..)
    }
    defineTest(linkToBundledLibdengGui) {
        linkBinaryToBundledLibdengGui($${1}.bundle/$$1)
    }
}
