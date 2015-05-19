cmake_minimum_required (VERSION 3.1)
project (DENG_TEST_INFO)
include (../TestConfig.cmake)

deng_test (test_info main.cpp)

install (FILES test_info.dei DESTINATION ${DENG_INSTALL_DATA_DIR})
