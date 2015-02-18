find_package (DengCore REQUIRED)
find_package (Qt5Gui REQUIRED)
find_package (Qt5OpenGL REQUIRED)

include ("${CMAKE_CURRENT_LIST_DIR}/DengGui.cmake")

list (APPEND DENG_REQUIRED_PACKAGES net.dengine.stdlib.gui)
