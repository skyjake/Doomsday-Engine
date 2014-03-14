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
models.files = models/*

macx {
    linkBinaryToBundledLibdeng2($${TARGET}.app/Contents/MacOS/$${TARGET})
    linkBinaryToBundledLibdengGui($${TARGET}.app/Contents/MacOS/$${TARGET})

    gfx.path = Contents/Resources/graphics
    QMAKE_BUNDLE_DATA += gfx
}
else {
    gfx.path = $$DENG_DATA_DIR/graphics
    models.path = $$DENG_DATA_DIR/models
    INSTALLS += gfx models
}

HEADERS += \
    testwindow.h
