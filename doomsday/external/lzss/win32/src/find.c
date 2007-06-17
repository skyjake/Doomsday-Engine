#include "Find.h"

int myfindfirst(char *filename, struct finddata *dta, long attrib)
{
	dta->hFile = _findfirst(filename, &dta->data);
	
	dta->date = dta->data.time_write;
	dta->time = dta->data.time_write;
	dta->size = dta->data.size;
	dta->name = dta->data.name;
	dta->attrib = dta->data.attrib;

	return dta->hFile<0;
}

int myfindnext(struct finddata *dta)
{
	int r = _findnext(dta->hFile, &dta->data);

	dta->date = dta->data.time_write;
	dta->time = dta->data.time_write;
	dta->size = dta->data.size;
	dta->name = dta->data.name;
	dta->attrib = dta->data.attrib;

	return r;
}

void myfindend(struct finddata *dta)
{
	_findclose(dta->hFile);
}
