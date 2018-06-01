find_package (DengCore REQUIRED)
find_package (DengLegacy REQUIRED)
find_package (DengShell REQUIRED)

if (NOT TARGET Deng::libdoomsday)
    include ("${DENG_SDK_DIR}/lib/cmake/DengDoomsday/DengDoomsday.cmake")
endif ()

list (APPEND DE_REQUIRED_PACKAGES net.dengine.base)
