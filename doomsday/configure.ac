AC_INIT(deng, [1.8.1], [jaakko.keranen@iki.fi])
AM_INIT_AUTOMAKE(deng, [1.8.1])

AM_CONFIG_HEADER(config.h)

dnl Enable building of the convenience library
dnl and set LIBLTDL accordingly
AC_LIBLTDL_CONVENIENCE

dnl Substitute LTDLINCL and LIBLTDL in the Makefiles
AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)

dnl Check for dlopen support
AC_LIBTOOL_DLOPEN

dnl Configure libtool
AC_PROG_LIBTOOL

dnl Configure libltdl
AC_CONFIG_SUBDIRS(libltdl)


dnl These are pretty lax warnings.
CFLAGS="-Wunused-variable -Wunused-function $CFLAGS"

AC_PROG_CC

AC_LANG(C)

dnl Check for SDL (using C).
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION,:,
	AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!]))

dnl Check for XFree86-VidModeExtension.
AC_CHECK_HEADERS([X11/Xlib.h])
AC_CHECK_HEADERS([X11/extensions/xf86vmode.h],
	[LIBS="$LIBS -L/usr/X11R6/lib -lXxf86vm"], [],
	[#if HAVE_X11_XLIB_H
	#  include <X11/Xlib.h>
	#endif
	])

dnl Check that the required libraries are available.
AC_CHECK_HEADER(curses.h,
	[USELESS_VAR=1],
	AC_MSG_ERROR([*** curses not found!]))

AC_CHECK_HEADER(SDL/SDL_net.h,
	[USELESS_VAR=1],
	AC_MSG_ERROR([*** SDL_net not found!]))

OPTIONAL_SUBDIRS=""

AC_DEFUN(DENG_USE_OPENGL,[
	AC_CHECK_HEADER(GL/gl.h,
		[OPTIONAL_SUBDIRS="drOpenGL $OPTIONAL_SUBDIRS"],
		[AC_MSG_WARN([OpenGL is not available, skipping drOpenGL])])

	AC_CHECK_HEADER(GL/glu.h,
		[HAVE_GLU=1],
		[AC_MSG_ERROR([*** GLU not found!])])
])

dnl Optional libraries.
AC_ARG_WITH(opengl,
	[AS_HELP_STRING([--without-opengl],
		[don't use OpenGL (default=yes)])],
	[if test $withval = yes;
		then DENG_USE_OPENGL()
	fi],
	[DENG_USE_OPENGL()])

AC_CHECK_HEADER(SDL/SDL_mixer.h,
	[OPTIONAL_SUBDIRS="dsSDLMixer $OPTIONAL_SUBDIRS"],
	[AC_MSG_WARN([SDL_mixer is not available, skipping dsSDLMixer])])

AC_CHECK_HEADER(AL/al.h,
	[OPTIONAL_SUBDIRS="dsOpenAL $OPTIONAL_SUBDIRS"],
	[AC_MSG_WARN([openal is not available, skipping dsOpenAL])])

AC_SUBST(OPTIONAL_SUBDIRS)

AC_CONFIG_FILES([ Makefile Src/Makefile Src/drOpenGL/Makefile
	Src/jDoom/Makefile Src/jHeretic/Makefile Src/jHexen/Makefile
	Src/dpDehRead/Makefile Src/dpMapLoad/Makefile
	Src/dsSDLMixer/Makefile	Src/dsOpenAL/Makefile ])

AC_OUTPUT
