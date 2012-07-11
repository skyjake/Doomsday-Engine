# Build configuration for using the libdeng2 C wrapper.
INCLUDEPATH += $$PWD/libdeng2/include

# Use the appropriate library path.
!useLibDir($$OUT_PWD/../libdeng2) {
    !useLibDir($$OUT_PWD/../../libdeng2) {
	useLibDir($$OUT_PWD/../../builddir/libdeng2)
    }
}

LIBS += -ldeng2

macx {
    defineTest(linkToBundledLibdeng2) {
        fixInstallName($${1}.bundle/$$1, libdeng2.2.dylib, ..)
    }
}
