# find_package (Qt5 COMPONENTS Gui OpenGL REQUIRED)
find_package (DengCore REQUIRED)

# Deng::libgui may exist in the current build, in which case using 
# a previously installed version is inappropriate.
if (NOT TARGET Deng::libgui)
    include ("${CMAKE_CURRENT_LIST_DIR}/DengGui.cmake")
endif ()

list (APPEND DENG_REQUIRED_PACKAGES net.dengine.stdlib.gui)
