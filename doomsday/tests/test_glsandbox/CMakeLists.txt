cmake_minimum_required (VERSION 3.1)
project (DE_TEST_GLSANDBOX)
include (../TestConfig.cmake)

find_package (Qt5 COMPONENTS Gui Widgets)
find_package (DengAppfw)

deng_add_package (net.dengine.test.glsandbox)

deng_add_application (test_glsandbox
    main.cpp
    testwindow.cpp
    testwindow.h
    glsandbox.qrc
)

target_link_libraries (test_glsandbox PRIVATE Deng::libappfw)

if (APPLE)
    set_property (TARGET test_glsandbox PROPERTY OUTPUT_NAME test_glsandbox)
    deng_install_bundle_deps (test_glsandbox
        Deng::libcore Deng::libgui Deng::libappfw Deng::libshell
    )
    deng_install_deployqt (test_glsandbox)
endif ()
