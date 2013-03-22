include(../config_test.pri)

TEMPLATE = app
TARGET = test_string

SOURCES += main.cpp

deployTest($$TARGET)
