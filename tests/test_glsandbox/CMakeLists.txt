cmake_minimum_required (VERSION 3.10)
project (DE_TEST_GLSANDBOX)
include (../TestConfig.cmake)

find_package (SDL3Libs)

list (APPEND DE_REQUIRED_PACKAGES net.dengine.stdlib)
deng_add_package (net.dengine.test.glsandbox)

deng_add_application (test_glsandbox
    main.cpp
    testwindow.cpp
    testwindow.h
)

target_link_sdl3 (test_glsandbox PUBLIC)
deng_link_libraries (test_glsandbox PRIVATE DengGui)

if (APPLE)
    set_property (TARGET test_glsandbox PROPERTY OUTPUT_NAME test_glsandbox)
    find_package (the_Foundation REQUIRED)
    find_package (glbinding REQUIRED MODULE)
    deng_install_bundle_deps (test_glsandbox
        Deng::libcore
        Deng::libgui
        the_Foundation::the_Foundation
        glbinding::glbinding
        assimp::assimp
    )
    deng_bundle_sdl3 (test_glsandbox)
endif ()

deng_deploy_target (test_glsandbox)
deng_codesign (test_glsandbox)
