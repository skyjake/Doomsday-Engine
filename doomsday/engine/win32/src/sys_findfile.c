/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Wrappers for File Finding (Win32)
 */

#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "de_base.h"
#include "de_filesys.h"

#include "dd_string.h"

typedef struct winfinddata_s {
    struct _finddata_t data;
    intptr_t handle;
} winfinddata_t;

static void setdata(finddata_t* dta)
{
    winfinddata_t* fd = dta->finddata;
    dta->date = fd->data.time_write;
    dta->time = fd->data.time_write;
    dta->size = fd->data.size;
    Str_Set(&dta->name, fd->data.name);
    Dir_FixSeparators(Str_Text(&dta->name), Str_Length(&dta->name));
    dta->attrib = 0;
    if(fd->data.attrib & _A_SUBDIR)
    {
        if(Str_Compare(&dta->name, ".") && Str_Compare(&dta->name, ".."))
            F_AppendMissingSlash(&dta->name);
        dta->attrib |= A_SUBDIR;
    }
}

int myfindfirst(const char* filename, finddata_t* dta)
{
    winfinddata_t* fd;
    assert(filename && dta);

    // Allocate a new private finddata struct.
    dta->finddata = fd = calloc(1, sizeof(*fd));
    Str_InitStd(&dta->name);

    // Begin the search.
    fd->handle = _findfirst(filename, &fd->data);

    setdata(dta);
    return (fd->handle == (long) (-1));
}

int myfindnext(finddata_t* dta)
{
    winfinddata_t* fd;
    int result;

    if(!dta) return 0;

    fd = dta->finddata;
    result = _findnext(fd->handle, &fd->data);
    if(!result)
        setdata(dta);
    return result != 0;
}

void myfindend(finddata_t* dta)
{
    assert(dta);
    _findclose(((winfinddata_t*) dta->finddata)->handle);
    free(dta->finddata);
    Str_Free(&dta->name);
    memset(dta, 0, sizeof(finddata_t));
}
