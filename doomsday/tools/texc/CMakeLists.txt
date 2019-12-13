# Doomsday Engine - Texture Compiler

cmake_minimum_required (VERSION 3.1)
project (DE_TEXC)
include (../../cmake/Config.cmake)

add_executable (texc import.cpp texc.cpp texc.h)
set_property (TARGET texc PROPERTY FOLDER Tools)

set (DOC_DIR ${DE_INSTALL_DOC_DIR}/texc)
deng_install_tool (texc)
install (FILES doc/texc.txt DESTINATION ${DOC_DIR})
install (DIRECTORY examples DESTINATION ${DOC_DIR})
