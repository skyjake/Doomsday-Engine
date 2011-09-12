# Build configuration for OpenGL.
macx {
    # Mac OS X.
    INCLUDEPATH += /Library/Frameworks/OpenGL.framework/Headers
    LIBS += -framework OpenGL
}
else:win32 {
    # Windows.
}
else {
    # Generic Unix.
    LIBS += -lGL -lGLU
}
