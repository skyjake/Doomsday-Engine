include(../config_test.pri)

TEMPLATE = app
TARGET = test_record

SOURCES += main.cpp

macx {
    cfg.files = $$DENG_CONFIG_DIR/deng.de
    cfg.path = Contents/Resources/config

    QMAKE_BUNDLE_DATA += cfg

    macDeployTest($$TARGET)
}
