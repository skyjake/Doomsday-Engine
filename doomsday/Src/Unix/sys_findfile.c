/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sys_findfile.c: Win32-Style File Finding (findfirst/findnext)
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <sys/stat.h>

#include "sys_findfile.h"
#include "Unix/sys_path.h"

// MACROS ------------------------------------------------------------------

#define FIND_ERROR  -1

// TYPES -------------------------------------------------------------------

typedef struct fdata_s {
	char *pattern;
	glob_t buf;
	int pos;
} fdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Get the info for the next file.
 */
static int nextfinddata(finddata_t *fd)
{
	fdata_t *data = fd->finddata;
	char *fn;
	char ext[256];
	struct stat st;

	if(data->buf.gl_pathc <= data->pos)
	{
		// Nothing was found.
		return FIND_ERROR;
	}

	// Nobody needs these...
	fd->date = 0;
	fd->time = 0;
	
	// Get the size of the file.
	fn = data->buf.gl_pathv[data->pos];
	if(!stat(fn, &st))
		fd->size = st.st_size;
	else
		fd->size = 0;
	
	if(fd->name) free(fd->name);
	fd->name = malloc(strlen(fn) + 1);
	_splitpath(fn, NULL, NULL, fd->name, ext);
	strcat(fd->name, ext);

	// Is it a directory?
	if(fn[strlen(fn) - 1] == '/')
	{
		fd->attrib = A_SUBDIR;
	}
	else
	{
		fd->attrib = 0;
	}
	
	// Advance the position.
	data->pos++;
	return 0;
}

/*
 * Returns zero if successful.
 */
int myfindfirst(const char *filename, finddata_t *fd)
{
	fdata_t *data;
	
	// Allocate a new glob struct.
	fd->finddata = data = calloc(1, sizeof(fdata_t));
	fd->name = NULL;

	// Make a copy of the pattern.
	data->pattern = malloc(strlen(filename) + 1);
	strcpy(data->pattern, filename);

	// Do the glob.
	glob(filename, GLOB_MARK, NULL, &data->buf);

	return nextfinddata(fd);
	
/*	dta->hFile = _findfirst(filename, &dta->data);
	
	dta->date = dta->data.time_write;
	dta->time = dta->data.time_write;
	dta->size = dta->data.size;
	dta->name = dta->data.name;
	dta->attrib = dta->data.attrib;

	return dta->hFile<0;*/
}

/*
 * Returns zero if successful.
 */
int myfindnext(finddata_t *fd)
{
	if(!fd->finddata) return FIND_ERROR;
	return nextfinddata(fd);
}

void myfindend(finddata_t *fd)
{
	globfree(&((fdata_t*)fd->finddata)->buf);
	free(((fdata_t*)fd->finddata)->pattern);
	free(fd->name);
	free(fd->finddata);
	fd->finddata = NULL;
}
