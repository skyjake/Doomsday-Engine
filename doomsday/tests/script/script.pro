include(../config_test.pri)

TEMPLATE = app
TARGET = test_script

SOURCES += main.cpp

SCRIPTS = kitchen_sink.de sections.de

OTHER_FILES += $$SCRIPTS

macx {
    scripts.files = $$SCRIPTS
    scripts.path = Contents/Resources
    QMAKE_BUNDLE_DATA += scripts

    macDeployTest($$TARGET)
}
