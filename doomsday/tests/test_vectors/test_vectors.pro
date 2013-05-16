include(../config_test.pri)

TEMPLATE = app
TARGET = test_vectors

SOURCES += main.cpp

deployTest($$TARGET)
