include(../config.pri)
include(../dep_deng2.pri)

macx {
    defineTest(macDeployTest) {
        # Arg 1: target name (without .app)
        fwDir = \"$${OUT_PWD}/$${1}.app/Contents/Frameworks/\"
        doPostLink("rm -rf $$fwDir")
        doPostLink("mkdir $$fwDir")
        doPostLink("cp -fRp $$OUT_PWD/../../libdeng2/libdeng2*dylib $$fwDir")

        # Fix the dynamic linker paths so they point to ../Frameworks/ inside the bundle.
        fixInstallName($${1}.app/Contents/MacOS/$${1}, libdeng2.2.dylib, ..)
    }
}
