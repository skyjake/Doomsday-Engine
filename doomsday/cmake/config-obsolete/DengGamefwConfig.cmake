find_package (DengCore REQUIRED)
find_package (DengLegacy REQUIRED)

# Deng::libgamefw may exist in the current build, in which case using 
# a previously installed version is inappropriate.
if (NOT TARGET Deng::libgamefw)
    include ("$${DENG_SDK_DIR}/lib/cmake/DengGamefw/DengGamefw.cmake")
endif ()
