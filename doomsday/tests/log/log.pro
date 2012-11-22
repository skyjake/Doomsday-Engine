include(../config_test.pri)

TEMPLATE = app
TARGET = test_log

SOURCES += main.cpp

deployTest($$TARGET)
