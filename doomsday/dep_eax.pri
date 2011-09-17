# Build configuration for EAX.
win32 {
    isEmpty(EAX2_DIR) {
        error("dep_eax: EAX 2.0 SDK path not defined, check your config_user.pri")
    }

    INCLUDEPATH += $$EAX2_DIR/Include
    LIBS += -L$$EAX2_DIR/Libs -leax -leaxguid

    # Libraries for the built products.
    INSTALLS += eaxlibs
    eaxlibs.files = $$EAX2_DIR/dll/eax.dll
    eaxlibs.path = $$DENG_WIN_PRODUCTS_DIR
}
