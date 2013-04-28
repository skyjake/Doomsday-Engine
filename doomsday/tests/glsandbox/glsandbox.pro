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

macx {
    linkBinaryToBundledLibdengGui($${TARGET}.app/Contents/MacOS/$${TARGET})
}

HEADERS += \
    testwindow.h
