include(../config_test.pri)

TEMPLATE = app
TARGET = test_archive

SOURCES += main.cpp

OTHER_FILES += hello.txt

deployTest($$TARGET)

archived.files = test.zip hello.txt
archived.path = $$DENG_DATA_DIR

macx {
    archived.path = Contents/Resources
    QMAKE_BUNDLE_DATA += archived
}
else {
    INSTALLS += archived
}
