cmake_minimum_required (VERSION 3.1)
project (DE_TEST_ARCHIVE)
include (../TestConfig.cmake)

deng_test (test_archive main.cpp)

install (FILES test.zip hello.txt DESTINATION ${DE_INSTALL_DATA_DIR})
