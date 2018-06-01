# find_package (Qt5 COMPONENTS Core Network REQUIRED)

# Deng::libcore may exist in the current build, in which case using 
# a previously installed version is inappropriate.
if (NOT TARGET Deng::libcore)
    include ("${DENG_SDK_DIR}/lib/cmake/DengCore/DengCore.cmake")
endif ()

list (APPEND DE_REQUIRED_PACKAGES net.dengine.stdlib)
