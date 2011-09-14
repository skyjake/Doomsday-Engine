# Build configuration for SDL.
win32 {
    # Windows.
    INCLUDEPATH += \
        C:/SDK/SDL-1.2.14/include \
        C:/SDK/SDL_mixer-1.2.11/include
    LIBS += -LC:/SDK/SDL-1.2.14/lib -LC:/SDK/SDL_mixer-1.2.11/lib \
        -lsdl -lsdl_mixer
}
else:macx {
    # Mac OS X.
    SDL_FRAMEWORK_DIR = $$(HOME)/Library/Frameworks

    INCLUDEPATH += \
        $${SDL_FRAMEWORK_DIR}/SDL.framework/Headers \
        $${SDL_FRAMEWORK_DIR}/SDL_mixer.framework/Headers

    QMAKE_LFLAGS += -F$${SDL_FRAMEWORK_DIR}

    LIBS += -framework SDL -framework SDL_mixer
}
else {
    # Generic Unix.
    QMAKE_CFLAGS += $$system(pkg-config sdl --cflags)
    LIBS += $$system(pkg-config sdl --libs) -lSDL_mixer
}
