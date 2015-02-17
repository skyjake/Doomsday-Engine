find_package (ZLIB)
find_package (Qt5Core)
find_package (Qt5Network)

include ("${CMAKE_CURRENT_LIST_DIR}/DengCore.cmake")

list (APPEND DENG_REQUIRED_PACKAGES net.dengine.stdlib)
