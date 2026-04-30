cmake_minimum_required (VERSION 3.10)
project (DE_TEST_APPFW)
include (../TestConfig.cmake)

list (APPEND DE_REQUIRED_PACKAGES net.dengine.stdlib net.dengine.stdlib.gui)
deng_add_package (net.dengine.test.appfw)

file (GLOB SOURCES src/*.cpp src/*.h)
deng_add_application (test_appfw ${SOURCES})

target_link_sdl3 (test_appfw PUBLIC)
deng_link_libraries (test_appfw PRIVATE DengGui)

if (APPLE)
    set_property (TARGET test_appfw PROPERTY OUTPUT_NAME test_appfw)
    find_package (the_Foundation REQUIRED)
    find_package (glbinding REQUIRED MODULE)
    deng_install_bundle_deps (test_appfw
        Deng::libcore
        Deng::libgui
        the_Foundation::the_Foundation
        glbinding::glbinding
        assimp::assimp
    )
    deng_bundle_sdl3 (test_appfw)
endif ()

deng_deploy_target (test_appfw)
