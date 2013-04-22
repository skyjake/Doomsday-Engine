include(../config_test.pri)

TEMPLATE = app
TARGET = test_bitfield

SOURCES += main.cpp

deployTest($$TARGET)
