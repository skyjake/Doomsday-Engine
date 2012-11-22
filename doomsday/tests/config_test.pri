include(../config.pri)
include(../dep_deng2.pri)

cfg.files = $$DENG_CONFIG_DIR/deng.de

macx {
    cfg.path = Contents/Resources/config

    defineTest(deployTest) {
        # Arg 1: target name (without .app)
        fwDir = \"$${OUT_PWD}/$${1}.app/Contents/Frameworks/\"
        doPostLink("rm -rf $$fwDir")
        doPostLink("mkdir $$fwDir")
        doPostLink("ln -s $$OUT_PWD/../../libdeng2/libdeng2.dylib $$fwDir/libdeng2.dylib")
        doPostLink("ln -s $$OUT_PWD/../../libdeng2/libdeng2.2.dylib $$fwDir/libdeng2.2.dylib")

        # Fix the dynamic linker paths so they point to ../Frameworks/ inside the bundle.
        fixInstallName($${1}.app/Contents/MacOS/$${1}, libdeng2.2.dylib, ..)

        QMAKE_BUNDLE_DATA += cfg
        export(QMAKE_BUNDLE_DATA)
    }
}
else:win32 {
    CONFIG += console

    target.path = $$DENG_BIN_DIR
    cfg.path = $$DENG_DATA_DIR/config

    defineTest(deployTest) {
        INSTALLS += cfg target
        export(INSTALLS)
    }
}
else {
    target.path = $$DENG_BIN_DIR
    cfg.path = $$DENG_DATA_DIR/config

    defineTest(deployTest) {
        INSTALLS += cfg target
        export(INSTALLS)
    }
}
