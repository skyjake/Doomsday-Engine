AC_INIT(deng, [1.8.0-rc1], [jaakko.keranen@iki.fi])
AM_INIT_AUTOMAKE(deng, [1.8.0-rc1])

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

dnl TODO: Do more tests to see if these are available:
dnl - OpenGL
dnl - GLU
dnl - SDL_net
dnl - SDL_mixer (optional)
dnl - OpenAL (optional)

AC_OUTPUT([Makefile Src/Makefile Src/drOpenGL/Makefile
	Src/dsSDLMixer/Makefile Src/dsOpenAL/Makefile
	Src/jDoom/Makefile Src/jHeretic/Makefile Src/jHexen/Makefile
	Src/dpDehRead/Makefile])
