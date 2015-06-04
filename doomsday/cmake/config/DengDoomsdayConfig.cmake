find_package (DengCore REQUIRED)
find_package (DengLegacy REQUIRED)
find_package (DengShell REQUIRED)

if (NOT TARGET Deng::libdoomsday)
    include ("${CMAKE_CURRENT_LIST_DIR}/DengDoomsday.cmake")
endif ()

list (APPEND DENG_REQUIRED_PACKAGES net.dengine.base)
