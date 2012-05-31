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
    # Check that the dev library is installed.
    !system(pkg-config --exists openal) {
        error(Missing dependency: OpenAL library (development headers). Alternatively disable the OpenAL plugin by removing deng_openal from CONFIG.)
    }
    oalFlags = $$system(pkg-config --cflags openal)
    QMAKE_CFLAGS += $$oalFlags
    QMAKE_CXXFLAGS += $$oalFlags
    LIBS += $$system(pkg-config --libs openal)
}
