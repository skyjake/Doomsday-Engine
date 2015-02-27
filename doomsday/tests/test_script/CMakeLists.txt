cmake_minimum_required (VERSION 3.1)
project (DENG_TEST_SCRIPT)
include (../TestConfig.cmake)

deng_test (test_script main.cpp)

install (FILES kitchen_sink.de sections.de DESTINATION ${DENG_INSTALL_DATA_DIR})
