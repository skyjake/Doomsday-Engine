lib_LTLIBRARIES = libdsopenal.la

INCLUDES = -I$(top_srcdir)/Include
DEFS = -DUNIX -D@FIXED_IMPL@

libdsopenal_la_CFLAGS = @SDL_CFLAGS@
libdsopenal_la_LDFLAGS = -module @SDL_LIBS@ -lopenal

libdsopenal_la_SOURCES = driver_openal.c
