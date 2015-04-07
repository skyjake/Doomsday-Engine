cmake_minimum_required (VERSION 3.1)
project (DENG_TEST_APPFW)
include (../TestConfig.cmake)

find_package (Qt5 COMPONENTS Gui OpenGL Widgets)
find_package (DengAppfw QUIET)
include (OpenGL)

deng_add_package (net.dengine.test.appfw)

file (GLOB SOURCES src/*.cpp src/*.h)
deng_add_application (test_appfw ${SOURCES})

target_link_libraries (test_appfw Deng::libappfw opengl)

if (APPLE)
    deng_install_deployqt (test_appfw)
endif ()
