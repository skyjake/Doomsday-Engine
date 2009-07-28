# The Doomsday Engine Project -- libdeng2
#
# Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

CMAKE_MINIMUM_REQUIRED ( VERSION 2.6 )

# Version numbers.
# Note: This is the place for all version numbers.
SET ( LIBDENG2_RELEASE_LABEL    Dev )
SET ( LIBDENG2_MAJOR_VERSION    2 )
SET ( LIBDENG2_MINOR_VERSION    0 )
SET ( LIBDENG2_PATCHLEVEL       0 )

SET ( CMAKE_INSTALL_PREFIX ./install CACHE PATH "Path where to install for packaging." )

PROJECT ( deng )

# Check for an in-tree build.
IF ( ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR} )
    MESSAGE ( FATAL_ERROR 
        "In-tree builds are not supported. For instance, try building in ${CMAKE_SOURCE_DIR}/mybuild/." )
ENDIF ()

# General configuration.
SET ( DENG_BINARY_DIR bin )
SET ( DENG_DATA_DIR data )

# Win32 configuration.
IF ( WIN32 )
    INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/external/opengl )
    SET ( CMAKE_INCLUDE_PATH 
        ${CMAKE_SOURCE_DIR}/external/libcurl/include 
        ${CMAKE_SOURCE_DIR}/external/zlib/include
        ${CMAKE_SOURCE_DIR}/external/libpng/include )
    SET ( CMAKE_LIBRARY_PATH 
        ${CMAKE_SOURCE_DIR}/external/libcurl/lib 
        ${CMAKE_SOURCE_DIR}/external/zlib/lib
        ${CMAKE_SOURCE_DIR}/external/libpng/lib )
    SET ( PNG_NAMES libpng13 )

    IF ( "$ENV{SDLDIR}" STREQUAL "" )
        MESSAGE ( FATAL_ERROR
            "You have not set the SDK environment variables. See build\\win32\\dengenv.bat." )
    ENDIF ()
    
    # Take care of the slash issue.
    FILE ( TO_CMAKE_PATH $ENV{SDLDIR} SDLDIR )
    FILE ( TO_CMAKE_PATH $ENV{SDLMIXERDIR} SDLMIXERDIR )
    FILE ( TO_CMAKE_PATH $ENV{SDLNETDIR} SDLNETDIR )
    
    # Where are the DirectX libs?
    FILE ( TO_CMAKE_PATH $ENV{DXDIR}/lib/$ENV{PROCESSOR_ARCHITECTURE} DXLIBDIR )
    IF ( NOT EXISTS $ENV{DXDIR}/lib/$ENV{PROCESSOR_ARCHITECTURE} )
        FILE ( TO_CMAKE_PATH $ENV{DXDIR}/lib DXLIBDIR )
    ENDIF ()
    
    MESSAGE ( STATUS "DirectX libraries: ${DXLIBDIR}" )
ENDIF ( WIN32 )

# Apple configuration.
IF ( APPLE )
    # Install into a bundle directory.
    SET ( DENG_BUNDLE_DIR Doomsday.app/Contents )
    SET ( DENG_BINARY_DIR ${DENG_BUNDLE_DIR}/MacOS )
    SET ( DENG_DATA_DIR ${DENG_BUNDLE_DIR}/Resources )
ENDIF ( APPLE )

# Dependencies.
FIND_PACKAGE ( OpenGL REQUIRED )
FIND_PACKAGE ( ZLIB REQUIRED )
FIND_PACKAGE ( PNG REQUIRED )
FIND_PACKAGE ( CURL REQUIRED )
FIND_PACKAGE ( SDL REQUIRED )
FIND_PACKAGE ( SDL_mixer REQUIRED )
FIND_PACKAGE ( SDL_image REQUIRED )
FIND_PACKAGE ( SDL_net REQUIRED )
FIND_PACKAGE ( SDL_ttf REQUIRED )
FIND_PACKAGE ( PythonInterp REQUIRED )
FIND_PACKAGE ( Doxygen )

# Set up project-wide variables and definitions.
IF ( CMAKE_BUILD_TYPE STREQUAL Debug )
    ADD_DEFINITIONS ( -D_DEBUG -DENABLERANGECHECKING )
ENDIF ()
IF ( CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX )
    SET ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall" )
    SET ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall" )
ENDIF ()
IF ( WIN32 )
    ADD_DEFINITIONS ( -DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE )
    
    SET ( APP_SUPPORT_SOURCES "" )
