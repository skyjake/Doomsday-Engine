cmake_minimum_required (VERSION 3.1)
project (DE_TEST_APPFW)
include (../TestConfig.cmake)

find_package (Qt5 COMPONENTS Gui Widgets)
find_package (DengAppfw)

deng_add_package (net.dengine.test.appfw)

file (GLOB SOURCES src/*.cpp src/*.h)
deng_add_application (test_appfw ${SOURCES})

target_link_libraries (test_appfw Deng::libappfw)

if (APPLE)
    set_property (TARGET test_appfw PROPERTY OUTPUT_NAME test_appfw)
    deng_install_bundle_deps (test_appfw
        Deng::libcore Deng::libgui Deng::libappfw Deng::libshell
    )
    deng_install_deployqt (test_appfw)
endif ()
