find_package (DengCore REQUIRED)
find_package (Qt5Gui REQUIRED)
find_package (Qt5OpenGL REQUIRED)
find_package (Assimp REQUIRED)

include ("${CMAKE_CURRENT_LIST_DIR}/DengGui.cmake")

list (APPEND DENG_REQUIRED_PACKAGES net.dengine.stdlib.gui)
