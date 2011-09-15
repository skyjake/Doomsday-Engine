# Build configuration for EAX.
win32 {
    INCLUDEPATH += "C:/SDK/EAX 2.0 SDK/Include"
    LIBS += -L"C:/SDK/EAX 2.0 SDK/Libs" -leax -leaxguid

    # Libraries for the built products.
    INSTALLS += eaxlibs
    eaxlibs.files = "C:/SDK/EAX 2.0 SDK/dll/eax.dll"
    eaxlibs.path = $$DENG_WIN_PRODUCTS_DIR
}
