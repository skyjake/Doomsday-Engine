!deng_nosdl {

# Build configuration for SDL (core library).
win32 {
    isEmpty(SDL_DIR) {
        error("dep_sdl: SDL path not defined, check your config_user.pri")
    }
    sdlLibDir = $$SDL_DIR/lib
    exists($$SDL_DIR/lib/x86): sdlLibDir = $$SDL_DIR/lib/x86
    
    INCLUDEPATH += $$SDL_DIR/include
    LIBS += -L$$sdlLibDir -lsdl

    # Libraries to copy to the products directory.
    INSTALLS += sdllibs
    sdllibs.files = $$sdlLibDir/SDL.dll
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
        sdlMixerLibDir = $$SDL_MIXER_DIR/lib
        exists($$SDL_MIXER_DIR/lib/x86): sdlMixerLibDir = $$SDL_MIXER_DIR/lib/x86
        
        INCLUDEPATH += $$SDL_MIXER_DIR/include
        LIBS += -L$$sdlMixerLibDir -lsdl_mixer

        # Libraries to copy to the products directory.
        INSTALLS += sdlmixerlibs
        sdlmixerlibs.files = \
            $$sdlMixerLibDir/libogg-0.dll \
            $$sdlMixerLibDir/libvorbis-0.dll \
            $$sdlMixerLibDir/libvorbisfile-3.dll \
            $$sdlMixerLibDir/mikmod.dll \
            $$sdlMixerLibDir/SDL_mixer.dll \
            $$sdlMixerLibDir/smpeg.dll
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

} # !deng_nosdl