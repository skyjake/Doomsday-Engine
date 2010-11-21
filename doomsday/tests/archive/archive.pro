include(../tests.pri)

TEMPLATE = app
TARGET = test_archive

SOURCES += main.cpp

macx {
    TEST_FILES.files = test.zip hello.txt
    TEST_FILES.path = Contents/Resources
    QMAKE_BUNDLE_DATA += TEST_FILES
}
