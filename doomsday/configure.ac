#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

################################################################################
## Configuration file for deng
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
##
################################################################################
define([DENG_VERSION],[1.9.0beta5])

################################################################################
## Autoconf initialisation.
################################################################################
AC_INIT(deng, [DENG_VERSION], [skyjake@users.sourceforge.net])
AC_CONFIG_SRCDIR([./engine/api/dd_api.h])
AC_CANONICAL_SYSTEM

################################################################################
## Automake Initialisation.
################################################################################
AM_INIT_AUTOMAKE([-Wall foreign])
AC_CONFIG_HEADER([config.h])

################################################################################
## Build Flags. (If needed)
################################################################################
CFLAGS="$CFLAGS"


################################################################################
################################################################################
## Build Options Start Here.
################################################################################
################################################################################

################################################################################
## Disable Building of jDoom, jHeretic or jHexen if requested.
################################################################################
##OPTIONAL_SUBDIRS=""
##
##AC_DEFUN([DENG_OPTIONAL],[OPTIONAL_SUBDIRS="$OPTIONAL_SUBDIRS $1"])
##
##buildjdoom="yes"
##AC_ARG_WITH(jdoom,
##	[AC_HELP_STRING([--without-jdoom], [don't build jDoom])],
##	[if test $withval = yes;
##		then DENG_OPTIONAL(jDoom)
##		$buildjdoom="no"
##	fi],
##	[DENG_OPTIONAL(jDoom)])
##
##
##buildjheretic="yes"
##AC_ARG_WITH(jheretic,
##	[AC_HELP_STRING([--without-jheretic], [don't build jHeretic])],
##	[if test $withval = yes;
##		then DENG_OPTIONAL(jHeretic)
##		buildjheretic="no"
##	fi],
##	[DENG_OPTIONAL(jHeretic)])
##
##buildjhexen="yes"
##AC_ARG_WITH(jhexen,
##	[AC_HELP_STRING([--without-jhexen], [don't build jHexen])],
##	[if test $withval = yes;
##		then DENG_OPTIONAL(jHexen)
##		buildjhexen="no"
##	fi],
##	[DENG_OPTIONAL(jHexen)])
##
##
##AC_SUBST(OPTIONAL_SUBDIRS)

################################################################################
## Use fixed x86 assembler if the TARGET is x86.
## Otherwise cross-compiles may fail
################################################################################

### Check architecture and choose a fixed-point math implementation.
##case $target_cpu in
##  i*86)
##    FIXED_IMPL="GNU_X86_FIXED_ASM"
##    fixedasm="yes"
##    break ;;
##  *)
##    FIXED_IMPL="NO_FIXED_ASM"
##    fixedasm="no"
##    break ;;
##esac
##AC_SUBST(FIXED_IMPL)

################################################################################
################################################################################
## Build Options End Here.
################################################################################
################################################################################

################################################################################
## Compiler checks.
################################################################################
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

################################################################################
## Libtool initialisation.
################################################################################
AC_LIBLTDL_CONVENIENCE
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)
AC_CONFIG_SUBDIRS(libltdl)

################################################################################
## Required libraries.
################################################################################
# Check for OpenGL.
AC_CHECK_LIB([GL], [glGetIntegerv],,
[AC_MSG_ERROR("*** libGL not found. On Ubuntu install libgl1-mesa-dev.")])

# Check for GLU
AC_CHECK_LIB([GLU], [gluBuild2DMipmaps],,
[AC_MSG_ERROR("*** libGLU not found. On Ubuntu install libglu1-mesa-dev.")])

# Check for SDL 1.2.0 or greater
AC_CHECK_LIB([SDL], [SDL_Init],,
[AC_MSG_ERROR("*** SDL not found. On Ubuntu install libsdl1.2-dev.")])
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION,:,
AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!]))

# Check for SDL Mixer.
AC_CHECK_LIB([SDL_mixer], [Mix_OpenAudio],,
[AC_MSG_ERROR("*** SDL Mixer not found. On Ubuntu install libsdl-mixer1.2-dev.")])

# Check for SDL Net
AC_CHECK_LIB([SDL_net], [SDLNet_UDP_Send],,
[AC_MSG_ERROR("*** SDL Net not found. On Ubuntu install libsdl-net1.2-dev.")])

# Needed on *NIX
AC_CHECK_LIB([X11], [XOpenDisplay],,
[AC_MSG_ERROR("*** X11 not found. On Ubuntu install libx11-dev.")])

# What are we using that needs this ?
# FIXME: Replace `main' with a function in `-lXext':
AC_CHECK_LIB([Xext], [main])

# FIXME: Replace `main' with a function in `-lncurses':
AC_CHECK_LIB([ncurses], [main])

