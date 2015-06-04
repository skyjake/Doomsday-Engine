cmake_minimum_required (VERSION 3.1)
project (DENG_TEST_GLSANDBOX)
include (../TestConfig.cmake)

find_package (Qt5 COMPONENTS Gui OpenGL Widgets)
find_package (DengAppfw)
include (OpenGL)

deng_add_package (net.dengine.test.glsandbox)

deng_add_application (test_glsandbox
    main.cpp
    testwindow.cpp
    testwindow.h
    glsandbox.qrc
)

target_link_libraries (test_glsandbox PRIVATE Deng::libappfw opengl)

if (APPLE)
    deng_install_deployqt (test_glsandbox)
endif ()