ENDIF ( WIN32 )
IF ( UNIX )
     # Note that this applies to APPLE as well.
    FIND_PACKAGE ( Curses REQUIRED )

    ADD_DEFINITIONS ( -DUNIX -DNO_FIXED_ASM )

    SET ( APP_SUPPORT_SOURCES "" )
ENDIF ( UNIX )
IF ( APPLE )
    FIND_PACKAGE ( QuickTime REQUIRED )

    ADD_DEFINITIONS ( -DMACOSX )
    IF ( ${CMAKE_OSX_SYSROOT} STREQUAL /Developer/SDKs/MacOSX10.4u.sdk )
	    SET ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=10.4" )
	    SET ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.4" )
	ENDIF ()

    SET ( APP_SUPPORT_SOURCES ${CMAKE_SOURCE_DIR}/support/mac/sdlmain.m )
ENDIF ( APPLE )

# Package the resources into PK3 files.
ADD_CUSTOM_TARGET ( packres 
    ${PYTHON_EXECUTABLE} ./packres.py ${CMAKE_CURRENT_BINARY_DIR} 
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build/scripts
    SOURCES ${CMAKE_SOURCE_DIR}/build/scripts/packres.py )

# Installation.
INSTALL ( CODE "EXECUTE_PROCESS ( 
    COMMAND ${PYTHON_EXECUTABLE} ./packres.py ${CMAKE_CURRENT_BINARY_DIR} 
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build/scripts )" )

INSTALL ( FILES 
    ${CMAKE_CURRENT_BINARY_DIR}/doomsday.pk3 
    ${CMAKE_CURRENT_BINARY_DIR}/doom.pk3 
    ${CMAKE_CURRENT_BINARY_DIR}/heretic.pk3 
    ${CMAKE_CURRENT_BINARY_DIR}/hexen.pk3 
    DESTINATION ${DENG_DATA_DIR} )
    
# Win32-specific installation.
IF ( WIN32 )
    # Install DLLs to the binary directory.
    INSTALL ( FILES 
        ${SDLDIR}/lib/SDL.dll
        ${SDLMIXERDIR}/lib/SDL_mixer.dll
        ${SDLNETDIR}/lib/SDL_net.dll
        external/libpng/lib/libpng13.dll 
        external/zlib/lib/zlib1.dll
        external/lzss/win32/lzss.dll
        DESTINATION ${DENG_BINARY_DIR} )
ENDIF ( WIN32 )
    
# Install OS X specific files.
IF ( APPLE )
    INSTALL ( FILES 
        build/mac/Info.plist
        build/mac/PkgInfo
        DESTINATION ${DENG_BUNDLE_DIR} )

    INSTALL ( FILES
        build/mac/deng.icns 
        DESTINATION ${DENG_DATA_DIR} )

    INSTALL ( DIRECTORY
        build/mac/English.lproj
        DESTINATION ${DENG_DATA_DIR} 
        PATTERN ".svn" EXCLUDE )

    # Frameworks.
    # Clean the framework path.
    STRING ( REGEX REPLACE "([^;]*);.*" "\\1" SDL_LIBRARY_CLEAN "${SDL_LIBRARY}" )
    INSTALL ( DIRECTORY 
        ${SDL_LIBRARY_CLEAN}
        DESTINATION ${DENG_BUNDLE_DIR}/Frameworks 
        PATTERN "Headers" EXCLUDE
        PATTERN ".DS_Store" EXCLUDE )
    INSTALL ( DIRECTORY 
        ${SDLMIXER_LIBRARY}
        DESTINATION ${DENG_BUNDLE_DIR}/Frameworks 
        PATTERN "Headers" EXCLUDE
        PATTERN ".DS_Store" EXCLUDE )
    INSTALL ( DIRECTORY 
        ${SDLNET_LIBRARY}
        DESTINATION ${DENG_BUNDLE_DIR}/Frameworks 
        PATTERN "Headers" EXCLUDE
        PATTERN ".DS_Store" EXCLUDE )
ENDIF ( APPLE )

# Documentation.
IF ( DOXYGEN_FOUND )
    ADD_CUSTOM_TARGET ( alldocs
        ${DOXYGEN_EXECUTABLE} 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
ENDIF ( DOXYGEN_FOUND )

# The real work is done in the subdirectories.
SUBDIRS ( libdeng libdeng2 server client plugins tests )
