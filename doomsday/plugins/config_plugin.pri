include(../config.pri)

macx {
    CONFIG += lib_bundle
    QMAKE_BUNDLE_EXTENSION = .bundle
}
win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

!deng_macx4u_32bit : !deng_macx6_32bit_64bit {
    *-g++* | *-gcc* | *-clang* {
        # In the game plugins there is a large number of thinkfunc_t related
        # casting from various types of functions. This should be removed
        # when the issue has been resolved:
        QMAKE_CFLAGS_WARN_ON += -Wno-incompatible-pointer-types
    }
}

INCLUDEPATH += $$DENG_API_DIR

!dengplugin_libdeng2_full {
    # The libdeng2 C wrapper can be used from all plugins.
    DEFINES += DENG_NO_QT DENG2_C_API_ONLY
    include(../dep_deng2_cwrapper.pri)
}
else: include(../dep_deng2.pri)

include(../dep_deng1.pri)
