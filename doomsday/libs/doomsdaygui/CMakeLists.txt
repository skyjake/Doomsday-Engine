# Doomsday Engine -- libdoomsdaygui : common libdoomsday-dependent GUI stuff (static lib)

cmake_minimum_required (VERSION 3.1)
include (../../cmake/Config.cmake)

set (SOURCES
    src/mapoutlinewidget.cpp
)

add_library (doomsdaygui STATIC ${SOURCES})
set_property (TARGET doomsdaygui PROPERTY FOLDER Libraries)
deng_target_defaults (doomsdaygui)
target_include_directories (doomsdaygui PUBLIC include)
deng_link_libraries (doomsdaygui PUBLIC DengGui DengDoomsday)
target_link_sdl2 (doomsdaygui PUBLIC)
