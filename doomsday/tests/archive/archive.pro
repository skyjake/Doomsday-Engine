include(../config_test.pri)

TEMPLATE = app
TARGET = test_archive

SOURCES += main.cpp

OTHER_FILES += hello.txt

macx {
    archived.files = test.zip hello.txt
    archived.path = Contents/Resources

    cfg.files = $$DENG_CONFIG_DIR/deng.de
    cfg.path = Contents/Resources/config

    QMAKE_BUNDLE_DATA += archived cfg

    macDeployTest($$TARGET)
}
