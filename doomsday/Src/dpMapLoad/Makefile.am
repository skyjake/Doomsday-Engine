lib_LTLIBRARIES = libdpmapload.la

DEFS = -DUNIX -DNO_FIXED_ASM -DGLBSP_PLUGIN
INCLUDES = -I$(top_srcdir)/Include
libdpmapload_la_SOURCES = maploader.c glBSP/blockmap.c glBSP/glbsp.c \
	glBSP/level.c glBSP/node.c glBSP/reject.c glBSP/seg.c	     \
	glBSP/system.c glBSP/util.c glBSP/wad.c glBSP/blockmap.h     \
	glBSP/glbsp.h glBSP/level.h glBSP/node.h glBSP/reject.h	     \
	glBSP/seg.h glBSP/structs.h glBSP/system.h glBSP/util.h	     \
	glBSP/wad.h
