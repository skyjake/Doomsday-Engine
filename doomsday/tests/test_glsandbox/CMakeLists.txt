cmake_minimum_required (VERSION 3.1)
project (DE_TEST_GLSANDBOX)
include (../TestConfig.cmake)

list (APPEND DE_REQUIRED_PACKAGES net.dengine.stdlib)
deng_add_package (net.dengine.test.glsandbox)

deng_add_application (test_glsandbox
    main.cpp
    testwindow.cpp
    testwindow.h
)

target_link_libraries (test_glsandbox PRIVATE SDL2 SDL2main)
deng_link_libraries (test_glsandbox PRIVATE DengGui)

if (APPLE)
    set_property (TARGET test_glsandbox PROPERTY OUTPUT_NAME test_glsandbox)
    deng_install_bundle_deps (test_glsandbox
        Deng::libcore Deng::libgui Deng::libshell
    )
endif ()
