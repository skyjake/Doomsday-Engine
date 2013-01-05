include(../config.pri)

macx {
    CONFIG += lib_bundle
    QMAKE_BUNDLE_EXTENSION = .bundle
}
win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

INCLUDEPATH += $$DENG_API_DIR

!dengplugin_libdeng2_full {
    # The libdeng2 C wrapper can be used from all plugins.
    DEFINES += DENG_NO_QT DENG2_C_API_ONLY
    include(../dep_deng2_cwrapper.pri)
}
else: include(../dep_deng2.pri)

include(../dep_deng.pri)
