include(../config_test.pri)

TEMPLATE = app
TARGET = test_record

SOURCES += main.cpp

deployTest($$TARGET)
