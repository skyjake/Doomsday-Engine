include(../config_test.pri)

TEMPLATE = app
TARGET = test_commandline

SOURCES += main.cpp

deployTest($$TARGET)

