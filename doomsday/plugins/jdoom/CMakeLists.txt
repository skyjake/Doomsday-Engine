## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################


################################################################################
######################## jDoom Game Plugin  ####################################

ADD_DEFINITIONS (-D__JDOOM__ )

INCLUDE_DIRECTORIES ( include )

FILE(GLOB JDOOM_SOURCES src/*.c)

ADD_LIBRARY (jdoom SHARED ${JDOOM_SOURCES})
