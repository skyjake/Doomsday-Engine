cmake_minimum_required (VERSION 3.1)
project (DE_TEST_SCRIPT)
include (../TestConfig.cmake)

deng_test (test_script main.cpp)

install (FILES kitchen_sink.ds sections.ds DESTINATION ${DE_INSTALL_DATA_DIR})
