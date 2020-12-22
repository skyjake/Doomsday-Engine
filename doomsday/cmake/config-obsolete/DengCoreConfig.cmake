find_package (CPlus REQUIRED)

# Deng::libcore may exist in the current build, in which case using 
# a previously installed version is inappropriate.
# if (NOT TARGET Deng::libcore)
#     include (${DE_SDK_DIR}/lib/cmake/DengCore/DengCore.cmake")
# endif ()

list (APPEND DE_REQUIRED_PACKAGES net.dengine.stdlib)
