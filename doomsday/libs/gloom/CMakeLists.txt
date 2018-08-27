cmake_minimum_required (VERSION 3.1)
project (DE_LIBGLOOM)

include (../../cmake/Config.cmake)

find_package (FMOD)
find_package (Json)

list (APPEND DE_REQUIRED_PACKAGES
    net.dengine.stdlib
    net.dengine.stdlib.gui
    net.dengine.base
)
deng_add_package (net.dengine.gloom)

# Definitions.
add_definitions (-D__LIBGLOOM__=1)

file (GLOB_RECURSE HEADERS include/gloom/[a-zA-Z]*)

deng_merge_sources (libgloom src/*.cpp)
deng_merge_sources (audio    src/audio/*.cpp)
deng_merge_sources (geo      src/geo/*.cpp)
deng_merge_sources (render   src/render/*.cpp)
deng_merge_sources (world    src/world/*.cpp)

# Deploy the FMOD library.
# if (NOT TARGET fmodex)
#     message (FATAL_ERROR "FMOD is required to build libgloom - Set the FMOD_DIR variable")
# endif ()
# if (APPLE)
#     get_property (fmodLibPath TARGET fmodex PROPERTY INTERFACE_LINK_LIBRARIES)
#     deng_find_resources (${fmodLibPath},Frameworks)
# endif ()

deng_add_library (libgloom ${SOURCES} ${HEADERS})

deng_link_libraries (libgloom PUBLIC DengAppfw DengDoomsday)
target_link_libraries (libgloom PRIVATE SDL2 fmodex json)
deng_deploy_library (libgloom DengGloom)

# if (APPLE)
#     set_target_properties (gloom PROPERTIES
#         OUTPUT_NAME gloom
#         MACOSX_BUNDLE_INFO_PLIST ${DE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
#      )
#     deng_install_bundle_deps (gloom
#         Deng::libcore
#         Deng::libgui
#         Deng::libappfw
#         Deng::libshell
#         Deng::liblegacy
#         Deng::libdoomsday
#         the_Foundation::the_Foundation
#         fmodex
#     )
#     deng_install_deployqt (gloom)
# endif ()
