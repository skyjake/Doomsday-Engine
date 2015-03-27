# find_package (Qt5 COMPONENTS Core Network REQUIRED)

# Deng::libcore may exist in the current build, in which case using 
# a previously installed version is inappropriate.
if (NOT TARGET Deng::libcore)
    include ("${CMAKE_CURRENT_LIST_DIR}/DengCore.cmake")
endif ()

list (APPEND DENG_REQUIRED_PACKAGES net.dengine.stdlib)
