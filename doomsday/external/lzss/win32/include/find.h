#ifndef __LZSS_FILEFINDER_H__
#define __LZSS_FILEFINDER_H__

#include <io.h>
#include <fcntl.h>

typedef struct finddata
{
	struct _finddata_t data;
	long date;
	long time;
	long size;
	char *name;
	long attrib;
	long hFile;
} 
finddata;

#define FILE_OPEN(filename, handle)             handle = open(filename, O_RDONLY | O_BINARY)
#define FILE_CREATE(filename, handle)           handle = open(filename, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC)
#define FILE_CLOSE(handle)                      close(handle)
#define FILE_READ(handle, buf, size, sz)        sz = read(handle, buf, size)
#define FILE_WRITE(handle, buf, size, sz)       sz = write(handle, buf, size) 
#define FILE_SEARCH_STRUCT                      struct finddata
#define FILE_FINDFIRST(filename, attrib, dta)   myfindfirst(filename, dta, attrib)
#define FILE_FINDNEXT(dta)                      myfindnext(dta)
#define FILE_FINDCLOSE(dta)						myfindend(dta)
#define FILE_ATTRIB                             attrib
#define FILE_SIZE                               size
#define FILE_NAME                               name
#define FILE_DATE								date
#define FILE_TIME								time

int myfindfirst(char *filename, struct finddata *dta, long attrib);
int myfindnext(struct finddata *dta);
void myfindend(struct finddata *dta);

#endif