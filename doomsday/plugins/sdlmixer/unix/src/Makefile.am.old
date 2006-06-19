lib_LTLIBRARIES = libdssdlmixer.la

INCLUDES = -I$(top_srcdir)/Include
DEFS = -DUNIX -D@FIXED_IMPL@

libdssdlmixer_la_CFLAGS = @SDL_CFLAGS@
libdssdlmixer_la_LDFLAGS = -module @SDL_LIBS@ -lSDL_mixer

libdssdlmixer_la_SOURCES = driver_sdlmixer.c mus2midi.c mus2midi.h \
	../Common/m_fixed.c driver.h sdl.c
