set (DIR ${CMAKE_CURRENT_LIST_DIR})
include_directories (${DIR}/include)
file (GLOB_RECURSE COMMON_SOURCES ${DIR}/src/* ${DIR}/include/*)
