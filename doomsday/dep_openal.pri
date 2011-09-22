# Build configuration for OpenAL.
win32 {
    isEmpty(OPENAL_DIR) {
        error("dep_openal: OpenAL SDK path not defined, check your config_user.pri")
    }

    # Windows.
    INCLUDEPATH += $$OPENAL_DIR/include
    LIBS += -L$$OPENAL_DIR/libs/win32 -lopenal32
}
else:macx {
    # Mac OS X.
}
else {
    # Generic Unix.
}
