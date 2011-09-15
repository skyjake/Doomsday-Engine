# Build configuration for OpenAL.
win32 {
    # Windows.
    INCLUDEPATH += "C:/SDK/OpenAL 1.1 SDK/include"
    LIBS += -L"C:/SDK/OpenAL 1.1 SDK/libs/win32" -lopenal32
}
else:macx {
    # Mac OS X.
}
else {
    # Generic Unix.
}
