find_package (DengCore REQUIRED)
find_package (DengLegacy REQUIRED)
find_package (DengShell REQUIRED)

include ("${CMAKE_CURRENT_LIST_DIR}/DengDoomsday.cmake")

list (APPEND DENG_REQUIRED_PACKAGES net.dengine.base)
