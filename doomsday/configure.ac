AC_INIT(deng, [1.9.0-beta4], [jaakko.keranen@iki.fi])
AM_INIT_AUTOMAKE(deng, [1.9.0-beta4])

AC_CANONICAL_BUILD

AM_CONFIG_HEADER(config.h)

dnl Check architecture and choose a fixed-point math implementation.
case $build_cpu in
  i*86)
    FIXED_IMPL="GNU_X86_FIXED_ASM"
    break ;;
  *)
    FIXED_IMPL="NO_FIXED_ASM"
    break ;;
esac
AC_SUBST(FIXED_IMPL)


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
dnl AC_CHECK_HEADERS([X11/Xlib.h])
dnl AC_CHECK_HEADERS([X11/extensions/xf86vmode.h],
dnl	[LIBS="$LIBS -L/usr/X11R6/lib -lXxf86vm"], [],
dnl	[#if HAVE_X11_XLIB_H
dnl	#  include <X11/Xlib.h>
dnl	#endif
dnl	])

dnl Check that the required libraries are available.
AC_CHECK_HEADER(curses.h,
	[USELESS_VAR=1],
	AC_MSG_ERROR([*** curses not found!]))

AC_CHECK_HEADER(SDL/SDL_net.h,
	[USELESS_VAR=1],
	AC_MSG_ERROR([*** SDL_net not found!]))

OPTIONAL_SUBDIRS=""

AC_DEFUN(DENG_OPTIONAL,[OPTIONAL_SUBDIRS="$OPTIONAL_SUBDIRS $1"])

AC_DEFUN(DENG_USE_OPENGL,[
	AC_CHECK_HEADER(GL/gl.h,
		[DENG_OPTIONAL(drOpenGL)],
		[AC_MSG_WARN([OpenGL is not available, skipping drOpenGL])])

	AC_CHECK_HEADER(GL/glu.h,
		[HAVE_GLU=1],
		[AC_MSG_ERROR([*** GLU not found!])])
])

dnl Optional libraries.
AC_ARG_WITH(opengl,
	[AC_HELP_STRING([--without-opengl],
		[don't use OpenGL (default=yes)])],
	[if test $withval = yes;
		then DENG_USE_OPENGL()
	fi],
	[DENG_USE_OPENGL()])

AC_ARG_WITH(jdoom,
	[AC_HELP_STRING([--without-jdoom], [don't build jDoom])],
	[if test $withval = yes;
		then DENG_OPTIONAL(jDoom)
	fi],
	[DENG_OPTIONAL(jDoom)])

AC_ARG_WITH(jheretic,
	[AC_HELP_STRING([--without-jheretic], [don't build jHeretic])],
	[if test $withval = yes;
		then DENG_OPTIONAL(jHeretic)
	fi],
	[DENG_OPTIONAL(jHeretic)])

AC_ARG_WITH(jhexen,
	[AC_HELP_STRING([--without-jhexen], [don't build jHexen])],
	[if test $withval = yes;
		then DENG_OPTIONAL(jHexen)
	fi],
	[DENG_OPTIONAL(jHexen)])

AC_CHECK_HEADER(SDL/SDL_mixer.h,
	[DENG_OPTIONAL(dsSDLMixer)],
	[AC_MSG_WARN([SDL_mixer is not available, skipping dsSDLMixer])])

AC_CHECK_HEADER(AL/al.h,
	[DENG_OPTIONAL(dsOpenAL)],
	[AC_MSG_WARN([openal is not available, skipping dsOpenAL])])

AC_SUBST(OPTIONAL_SUBDIRS)

AC_CONFIG_FILES([ Makefile Src/Makefile Src/drOpenGL/Makefile
	Src/jDoom/Makefile Src/jHeretic/Makefile Src/jHexen/Makefile
	Src/dpDehRead/Makefile Src/dpMapLoad/Makefile
	Src/dsSDLMixer/Makefile	Src/dsOpenAL/Makefile ])

AC_OUTPUT
