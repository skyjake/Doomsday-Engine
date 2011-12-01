# Build configuration for FMOD Ex.
win32 {
    isEmpty(FMOD_DIR) {
        error("dep_fmod: FMOD SDK path not defined, check your config_user.pri")
    }

    # Windows.
    INCLUDEPATH += $$FMOD_DIR/api/inc
    LIBS += -L$$FMOD_DIR/api/lib -lfmodex_vc
}
else:macx {
    # Mac OS X.
    INCLUDEPATH += $$FMOD_DIR/api/inc
    LIBS += -L$$FMOD_DIR/api/lib -lfmodex
}
else {
    # Generic Unix.
    # TODO!
}
