# Build configuration for using SDL 2

!deng_nosdl {

win32 {
    isEmpty(SDL_DIR) {
        error("dep_sdl: SDL path not defined, check your config_user.pri (SDL_DIR)")
    }
    sdlLibDir = $$SDL_DIR/lib
    exists($$SDL_DIR/lib/x86): sdlLibDir = $$SDL_DIR/lib/x86
    
    INCLUDEPATH += $$SDL_DIR/include
    LIBS += -L$$sdlLibDir -lsdl2

    # Libraries to copy to the products directory.
    INSTALLS += sdllibs
    sdllibs.files = $$sdlLibDir/SDL2.dll
    sdllibs.path = $$DENG_LIB_DIR
}
else:macx {
    # Mac OS X.
    isEmpty(SDL_FRAMEWORK_DIR) {
        error("dep_sdl: SDL framework path not defined, check your config_user.pri")
    }

    INCLUDEPATH += $${SDL_FRAMEWORK_DIR}/SDL2.framework/Headers
    QMAKE_LFLAGS += -F$${SDL_FRAMEWORK_DIR}

    LIBS += -framework SDL2
}
else {
    # Generic Unix.
    sdlflags = $$system(pkg-config sdl2 --cflags)
    QMAKE_CFLAGS += $$sdlflags
    QMAKE_CXXFLAGS += $$sdlflags
    LIBS += $$system(pkg-config sdl2 --libs)
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
        LIBS += -L$$sdlMixerLibDir -lsdl2_mixer

        # Libraries to copy to the products directory.
        INSTALLS += sdlmixerlibs
        sdlmixerlibs.files = \
            $$sdlMixerLibDir/libFLAC-8.dll \
            $$sdlMixerLibDir/libmikmod-2.dll \
            $$sdlMixerLibDir/libmodplug-1.dll \
            $$sdlMixerLibDir/libogg-0.dll \
            $$sdlMixerLibDir/libvorbis-0.dll \
            $$sdlMixerLibDir/libvorbisfile-3.dll \
            $$sdlMixerLibDir/SDL2_mixer.dll \
            $$sdlMixerLibDir/smpeg2.dll
        sdlmixerlibs.path = $$DENG_LIB_DIR
    }
    else:macx {
        INCLUDEPATH += $${SDL_FRAMEWORK_DIR}/SDL2_mixer.framework/Headers
        QMAKE_LFLAGS += -F$${SDL_FRAMEWORK_DIR}/SDL2_mixer.framework/Frameworks
        LIBS += -framework SDL2_mixer
    }
    else {
        # Generic Unix.
        sdlflags = $$system(pkg-config SDL2_mixer --cflags)
        QMAKE_CFLAGS += $$sdlflags
        QMAKE_CXXFLAGS += $$sdlflags
        LIBS += $$system(pkg-config SDL2_mixer --libs)
    }
}

} # !deng_nosdl