# Check for OpenAL
AC_CHECK_LIB([openal], [alSourcePlay],,
[AC_MSG_ERROR("*** OpenAL not found. On Ubuntu install libopenal-dev.")])

# Check for libPNG
AC_CHECK_LIB([png], [png_create_read_struct],,
[AC_MSG_ERROR("*** libPNG not found. On Ubuntu install libpng-dev.")])

# FIXME: Replace `main' with a function in `-lz':
AC_CHECK_LIB([z], [main])

################################################################################
## Required Headers
################################################################################
AC_HEADER_DIRENT
AC_HEADER_STDC

AC_CHECK_HEADER([SDL/SDL.h],
		[],
		[AC_MSG_ERROR([*** SDL include files not found!
You should install the SDL development package.])])

AC_CHECK_HEADER([SDL/SDL_opengl.h],
		[],
		[AC_MSG_ERROR([*** SDL_opengl include files not found!
You should install the SDL development package.])])

AC_CHECK_HEADER([SDL/SDL_mixer.h],
		[],
		[AC_MSG_ERROR([*** SDL_mixer include files not found!
You should install the SDL Mixer development package.])])

AC_CHECK_HEADER([SDL/SDL_net.h],
		[],
		[AC_MSG_ERROR([*** SDL_net include files not found!
You should install the SDL Net development package.])])



AC_CHECK_HEADERS([argz.h arpa/inet.h fcntl.h limits.h malloc.h netdb.h netinet/in.h stddef.h stdlib.h string.h strings.h sys/socket.h unistd.h])

################################################################################
## Required typedefs, structures, and compiler characteristics.
################################################################################
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_C_VOLATILE
AC_CHECK_TYPES([ptrdiff_t])

################################################################################
# Check for OGG support in SDL_mixer                                           
################################################################################

if test -e "build/unix/tests/doom_intro.ogg" ; then
        AC_LANG([C])
	AC_MSG_CHECKING([for OGG support in SDL_mixer])

        CFLAGS="$CFLAGS $SDL_CFLAGS"
	LIBS="$LIBS $SDL_LIBS $SDL_MIXER_LIBS"

	ac_link="$LDPREFIX $ac_link"
        AC_RUN_IFELSE([AC_LANG_SOURCE([
	#include <SDL_mixer.h>
        #include <stdlib.h>

	int main(int argc, char **argv)
        {
	    Mix_Music* music = Mix_LoadMUS("build/unix/tests/doom_intro.ogg");
	    if (music == NULL)
		exit(1);
	    exit(0);
        }
	])],
	[AC_MSG_RESULT(yes)],
	[AC_MSG_RESULT(no)]
	[AC_MSG_ERROR([*** SDL_mixer has no OGG support! You need SDL_mixer with OGG support])],
	[AC_MSG_RESULT([not tested in cross-compiling])])
fi

################################################################################
# Check for MP3 support in SDL_mixer                                           
################################################################################

if test -e "build/unix/tests/doom_intro.mp3" ; then
        AC_LANG([C])
	AC_MSG_CHECKING([for MP3 support in SDL_mixer])

        CFLAGS="$CFLAGS $SDL_CFLAGS"
	LIBS="$LIBS $SDL_LIBS $SDL_MIXER_LIBS"

	ac_link="$LDPREFIX $ac_link"
        AC_RUN_IFELSE([AC_LANG_SOURCE([
	#include <SDL_mixer.h>
        #include <stdlib.h>

	int main(int argc, char **argv)
        {
	    Mix_Music* music = Mix_LoadMUS("build/unix/tests/doom_intro.mp3");
	    if (music == NULL)
		exit(1);
	    exit(0);
        }
	])],
	[AC_MSG_RESULT(yes)],
	[AC_MSG_RESULT(no)]
	[AC_MSG_ERROR([*** SDL_mixer has no MP3 support! You need SDL_mixer with MP3 support])],
	[AC_MSG_RESULT([not tested in cross-compiling])])
fi

################################################################################
## Required library functions.
################################################################################
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([floor getcwd gethostbyaddr gethostbyname memmove memset mkdir pow socket sqrt strcasecmp strchr strdup strerror strncasecmp strrchr strstr strtol strtoul])

################################################################################
## Outputs.
################################################################################
AC_CONFIG_FILES([Makefile
		engine/portable/src/Makefile
		plugins/common/src/Makefile
		plugins/dehread/src/Makefile
		plugins/jdoom/src/Makefile
		plugins/jheretic/src/Makefile
		plugins/jhexen/src/Makefile
		plugins/mapload/src/Makefile
		plugins/openal/src/Makefile
		plugins/sdlmixer/portable/src/Makefile
		plugins/opengl/portable/src/Makefile
		plugins/opengl/unix/src/Makefile
		external/lzss/unix/src/Makefile
		external/glbsp/src/Makefile])
AC_OUTPUT
