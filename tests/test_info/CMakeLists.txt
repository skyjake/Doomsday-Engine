cmake_minimum_required (VERSION 3.10)
project (DE_TEST_INFO)
include (../TestConfig.cmake)

deng_test (test_info main.cpp)

install (FILES test_info.dei DESTINATION ${DE_INSTALL_DATA_DIR})

