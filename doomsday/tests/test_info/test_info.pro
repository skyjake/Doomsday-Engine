include(../config_test.pri)

TEMPLATE = app
TARGET = test_info

SOURCES += main.cpp

SCRIPTS = test_info.dei

OTHER_FILES += $$SCRIPTS

deployTest($$TARGET)

scripts.files = $$SCRIPTS
scripts.path  = $$DENG_DATA_DIR

macx {
    scripts.path = Contents/Resources
    QMAKE_BUNDLE_DATA += scripts
}
else {
    INSTALLS += scripts
}
