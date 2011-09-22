# Build configuration for DirectX.
win32 {
    isEmpty(DIRECTX_DIR) {
        error("dep_directx: DirectX SDK path not defined, check your config_user.pri")
    }

    INCLUDEPATH += $$DIRECTX_DIR/Include
    LIBS += -L$$DIRECTX_DIR/Lib/x86 -ldinput8 -ldsound -ldxguid
}
