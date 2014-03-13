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
model.files = \
    marine.md2 \
    boblampclean.md5anim \
    boblampclean.md5mesh \
    guard1_body.png \
    guard1_face.png \
    guard1_helmet.png \
    iron_grill.png \
    round_grill.png

macx {
    linkBinaryToBundledLibdeng2($${TARGET}.app/Contents/MacOS/$${TARGET})
    linkBinaryToBundledLibdengGui($${TARGET}.app/Contents/MacOS/$${TARGET})

    gfx.path = Contents/Resources/graphics
    QMAKE_BUNDLE_DATA += gfx
}
else {
    gfx.path = $$DENG_DATA_DIR/graphics
    model.path = $$DENG_DATA_DIR
    INSTALLS += gfx model
}

HEADERS += \
    testwindow.h
