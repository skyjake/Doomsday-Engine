find_package (DengCore REQUIRED)    

# Deng::liblegacy may exist in the current build, in which case using 
# a previously installed version is inappropriate.
if (NOT TARGET Deng::liblegacy)
    include ("${DENG_SDK_DIR}/lib/cmake/DengLegacy/DengLegacy.cmake")
endif ()
