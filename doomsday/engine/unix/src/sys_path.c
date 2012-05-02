/**\file sys_path.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <unistd.h>
#include <string.h>

#include "de_base.h"
#include "de_console.h"

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

    for(; *ch; ch++)
    {
        if(ch[0] == '/' && ch[1] == '.')
        {
            if(ch[2] == '/')
            {
                memmove(ch, ch + 2, end - ch - 1);
                ch--;
            }
            else if(ch[2] == '.' && ch[3] == '/')
            {
                memmove(prev, ch + 3, end - ch - 2);
                // Must restart from the beginning.
                // This is a tad inefficient, though.
                ch = path - 1;
                continue;
            }
        }
        if(*ch == '/')
            prev = ch;
    }
    }
}

char* _fullpath(char* full, const char* original, int maxLen)
{
    char* cwd, *buf;

    // @todo Check for '~'.

    if(original[0] != DIR_SEP_CHAR) // A relative path?
    {
        /// @todo Check for ERANGE.
        cwd = getcwd(NULL, 0);
        if(NULL == cwd)
        {
            // Yikes!
            Con_AbnormalShutdown("_fullpath: Failed retrieving the current working directory.");
            return NULL; // Unreachable.
        }
        // dj: I can't find any info about whether I can safely realloc the
        // pointer returned by getcwd so I'm building a copy.
        buf = (char*) malloc(strlen(cwd) + 1/*DIR_SEP_CHAR*/ + strlen(original) + 1);
        strcpy(buf, cwd);
        strcat(buf, DIR_SEP_STR);
        strcat(buf, original);
        free(cwd);
    }
    else
    {
        size_t len = strlen(original);
        buf = (char*) malloc(len+1);
        if(NULL == buf)
        {
            Con_AbnormalShutdown("_fullpath: Failed allocating storage for path copy.");
            return NULL; // Unreachable.
        }
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

void strzncpy(char* dest, const char* src, int count)
{
    char* out = dest;
    const char* in = src;

    while(count-- > 0)
    {
        *out++ = *in++;
        if(!*in)
            break;
    }
    *out = 0;
}

void _splitpath(const char* path, char* drive, char* dir, char* name, char* ext)
{
    char* lastPeriod, *lastSlash;

    if(drive)
        strcpy(drive, ""); // There is never a drive letter.

    lastPeriod = strrchr(path, '.');
    lastSlash = strrchr(path, '/');
    if(lastPeriod < lastSlash)
        lastPeriod = NULL;

    if(dir)
    {
        if(lastSlash)
            strzncpy(dir, path, lastSlash - path + 1);
        else
            strcpy(dir, "");
    }

    // The name should not include the extension.
    if(name)
    {
        if(lastSlash && lastPeriod)
            strzncpy(name, lastSlash + 1, lastPeriod - lastSlash - 1);
        else if(lastSlash)
            strcpy(name, lastSlash + 1);
        else
            strzncpy(name, path, lastPeriod - path);
    }

    // Last period gives us the extension.
    if(ext)
    {
        if(lastPeriod)
            strcpy(ext, lastPeriod);
        else
            strcpy(ext, "");
    }
}
