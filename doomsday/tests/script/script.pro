include(../tests.pri)

TEMPLATE = app
TARGET = test_script

SOURCES += main.cpp

macx {
    TEST_FILES.files = kitchen_sink.de sections.de
    TEST_FILES.path = Contents/Resources
    QMAKE_BUNDLE_DATA += TEST_FILES
}
