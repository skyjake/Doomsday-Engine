//===========================================================================
// DD_ZIP.H
//===========================================================================
#ifndef __DOOMSDAY_ZIP_PACKAGE_H__
#define __DOOMSDAY_ZIP_PACKAGE_H__

typedef int zipindex_t;

void		Zip_Init(void);
void		Zip_Shutdown(void);

boolean		Zip_Open(const char *fileName);
zipindex_t	Zip_Find(int (*finder)(const char*, void*), void *parm);
uint		Zip_GetSize(zipindex_t index);
uint		Zip_Read(zipindex_t index, void *buffer);

#endif 