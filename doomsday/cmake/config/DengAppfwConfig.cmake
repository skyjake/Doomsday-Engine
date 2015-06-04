find_package (Qt5Widgets REQUIRED)
find_package (DengCore REQUIRED)
find_package (DengGui REQUIRED)
find_package (DengShell REQUIRED)

# Deng::libappfw may exist in the current build, in which case using 
# a previously installed version is inappropriate.
if (NOT TARGET Deng::libappfw)
    include ("${CMAKE_CURRENT_LIST_DIR}/DengAppfw.cmake")
endif ()
