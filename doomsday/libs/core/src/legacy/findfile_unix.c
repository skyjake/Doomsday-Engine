/** @file findfile_unix.c Win32-style native file finding.
 *
 * @author Copyright &copy; 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>

#include "de/legacy/findfile.h"
#include "de/legacy/memory.h"

#define DIR_SEP_CHAR    '/'
#define DIR_SEP_STR     "/"
#define FIND_ERROR      -1

typedef struct fdata_s {
    char* pattern;
    glob_t buf;
    int pos;
} fdata_t;

/**
 * Get the info for the next file.
 */
static int nextfinddata(FindData *fd)
{
    fdata_t* data = fd->finddata;
    char* fn, *last;
    struct stat st;

    if ((int)data->buf.gl_pathc <= data->pos)
    {
        // Nothing was found.
        return FIND_ERROR;
    }

    // Nobody needs these...
    fd->date = 0;
    fd->time = 0;

    // Get the size of the file.
    fn = data->buf.gl_pathv[data->pos];
    if (!stat(fn, &st))
        fd->size = st.st_size;
    else
        fd->size = 0;

    // Is it a directory?
    last = fn + strlen(fn) - 1;
    if (*last == '/')
    {
        // Return the name of the last directory in the path.
        char* slash = last - 1;
        while (*slash != '/' && slash > fn) --slash;
        Str_Set(&fd->name, *slash == '/'? slash + 1 : slash);
        fd->attrib = A_SUBDIR;
    }
    else
    {
        char ext[256];
        char name[356];

        _splitpath(fn, NULL, NULL, name, ext);
        Str_Clear(&fd->name); // It may have previously been populated.
        Str_Appendf(&fd->name, "%s%s", name, ext);
        fd->attrib = 0;
    }

    // Advance the position.
    data->pos++;
    return 0;
}

int FindFile_FindFirst(FindData *fd, char const *filename)
{
    fdata_t* data;

    // Allocate a new glob struct.
    fd->finddata = data = M_Calloc(sizeof(*data));
    Str_InitStd(&fd->name);

    // Make a copy of the pattern.
    data->pattern = M_Malloc(strlen(filename) + 1);
    strcpy(data->pattern, filename);

    // Do the glob.
    glob(filename, GLOB_MARK, NULL, &data->buf);

    return nextfinddata(fd);
}

int FindFile_FindNext(FindData *fd)
{
    if (!fd->finddata)
        return FIND_ERROR;
    return nextfinddata(fd);
}

void FindFile_Finish(FindData *fd)
{
    globfree(&((fdata_t*) fd->finddata)->buf);
    M_Free(((fdata_t*) fd->finddata)->pattern);
    Str_Free(&fd->name);
    M_Free(fd->finddata);
    fd->finddata = NULL;
}

/**
 * Removes references to the current (.) and parent (..) directories.
 * The given path should be an absolute path.
 */
static void resolvePath(char* path)
{
    assert(path);
    {
    char* ch = path;
    char* end = path + strlen(path);
    char* prev = path; // Assume an absolute path.

    for (; *ch; ch++)
    {
        if (ch[0] == '/' && ch[1] == '.')
        {
            if (ch[2] == '/')
            {
                memmove(ch, ch + 2, end - ch - 1);
                ch--;
            }
            else if (ch[2] == '.' && ch[3] == '/')
            {
                memmove(prev, ch + 3, end - ch - 2);
                // Must restart from the beginning.
                // This is a tad inefficient, though.
                ch = path - 1;
                continue;
            }
        }
        if (*ch == '/')
            prev = ch;
    }
    }
}

char* _fullpath(char* full, const char* original, int maxLen)
{
    char* cwd, *buf;

    // @todo Check for '~'.

    if (original[0] != DIR_SEP_CHAR) // A relative path?
    {
        /// @todo Check for ERANGE.
        cwd = getcwd(NULL, 0);
        if (!cwd) Libdeng_BadAlloc();
        buf = (char*) M_Malloc(strlen(cwd) + 1/*DIR_SEP_CHAR*/ + strlen(original) + 1);
        strcpy(buf, cwd);
        strcat(buf, DIR_SEP_STR);
        strcat(buf, original);
        free(cwd);
    }
    else
    {
        size_t len = strlen(original);
        buf = (char*) M_Malloc(len + 1);
        memcpy(buf, original, len);
        buf[len] = 0;
    }

    // Remove "."s and ".."s.
    resolvePath(buf);

    // Clear the given buffer and copy the full path there.
    memset(full, 0, maxLen);
    strncpy(full, buf, maxLen - 1);
    free(buf);
    return full;
}

static void strzncpy(char* dest, const char* src, int count)
{
    char* out = dest;
    const char* in = src;

    while (count-- > 0)
    {
        *out++ = *in++;
        if (!*in)
            break;
    }
    *out = 0;
}

void _splitpath(const char* path, char* drive, char* dir, char* name, char* ext)
{
    char* lastPeriod, *lastSlash;

    if (drive)
        strcpy(drive, ""); // There is never a drive letter.

    lastPeriod = strrchr(path, '.');
    lastSlash = strrchr(path, '/');
    if (lastPeriod < lastSlash)
        lastPeriod = NULL;

    if (dir)
    {
        if (lastSlash)
            strzncpy(dir, path, lastSlash - path + 1);
        else
            strcpy(dir, "");
    }

    // The name should not include the extension.
    if (name)
    {
        if (lastSlash && lastPeriod)
            strzncpy(name, lastSlash + 1, lastPeriod - lastSlash - 1);
        else if (lastSlash)
            strcpy(name, lastSlash + 1);
        else if (lastPeriod)
            strzncpy(name, path, lastPeriod - path);
        else
            strcpy(name, path);
    }

    // Last period gives us the extension.
    if (ext)
    {
        if (lastPeriod)
            strcpy(ext, lastPeriod);
        else
            strcpy(ext, "");
    }
}
