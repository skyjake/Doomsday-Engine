lib_LTLIBRARIES = libdssdlmixer.la

INCLUDES = -I$(top_srcdir)/Include
DEFS = -DUNIX -DNO_FIXED_ASM

libdssdlmixer_la_CFLAGS = @SDL_CFLAGS@
libdssdlmixer_la_LDFLAGS = -module @SDL_LIBS@ -lSDL_mixer

libdssdlmixer_la_SOURCES = driver_sdlmixer.c
