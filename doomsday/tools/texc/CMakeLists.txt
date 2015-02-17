# Doomsday Engine - Texture Compiler

cmake_minimum_required (VERSION 3.0)
project (DENG_TEXC)
include (../../cmake/Config.cmake)

add_executable (texc import.cpp texc.cpp texc.h)
set_property (TARGET texc PROPERTY FOLDER Tools)

set (DOC_DIR ${DENG_INSTALL_DOC_DIR}/texc)
install (TARGETS texc DESTINATION bin)
install (FILES doc/texc.txt DESTINATION ${DOC_DIR})
install (DIRECTORY examples DESTINATION ${DOC_DIR})
