/** @file sys_direc.c Native file system directories.
 * @ingroup system
 *
 * @todo Rewrite using libdeng2's NativePath (and Qt).
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#if defined(WIN32)
#  include <direct.h>
#endif

#if defined(UNIX)
#  include <unistd.h>
#  include <limits.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <pwd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <de/findfile.h>

#include "de_base.h"
#include "de_platform.h"
#include "de_system.h"
#include "m_misc.h"

static void setPathFromPathDir(directory_t* dir, const char* path);

static void prependBasePath(char* newPath, const char* path, size_t maxLen);
static void resolveAppRelativeDirectives(char* translated, const char* path, size_t maxLen);
#if defined(UNIX)
static void resolveHomeRelativeDirectives(char* path, size_t maxLen);
#endif
static void resolvePathRelativeDirectives(char* path);

directory_t* Dir_New(const char* path)
{
    directory_t* dir = (directory_t*) malloc(sizeof *dir);
    if(!dir)
        printf("Dir::Construct: Failed on allocation of %lu bytes for new Dir.", (unsigned long) sizeof(*dir));
    Dir_SetPath(dir, path);
    return dir;
}

directory_t* Dir_NewFromCWD(void)
{
    directory_t* dir = (directory_t*) malloc(sizeof *dir);
    size_t lastIndex;
    char* cwd;
    if(!dir)
        printf("Dir::ConstructFromWorkDir: Failed on allocation of %lu bytes for new Dir.\n", (unsigned long) sizeof(*dir));

    cwd = Dir_CurrentPath();
    lastIndex = strlen(cwd);
    lastIndex = MIN_OF(lastIndex, FILENAME_T_LASTINDEX);

#if defined(WIN32)
    dir->drive = _getdrive();
#endif
    memcpy(dir->path, cwd, lastIndex);
    dir->path[lastIndex] = '\0';
    free(cwd);
    return dir;
}

directory_t* Dir_FromText(const char* path)
{
    directory_t* dir;
    if(!path || !path[0])
        return Dir_NewFromCWD();

    dir = (directory_t*) malloc(sizeof *dir);
    if(!dir)
        printf("Dir::ConstructFromFileDir: Failed on allocation of %lu bytes for new Dir.", (unsigned long) sizeof(*dir));
    setPathFromPathDir(dir, path);
    return dir;
}

void Dir_Delete(directory_t* dir)
{
    assert(NULL != dir);
    free(dir);
}

boolean Dir_IsEqual(directory_t* a, directory_t* b)
{
    if(a == b) return true;
#if defined(WIN32)
    if(a->drive != b->drive)
        return false;
#endif
    return !stricmp(a->path, b->path);
}

const char* Dir_Path(directory_t* dir)
{
    assert(NULL != dir);
    return dir->path;
}

void Dir_SetPath(directory_t* dir, const char* path)
{
    filename_t fileName;
    assert(dir);

    setPathFromPathDir(dir, path);
    Dir_FileName(fileName, path, FILENAME_T_MAXLEN);
    M_StrCat(dir->path, fileName, FILENAME_T_MAXLEN);
    // Ensure we've a well-formed path.
    Dir_CleanPath(dir->path, FILENAME_T_MAXLEN);
}

static void setPathFromPathDir(directory_t* dir, const char* path)
{
    filename_t temp, transPath;
    assert(dir && path && path[0]);

    resolveAppRelativeDirectives(transPath, path, FILENAME_T_MAXLEN);
#ifdef UNIX
    resolveHomeRelativeDirectives(transPath, FILENAME_T_MAXLEN);
#endif
    Dir_ToNativeSeparators(transPath, FILENAME_T_MAXLEN);

    _fullpath(temp, transPath, FILENAME_T_MAXLEN);
    _splitpath(temp, dir->path, transPath, 0, 0);
    M_StrCat(dir->path, transPath, FILENAME_T_MAXLEN);
#if defined(WIN32)
    dir->drive = toupper(dir->path[0]) - 'A' + 1;
#endif
    Dir_FixSeparators(dir->path, FILENAME_T_MAXLEN);
}

/// Class-Static Members:

static void prependBasePath(char* newPath, const char* path, size_t maxLen)
{
    assert(newPath && path);
    // Cannot prepend to absolute paths.
    if(!Dir_IsAbsolutePath(path))
    {
        filename_t buf;
        dd_snprintf(buf, maxLen, "%s%s", ddBasePath, path);
        memcpy(newPath, buf, maxLen);
        return;
    }
    strncpy(newPath, path, maxLen);
}

static void resolveAppRelativeDirectives(char* translated, const char* path, size_t maxLen)
{
    filename_t buf;
    assert(translated && path);

    if(path[0] == '>' || path[0] == '}')
    {
        path++;
        if(!Dir_IsAbsolutePath(path))
            prependBasePath(buf, path, maxLen);
        else
            strncpy(buf, path, maxLen);
        strncpy(translated, buf, maxLen);
    }
    else if(translated != path)
    {
        strncpy(translated, path, maxLen);
    }
}

#if defined(UNIX)
static void resolveHomeRelativeDirectives(char* path, size_t maxLen)
{
    filename_t buf;
    assert(path);

    if(!path[0] || 0 == maxLen || path[0] != '~') return;

    memset(buf, 0, sizeof(buf));

    if(path[1] == '/')
    {
        // Replace it with the HOME environment variable.
        strncpy(buf, getenv("HOME"), FILENAME_T_MAXLEN);
        if(LAST_CHAR(buf) != '/')
            M_StrCat(buf, "/", FILENAME_T_MAXLEN);

        // Append the rest of the original path.
        M_StrCat(buf, path + 2, FILENAME_T_MAXLEN);
    }
    else
    {
        char userName[4096], *end = NULL;
        struct passwd *pw;

        end = strchr(path + 1, '/');
        strncpy(userName, path, end - path - 1);
        userName[end - path - 1] = 0;

        pw = getpwnam(userName);
        if(pw)
        {
            strncpy(buf, pw->pw_dir, FILENAME_T_MAXLEN);
            if(LAST_CHAR(buf) != '/')
                M_StrCat(buf, "/", FILENAME_T_MAXLEN);
        }

        M_StrCat(buf, path + 1, FILENAME_T_MAXLEN);
    }

    // Replace the original.
    strncpy(path, buf, maxLen - 1);
}
#endif

static void resolvePathRelativeDirectives(char* path)
{
    assert(NULL != path);
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

void Dir_CleanPath(char* path, size_t len)
{
    if(!path || 0 == len) return;

    M_Strip(path, len);
#if defined(UNIX)
    resolveHomeRelativeDirectives(path, len);
#endif
    Dir_FixSeparators(path, len);
    resolvePathRelativeDirectives(path);
}

void Dir_CleanPathStr(ddstring_t* str)
{
    size_t len = Str_Length(str);
    char* path = strdup(Str_Text(str));
    Dir_CleanPath(path, len);
    Str_Set(str, path);
    free(path);
}

char* Dir_CurrentPath(void)
{
    char* path = _getcwd(NULL, 0);
    size_t len = strlen(path);
    // Why oh why does the OS not do this for us?
    if(len != 0)
    {
        if(path[len - 1] != '/')
        {
            path = (char*) realloc(path, len+2);
            if(!path)
            {
                Sys_CriticalMessagef("Dir::WorkDir: Failed on reallocation of %lu bytes for out string.", (unsigned long) (len+2));
                return NULL;
            }
            strcat(path, "/");
        }
        Dir_ToNativeSeparators(path, len);
    }
    return path;
}

void Dir_FileName(char* name, const char* path, size_t len)
{
    char ext[100]; /// @todo  Use dynamic string.
    if(!path || !name || 0 == len) return;
    _splitpath(path, 0, 0, name, ext);
    M_StrCat(name, ext, len);
}

int Dir_IsAbsolutePath(const char* path)
{
    if(!path || !path[0]) return 0;
    if(path[0] == '/' || path[1] == ':')
        return true;
#if defined(UNIX)
    if(path[0] == '~')
        return true;
#endif
    return false;
}

boolean Dir_mkpath(const char* path)
{
#if !defined(WIN32) && !defined(UNIX)
#  error Dir_mkpath has no implementation for this platform.
#endif

    filename_t full, buf;
    char* ptr, *endptr;

    if(!path || !path[0]) return false;

    // Convert all backslashes to normal slashes.
    strncpy(full, path, FILENAME_T_MAXLEN);
    Dir_ToNativeSeparators(full, FILENAME_T_MAXLEN);

    // Does this path already exist?
    if(0 == access(full, 0))
        return true;

    // Check and create the path in segments.
    ptr = full;
    memset(buf, 0, sizeof(buf));
    do
    {
        endptr = strchr(ptr, DIR_SEP_CHAR);
        if(!endptr)
            M_StrCat(buf, ptr, FILENAME_T_MAXLEN);
        else
            M_StrnCat(buf, ptr, endptr - ptr, FILENAME_T_MAXLEN);

        if(buf[0] && access(buf, 0))
        {
            // Path doesn't exist, create it.
#if defined(WIN32)
            mkdir(buf);
#elif defined(UNIX)
            mkdir(buf, 0775);
#endif
        }
        M_StrCat(buf, DIR_SEP_STR, FILENAME_T_MAXLEN);
        ptr = endptr + 1;

    } while(endptr);

    return (0 == access(full, 0));
}

void Dir_MakeAbsolutePath(char* path, size_t len)
{
    filename_t buf;
    if(!path || !path[0] || 0 == len) return;

#if defined(UNIX)
    resolveHomeRelativeDirectives(path, len);
#endif
    _fullpath(buf, path, FILENAME_T_MAXLEN);
    strncpy(path, buf, len);
    Dir_FixSeparators(path, len);
}

void Dir_ToNativeSeparators(char* path, size_t len)
{
    size_t i;
    if(!path || !path[0] || 0 == len) return;

    for(i = 0; i < len && path[i]; ++i)
    {
        if(path[i] == DIR_WRONG_SEP_CHAR)
            path[i] = DIR_SEP_CHAR;
    }
}

void Dir_FixSeparators(char* path, size_t len)
{
    size_t i;
    if(!path || !path[0] || 0 == len) return;

    for(i = 0; i < len && path[i]; ++i)
    {
        if(path[i] == '\\')
            path[i] = '/';
    }
}

boolean Dir_SetCurrent(const char* path)
{
    boolean success = false;
    if(path && path[0])
    {
        success = !_chdir(path);
    }
    printf("Dir::ChangeWorkDir: %s: %s\n", success ? "Succeeded" : "Failed", path);
    return success;
}
