# Build configuration for using SDL 2

!deng_nosdl {

win32 {
    isEmpty(SDL2_DIR) {
        error("dep_sdl: SDL path not defined, check your config_user.pri (SDL2_DIR)")
    }
    sdlLibDir = $$SDL2_DIR/lib
    exists($$SDL2_DIR/lib/x86): sdlLibDir = $$SDL2_DIR/lib/x86
    
    INCLUDEPATH += $$SDL2_DIR/include
    LIBS += -L$$sdlLibDir -lsdl2

    # Libraries to copy to the products directory.
    INSTALLS += sdllibs
    sdllibs.files = $$sdlLibDir/SDL2.dll
    sdllibs.path = $$DENG_LIB_DIR
}
else:macx {
    # Mac OS X.
    isEmpty(SDL2_FRAMEWORK_DIR) {
        error("dep_sdl: SDL2 framework path not defined, check your config_user.pri (SDL2_FRAMEWORK_DIR)")
    }

    INCLUDEPATH += $${SDL2_FRAMEWORK_DIR}/SDL2.framework/Headers
    QMAKE_LFLAGS += -F$${SDL2_FRAMEWORK_DIR}

    LIBS += -framework SDL2
}
else {
    # Generic Unix.
    sdlflags = $$system(pkg-config sdl2 --cflags)
    QMAKE_CFLAGS += $$sdlflags
    QMAKE_CXXFLAGS += $$sdlflags
    LIBS += $$system(pkg-config sdl2 --libs)
}

# Should we include SDL2_mixer in the build, too?
!deng_nosdlmixer {
    win32 {
        isEmpty(SDL2_MIXER_DIR) {
            error("dep_sdl: SDL2_mixer path not defined, check your config_user.pri")
        }
        sdlMixerLibDir = $$SDL2_MIXER_DIR/lib
        exists($$SDL2_MIXER_DIR/lib/x86): sdlMixerLibDir = $$SDL2_MIXER_DIR/lib/x86
        
        INCLUDEPATH += $$SDL2_MIXER_DIR/include
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
        INCLUDEPATH += $${SDL2_FRAMEWORK_DIR}/SDL2_mixer.framework/Headers
        QMAKE_LFLAGS += -F$${SDL2_FRAMEWORK_DIR}/SDL2_mixer.framework/Frameworks
        LIBS += -framework SDL2_mixer
    }
    else {
        LIBS += -lSDL2_mixer
    }
}

} # !deng_nosdl
