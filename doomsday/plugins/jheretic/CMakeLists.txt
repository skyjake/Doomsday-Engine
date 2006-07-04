## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################


################################################################################
######################## jHeretic Game Plugin  ####################################

ADD_DEFINITIONS (-D__JHERETIC__ )

INCLUDE_DIRECTORIES ( include )

FILE(GLOB JHERETIC_SOURCES src/*.c)

ADD_LIBRARY (jheretic SHARED ${JHERETIC_SOURCES})
