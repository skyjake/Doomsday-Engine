set (DIR ${CMAKE_CURRENT_LIST_DIR})
include_directories (${DIR}/include)

file (GLOB_RECURSE COMMON_HEADERS ${DIR}/include/[A-Za-z]*)

file (GLOB COMMON_SOURCES ${DIR}/src/[A-Za-z]*)

deng_glob_sources (COMMON_SOURCES ${DIR}/src/hud/*.cpp)
deng_glob_sources (COMMON_SOURCES ${DIR}/src/menu/*.cpp)

deng_merge_sources (common_acs          ${DIR}/src/acs/*.cpp)
deng_merge_sources (common_game         ${DIR}/src/game/*.cpp)
deng_merge_sources (common_hud_widgets  ${DIR}/src/hud/widgets/*.cpp)
deng_merge_sources (common_menu_widgets ${DIR}/src/menu/widgets/*.cpp)
deng_merge_sources (common_network      ${DIR}/src/network/*.cpp)
deng_merge_sources (common_world        ${DIR}/src/world/*.cpp)

list (APPEND COMMON_SOURCES ${COMMON_HEADERS} ${SOURCES})
