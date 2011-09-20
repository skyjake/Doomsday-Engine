# Build configuration for SDL.
win32 {
    isEmpty(SDL_DIR) {
        error("dep_sdl: SDL path not defined, check your config_user.pri")
    }
    isEmpty(SDL_MIXER_DIR) {
        error("dep_sdl: SDL_mixer path not defined, check your config_user.pri")
    }

    # Windows.
    INCLUDEPATH += \
        $$SDL_DIR/include \
        $$SDL_MIXER_DIR/include
    LIBS += -L$$SDL_DIR/lib -L$$SDL_MIXER_DIR/lib \
        -lsdl -lsdl_mixer

    # Libraries to copy to the products directory.
    INSTALLS += sdllibs
    sdllibs.files = \
        $$SDL_DIR/lib/SDL.dll \
        $$SDL_MIXER_DIR/lib/libogg-0.dll \
        $$SDL_MIXER_DIR/lib/libvorbis-0.dll \
        $$SDL_MIXER_DIR/lib/libvorbisfile-3.dll \
        $$SDL_MIXER_DIR/lib/mikmod.dll \
        $$SDL_MIXER_DIR/lib/SDL_mixer.dll \
        $$SDL_MIXER_DIR/lib/smpeg.dll
    sdllibs.path = $$DENG_WIN_PRODUCTS_DIR
}
else:macx {
    # Mac OS X.
    isEmpty(SDL_FRAMEWORK_DIR) {
        error("dep_sdl: SDL framework path not defined, check your config_user.pri")
    }

    INCLUDEPATH += \
        $${SDL_FRAMEWORK_DIR}/SDL.framework/Headers \
        $${SDL_FRAMEWORK_DIR}/SDL_mixer.framework/Headers

    QMAKE_LFLAGS += -F$${SDL_FRAMEWORK_DIR} -F$${SDL_FRAMEWORK_DIR}/SDL_mixer.framework/Frameworks

    LIBS += -framework SDL -framework SDL_mixer
}
else {
    # Generic Unix.
    QMAKE_CFLAGS += $$system(pkg-config sdl --cflags)
    LIBS += $$system(pkg-config sdl --libs) -lSDL_mixer
}
