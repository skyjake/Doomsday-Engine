# Build configuration for SDL.
macx {
    SDL_FRAMEWORK_DIR = $$(HOME)/Library/Frameworks

    INCLUDEPATH += \
        $${SDL_FRAMEWORK_DIR}/SDL.framework/Headers \
        $${SDL_FRAMEWORK_DIR}/SDL_mixer.framework/Headers

    QMAKE_LFLAGS += -F$${SDL_FRAMEWORK_DIR}

    LIBS += -framework SDL -framework SDL_mixer
}
