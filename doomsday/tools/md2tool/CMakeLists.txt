# Doomsday Engine - MD2 Tool

cmake_minimum_required (VERSION 3.1)
project (DE_MD2TOOL)
include (../../cmake/Config.cmake)

add_executable (md2tool md2tool.c md2tool.h anorms.h)
set_property (TARGET md2tool PROPERTY FOLDER Tools)

if (UNIX)
    target_link_libraries (md2tool PUBLIC m)
endif ()

deng_install_tool (md2tool)
