/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Win32-Style File Finding (findfirst/findnext)
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <sys/stat.h>

#include "sys_findfile.h"
#include "../include/sys_path.h"

// MACROS ------------------------------------------------------------------

#define FIND_ERROR  -1

// TYPES -------------------------------------------------------------------

typedef struct fdata_s {
    char* pattern;
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

/**
 * Get the info for the next file.
 */
static int nextfinddata(finddata_t* fd)
{
    fdata_t* data = fd->finddata;
    char* fn, *last;
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

    if(fd->name)
        free(fd->name);
    fd->name = malloc(strlen(fn) + 1);

    // Is it a directory?
    last = fn + strlen(fn) - 1;
    if(*last == '/')
    {
        // Return the name of the last directory in the path.
        char *slash = last - 1;
        int len;
        while(*slash != '/' && slash > fn) --slash;
        len = last - slash - 1;
        strncpy(fd->name, slash + 1, len);
        fd->name[len] = 0;
        fd->attrib = A_SUBDIR;
    }
    else
    {
        _splitpath(fn, NULL, NULL, fd->name, ext);
        strcat(fd->name, ext);
        fd->attrib = 0;
    }

    // Advance the position.
    data->pos++;
    return 0;
}

int myfindfirst(const char* filename, finddata_t* fd)
{
    fdata_t* data;

    // Allocate a new glob struct.
    fd->finddata = data = calloc(1, sizeof(*data));
    fd->name = NULL;

    // Make a copy of the pattern.
    data->pattern = malloc(strlen(filename) + 1);
    strcpy(data->pattern, filename);

    // Do the glob.
    glob(filename, GLOB_MARK, NULL, &data->buf);

    return nextfinddata(fd);
}

int myfindnext(finddata_t* fd)
{
    if(!fd->finddata)
        return FIND_ERROR;
    return nextfinddata(fd);
}

void myfindend(finddata_t* fd)
{
    globfree(&((fdata_t*) fd->finddata)->buf);
    free(((fdata_t*) fd->finddata)->pattern);
    free(fd->name);
    free(fd->finddata);
    fd->finddata = NULL;
}
