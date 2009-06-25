/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * sys_findfile.c: Wrappers for File Finding (Win32)
 */

// HEADER FILES ------------------------------------------------------------

#include <io.h>
#include <stdlib.h>
#include <string.h>

#include "sys_findfile.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct winfinddata_s {
    struct _finddata_t data;
    intptr_t        handle;
} winfinddata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void setdata(finddata_t *dta)
{
    winfinddata_t *fd = dta->finddata;

    dta->date = fd->data.time_write;
    dta->time = fd->data.time_write;
    dta->size = fd->data.size;
    dta->name = fd->data.name;
    dta->attrib = 0;
    if(fd->data.attrib & _A_SUBDIR)
        dta->attrib |= A_SUBDIR;
}

int myfindfirst(const char *filename, finddata_t *dta)
{
    winfinddata_t *fd;

    // Allocate a new private finddata struct.
    dta->finddata = fd = calloc(1, sizeof(winfinddata_t));

    // Begin the search.
    fd->handle = _findfirst(filename, &fd->data);

    setdata(dta);
    return (fd->handle == (long) (-1));
}

int myfindnext(finddata_t *dta)
{
    int         result;
    winfinddata_t *fd = dta->finddata;

    result = _findnext(fd->handle, &fd->data);
    if(!result)
        setdata(dta);
    return result != 0;
}

void myfindend(finddata_t *dta)
{
    _findclose(((winfinddata_t *) dta->finddata)->handle);
    free(dta->finddata);
    memset(dta, 0, sizeof(*dta));
}
