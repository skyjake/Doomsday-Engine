include(../config_test.pri)

TEMPLATE = app
TARGET = test_script

SOURCES += main.cpp

SCRIPTS = kitchen_sink.de sections.de

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
