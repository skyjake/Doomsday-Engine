# $Id$
INSTALLDIR=$(HOME)/bin
LIBS=-lm

.PHONY: clean install

md2: md2tool.c anorms.h md2tool.h
	$(CC) $(CFLAGS) -o md2 md2tool.c $(LIBS)

clean:
	rm -rf md2tool.o md2

install:
	install md2 $(INSTALLDIR)
