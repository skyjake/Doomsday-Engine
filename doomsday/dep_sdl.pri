!deng_nosdl {

# Build configuration for SDL (core library).
win32 {
    isEmpty(SDL_DIR) {
        error("dep_sdl: SDL path not defined, check your config_user.pri")
    }

    INCLUDEPATH += $$SDL_DIR/include
    LIBS += -L$$SDL_DIR/lib -lsdl

    # Libraries to copy to the products directory.
    INSTALLS += sdllibs
    sdllibs.files = $$SDL_DIR/lib/SDL.dll
    sdllibs.path = $$DENG_LIB_DIR
}
else:macx {
    # Mac OS X.
    isEmpty(SDL_FRAMEWORK_DIR) {
        error("dep_sdl: SDL framework path not defined, check your config_user.pri")
    }

    INCLUDEPATH += $${SDL_FRAMEWORK_DIR}/SDL.framework/Headers
    QMAKE_LFLAGS += -F$${SDL_FRAMEWORK_DIR}

    LIBS += -framework SDL
}
else {
    # Generic Unix.
    sdlflags = $$system(pkg-config sdl --cflags)
    QMAKE_CFLAGS += $$sdlflags
    QMAKE_CXXFLAGS += $$sdlflags
    LIBS += $$system(pkg-config sdl --libs)
}

# Should we include SDL_mixer in the build, too?
!deng_nosdlmixer {
    win32 {
        isEmpty(SDL_MIXER_DIR) {
            error("dep_sdl: SDL_mixer path not defined, check your config_user.pri")
        }
        INCLUDEPATH += $$SDL_MIXER_DIR/include
        LIBS += -L$$SDL_MIXER_DIR/lib -lsdl_mixer

        # Libraries to copy to the products directory.
        INSTALLS += sdlmixerlibs
        sdlmixerlibs.files = \
            $$SDL_MIXER_DIR/lib/libogg-0.dll \
            $$SDL_MIXER_DIR/lib/libvorbis-0.dll \
            $$SDL_MIXER_DIR/lib/libvorbisfile-3.dll \
            $$SDL_MIXER_DIR/lib/mikmod.dll \
            $$SDL_MIXER_DIR/lib/SDL_mixer.dll \
            $$SDL_MIXER_DIR/lib/smpeg.dll
        sdlmixerlibs.path = $$DENG_LIB_DIR
    }
    else:macx {
        INCLUDEPATH += $${SDL_FRAMEWORK_DIR}/SDL_mixer.framework/Headers
        QMAKE_LFLAGS += -F$${SDL_FRAMEWORK_DIR}/SDL_mixer.framework/Frameworks
        LIBS += -framework SDL_mixer
    }
    else {
        LIBS += -lSDL_mixer
    }
}

}