# $Id$

SRC = 	$(wildcard Src/cl_*.c) \
	$(wildcard Src/con_*.c) \
	Src/dd_defns.c \
	Src/dd_dgl.c \
	Src/dd_help.c \
	Src/dd_input.c \
	Src/dd_loop.c \
	Src/dd_main.c \
	Src/dd_pinit.c \
	Src/dd_plugin.c \
	Src/dd_wad.c \
	Src/dd_zip.c \
	Src/dd_zone.c \
	$(wildcard Src/ded*.c) \
	$(wildcard Src/gl_*.c) \
	$(wildcard Src/m_*.c) \
	$(wildcard Src/net_*.c) \
	$(wildcard Src/p_*.c) \
	$(wildcard Src/r*.c) \
	$(wildcard Src/s_*.c) \
	$(wildcard Src/sv_*.c) \
	$(wildcard Src/tab_*.c) \
	$(wildcard Src/ui_*.c) \
	Src/sys_filein.c \
	Src/sys_direc.c \
	Src/sys_master.c \
	Src/sys_sock.c \
	Src/sys_system.c \
	Src/sys_timer.c \
	$(wildcard Src/Unix/*.c) \
	Src/Common/m_fixed.c \

INC = $(wildcard Include/*.h)
OBJ = $(patsubst %.c,%.o,$(SRC))
DEFS = -DUNIX -DNO_FIXED_ASM -D__DOOMSDAY__ -DNORANGECHECKING
DEBUGFLAGS = -g -Wall
CFLAGS = $(DEBUGFLAGS) -IInclude $(DEFS) $(shell sdl-config --cflags)
LIBS = `sdl-config --libs` -lSDL_net -ldl -lpng -lncurses

.PHONY: dep clean realclean

doomsday: $(OBJ) $(INC)
	$(CC) -rdynamic -o doomsday $(LIBS) $(OBJ)

clean:
	rm -f $(OBJ) doomsday
	rm -f Src/dpDehRead/dehmain.o

realclean:
	make -k clean
	makedepend

dep:
	etags $(SRC) Include/*.h Include/Common/*.h
	makedepend -YInclude $(DEFS) $(SRC) 2> /dev/null

# The DeHackEd reader plugin is so simple that it can be built here as
# well.
dpdehread.so: Src/dpDehRead/dehmain.o
	$(CC) -shared -o dpdehread.so Src/dpDehRead/dehmain.o

Src/dpDehRead/dehmain.o: Src/dpDehRead/dehmain.c Include/doomsday.h Include/dedfile.h
	$(CC) -c $(DEFS) $(DEBUGFLAGS) -fPIC -IInclude -o Src/dpDehRead/dehmain.o Src/dpDehRead/dehmain.c

# DO NOT DELETE
