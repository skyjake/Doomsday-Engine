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
models.files = $$files(models/*)

macx {
    linkBinaryToBundledLibdeng2($${TARGET}.app/Contents/MacOS/$${TARGET})
    linkBinaryToBundledLibdengGui($${TARGET}.app/Contents/MacOS/$${TARGET})

    gfx.path = Contents/Resources/graphics
    models.path = Contents/Resources/models
    QMAKE_BUNDLE_DATA += gfx models
}
else {
    gfx.path = $$DENG_DATA_DIR/graphics
    models.path = $$DENG_DATA_DIR/models
    INSTALLS += gfx models
}

HEADERS += \
    testwindow.h
