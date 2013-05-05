CONFIG += deng_qtopengl deng_qtgui

include(../config_test.pri)
include(../../dep_gui.pri)
include(../../dep_opengl.pri)

CONFIG -= console
TEMPLATE = app
TARGET = test_glsandbox

RESOURCES += glsandbox.qrc

SOURCES += \
    main.cpp \
    testwindow.cpp

deployTest($$TARGET)

gfx.files = testpic.png

win32 {
    gfx.path = $$DENG_DATA_DIR/graphics
    INSTALLS += gfx
}
else:macx {
    linkBinaryToBundledLibdengGui($${TARGET}.app/Contents/MacOS/$${TARGET})

    gfx.path = Contents/Resources/graphics
    QMAKE_BUNDLE_DATA += gfx
}

HEADERS += \
    testwindow.h
