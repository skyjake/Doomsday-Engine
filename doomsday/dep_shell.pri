# Build configuration for using the libdeng_shell library.
shellDir = tools/shell/libshell
INCLUDEPATH += $$PWD/$$shellDir/include

# Use the appropriate library path.
!useLibDir($$OUT_PWD/../$$shellDir) {
    !useLibDir($$OUT_PWD/../../$$shellDir) {
        !useLibDir($$OUT_PWD/../../../$$shellDir) {
            useLibDir($$OUT_PWD/../../builddir/$$shellDir)
        }
    }
}

LIBS += -ldeng_shell

macx {
    defineTest(linkBinaryToBundledLibdengShell) {
        fixInstallName($${1}, libdeng_shell.0.dylib, ..)
    }
    defineTest(linkToBundledLibdengShell) {
        linkBinaryToBundledLibdengShell($${1}.bundle/$$1)
    }
}
