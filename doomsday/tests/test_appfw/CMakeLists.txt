cmake_minimum_required (VERSION 3.0)
project (DENG_TEST_APPFW)
include (../TestConfig.cmake)

find_package (DengAppfw)

deng_add_package (net.dengine.test.appfw)

file (GLOB SOURCES src/*.cpp src/*.h)
deng_add_application (test_appfw ${SOURCES})

target_link_libraries (test_appfw Deng::libappfw)
