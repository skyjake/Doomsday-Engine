# Build configuration for OpenGL.
win32 {
    # Windows.
    INCLUDEPATH += C:/SDK/OpenGL
}
else:macx {
    # Mac OS X.
    INCLUDEPATH += /Library/Frameworks/OpenGL.framework/Headers
    LIBS += -framework OpenGL
}
else {
    # Generic Unix.
    LIBS += -lGL -lGLU
}
