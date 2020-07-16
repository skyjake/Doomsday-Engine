/** @file findfile_windows.cpp  Win32-style native file finding.
 *
 * @author Copyright &copy; 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#if defined (WIN32) && defined (_MSC_VER)

#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <string>
#include "de/string.h"

#include "de/legacy/memory.h"
#include "de/legacy/findfile.h"

typedef struct winfinddata_s {
    struct _wfinddata_t data;
    intptr_t handle;
} winfinddata_t;

static void setdata(FindData *dta)
{
    winfinddata_t *fd = reinterpret_cast<winfinddata_t *>(dta->finddata);
    dta->date = fd->data.time_write;
    dta->time = fd->data.time_write;
    dta->size = fd->data.size;
    Str_Set(&dta->name, de::String(fd->data.name));
    Str_ReplaceAll(&dta->name, '\\', '/');
    dta->attrib = 0;
    if (fd->data.attrib & _A_SUBDIR)
    {
        if (Str_Compare(&dta->name, ".") && Str_Compare(&dta->name, ".."))
        {
            if (!Str_EndsWith(&dta->name, "/")) Str_Append(&dta->name, "/");
        }
        dta->attrib |= A_SUBDIR;
    }
}

int FindFile_FindFirst(FindData *dta, const char *filenameUtf8)
{
    DE_ASSERT(filenameUtf8 && dta);

    // Allocate a new private finddata struct.
    winfinddata_t *fd = reinterpret_cast<winfinddata_t *>(M_Calloc(sizeof(*fd)));
    dta->finddata = fd;
    Str_InitStd(&dta->name);

    // Begin the search.
    fd->handle = _wfindfirst(de::String(filenameUtf8).toWideString().c_str(), &fd->data);

    setdata(dta);
    return (fd->handle == (long) (-1));
}

int FindFile_FindNext(FindData *dta)
{
    winfinddata_t *fd;
    int result;

    if (!dta) return 0;

    fd = reinterpret_cast<winfinddata_t *>(dta->finddata);
    result = _wfindnext(fd->handle, &fd->data);
    if (!result)
    {
        setdata(dta);
    }
    return result != 0;
}

void FindFile_Finish(FindData *dta)
{
    DE_ASSERT(dta);

    _findclose(((winfinddata_t*) dta->finddata)->handle);
    M_Free(dta->finddata);
    Str_Free(&dta->name);
    memset(dta, 0, sizeof(FindData));
}

#endif // defined (WIN32) && defined (_MSC_VER)
