cmake_minimum_required (VERSION 3.0)
project (DENG_SAVEGAMECONVERTER)
include (../PluginConfig.cmake)

include_directories (include)
file (GLOB SOURCES src/*.cpp include/*.h)

deng_add_plugin (savegameconverter ${SOURCES} api/dpsavegameconverter.def)
