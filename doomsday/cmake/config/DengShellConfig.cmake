find_package (DengCore REQUIRED)

# Deng::libshell may exist in the current build, in which case using 
# a previously installed version is inappropriate.
if (NOT TARGET Deng::libshell)
    include ("${DENG_SDK_DIR}/lib/cmake/DengShell/DengShell.cmake")
endif ()
