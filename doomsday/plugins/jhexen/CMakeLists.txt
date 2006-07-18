## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/plugins/common/include )
##FILE(GLOB COMMON_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/plugins/common/src/*.c)
SET (COMMON_PLUGIN_SOURCES 
${CMAKE_SOURCE_DIR}/plugins/common/src/am_map.c
${CMAKE_SOURCE_DIR}/plugins/common/src/d_net.c
${CMAKE_SOURCE_DIR}/plugins/common/src/d_netcl.c
${CMAKE_SOURCE_DIR}/plugins/common/src/d_netsv.c
${CMAKE_SOURCE_DIR}/plugins/common/src/dmu_lib.c
${CMAKE_SOURCE_DIR}/plugins/common/src/f_infine.c
${CMAKE_SOURCE_DIR}/plugins/common/src/g_controls.c
${CMAKE_SOURCE_DIR}/plugins/common/src/g_dglinit.c
${CMAKE_SOURCE_DIR}/plugins/common/src/g_game.c
${CMAKE_SOURCE_DIR}/plugins/common/src/g_update.c
${CMAKE_SOURCE_DIR}/plugins/common/src/hu_lib.c
${CMAKE_SOURCE_DIR}/plugins/common/src/hu_msg.c
${CMAKE_SOURCE_DIR}/plugins/common/src/hu_pspr.c
${CMAKE_SOURCE_DIR}/plugins/common/src/hu_stuff.c
${CMAKE_SOURCE_DIR}/plugins/common/src/m_ctrl.c
${CMAKE_SOURCE_DIR}/plugins/common/src/m_fixed.c
${CMAKE_SOURCE_DIR}/plugins/common/src/m_multi.c
${CMAKE_SOURCE_DIR}/plugins/common/src/mn_menu.c
${CMAKE_SOURCE_DIR}/plugins/common/src/p_actor.c
${CMAKE_SOURCE_DIR}/plugins/common/src/p_mapsetup.c
${CMAKE_SOURCE_DIR}/plugins/common/src/p_player.c
${CMAKE_SOURCE_DIR}/plugins/common/src/p_start.c
${CMAKE_SOURCE_DIR}/plugins/common/src/p_svtexarc.c
${CMAKE_SOURCE_DIR}/plugins/common/src/p_tick.c
${CMAKE_SOURCE_DIR}/plugins/common/src/p_user.c
${CMAKE_SOURCE_DIR}/plugins/common/src/p_view.c
${CMAKE_SOURCE_DIR}/plugins/common/src/r_common.c
${CMAKE_SOURCE_DIR}/plugins/common/src/st_lib.c
${CMAKE_SOURCE_DIR}/plugins/common/src/x_hair.c )

## Currently not used. Including them results in a FTBFS
##${CMAKE_SOURCE_DIR}/plugins/common/src/p_saveg.c
##${CMAKE_SOURCE_DIR}/plugins/common/src/p_xgfile.c
##${CMAKE_SOURCE_DIR}/plugins/common/src/p_xgline.c
##${CMAKE_SOURCE_DIR}/plugins/common/src/p_xgsave.c
##${CMAKE_SOURCE_DIR}/plugins/common/src/p_xgsec.c
################################################################################
######################## External LZSS Files ###################################

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/external/lzss/portable/include )
FILE(GLOB LZSS_SOURCES ${CMAKE_SOURCE_DIR}/external/lzss/unix/src/*.c)

################################################################################
######################## jHexen Game Plugin  ####################################

ADD_DEFINITIONS (-D__JHEXEN__ )

INCLUDE_DIRECTORIES ( include )

FILE(GLOB JHEXEN_SOURCES src/*.c)

ADD_LIBRARY (jhexen SHARED ${JHEXEN_SOURCES} ${LZSS_SOURCES}
${COMMON_PLUGIN_SOURCES} )

################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS jhexen
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR}
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
