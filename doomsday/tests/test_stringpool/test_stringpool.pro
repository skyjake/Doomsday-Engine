include(../config_test.pri)

TEMPLATE = app
TARGET = test_stringpool

SOURCES += main.cpp

deployTest($$TARGET)
