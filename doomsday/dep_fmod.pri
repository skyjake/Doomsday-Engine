isEmpty(FMOD_DIR) {
    error("dep_fmod: FMOD SDK path not defined, check your config_user.pri (FMOD_DIR)")
}

INCLUDEPATH += $$FMOD_DIR/api/inc

# Build configuration for FMOD Ex.
win32 {
    # Windows.
    LIBS += -L$$FMOD_DIR/api/lib -lfmodex_vc

    INSTALLS += fmodlibs
    fmodlibs.files = $$FMOD_DIR/api/fmodex.dll
    fmodlibs.path = $$DENG_LIB_DIR
}
else:macx {
    # Mac OS X.
    LIBS += -L$$FMOD_DIR/api/lib -lfmodex

    # The library must be bundled as a post-build step.
}
else {
    # Generic Unix.
    isEmpty(FMOD_LIB) {
        error("dep_fmod: Set FMOD_LIB in config_user.pri to specify which library to link against")
    }

    contains(QMAKE_HOST.arch, x86_64): libname = fmodex64
    else: libname = fmodex

    LIBS += -L$$FMOD_DIR/api/lib -l$$FMOD_LIB

    INSTALLS += fmodlibs
    fmodlibs.files = $$FMOD_DIR/api/lib/lib$${FMOD_LIB}.so
    fmodlibs.path = $$DENG_LIB_DIR
}
