## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################


################################################################################
######################## External LZSS Files ###################################

INCLUDE_DIRECTORIES ( external/lzss/portable/include )
FILE(GLOB LZSS_SOURCES external/lzss/unix/src/*.c)

################################################################################
######################## jHexen Game Plugin  ####################################

ADD_DEFINITIONS (-D__JHEXEN__ )

INCLUDE_DIRECTORIES ( include )

FILE(GLOB JHEXEN_SOURCES src/*.c)

ADD_LIBRARY (jhexen SHARED ${JHEXEN_SOURCES} ${LZSS_SOURCES})
