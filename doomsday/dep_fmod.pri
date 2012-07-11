# Build configuration for FMOD Ex.
isEmpty(FMOD_DIR) {
    error("dep_fmod: FMOD SDK path not defined, check your config_user.pri (FMOD_DIR)")
}

INCLUDEPATH += $$FMOD_DIR/api/inc

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
    contains(QMAKE_HOST.arch, x86_64): libname = fmodex64
    else: libname = fmodex

    LIBS += -L$$FMOD_DIR/api/lib -l$$libname

    INSTALLS += fmodlibs
    fmodlibs.files = $$FMOD_DIR/api/lib/lib$${libname}.so
    fmodlibs.path = $$DENG_LIB_DIR
}
