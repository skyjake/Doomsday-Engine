# Build configuration for OpenGL.
win32 {
    isEmpty(OPENGL_DIR) {
        error("dep_opengl: OpenGL SDK path not defined, check your config_user.pri")
    }

    # Windows.
    INCLUDEPATH += $$OPENGL_DIR
}
else:macx {
    # Mac OS X.
    deng_macx8_64bit {
        INCLUDEPATH += /System/Library/Frameworks/OpenGL.framework/Headers
    }
    else: INCLUDEPATH += /Library/Frameworks/OpenGL.framework/Headers

    LIBS += -framework OpenGL
}
else {
    # Generic Unix.
    LIBS += -lGL -lGLU
}
