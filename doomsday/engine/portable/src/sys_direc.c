/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
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
 * sys_direc.c: Directory Utilities
 *
 * Directory and file system related stuff.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

#if defined(WIN32)
#   include <direct.h>
#endif
#if defined(UNIX)
#   include <unistd.h>
#endif

#include "de_platform.h"

#if defined(WIN32)
#  include <direct.h>
#endif
#if defined(UNIX)
#  include <unistd.h>
#  include <limits.h>
#  include <sys/types.h>
#  include <pwd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Dir_GetDir(directory_t *dir)
{
    memset(dir, 0, sizeof(*dir));

    dir->drive = _getdrive();
    if(!_getcwd(dir->path, 255))
    {
        fprintf(stderr, "Dir_GetDir: Failed to determine current directory.\n");
    }

    if(LAST_CHAR(dir->path) != DIR_SEP_CHAR)
        strcat(dir->path, DIR_SEP_STR);

    /* VERBOSE2( printf("Dir_GetDir: %s\n", dir->path) ); */
}

int Dir_ChDir(directory_t* dir)
{
    int                 success;

    _chdrive(dir->drive);
    success = !_chdir(dir->path);   // Successful if == 0.

    VERBOSE2(Con_Printf
             ("Dir_ChDir: %s: %s\n", success ? "Succeeded" : "Failed",
              M_PrettyPath(dir->path)));

    return success;
}

void Dir_MakeDir(const char* path, directory_t* dir)
{
    filename_t          temp;

    Dir_FileDir(path, dir);
    Dir_FileName(temp, path, FILENAME_T_MAXLEN);
    strncat(dir->path, temp, FILENAME_T_MAXLEN);
    // Make it a well formed path.
    Dir_ValidDir(dir->path, FILENAME_T_MAXLEN);
}

/**
 * Translates the given filename (>,} => basedir).
 */
void Dir_FileDir(const char* str, directory_t* dir)
{
    filename_t          temp, pth;
    
    if(!str)
    {
        // Nothing to do.
        return;
    }
    
    M_TranslatePath(pth, str, FILENAME_T_MAXLEN);

    _fullpath(temp, pth, FILENAME_T_MAXLEN);
    _splitpath(temp, dir->path, pth, 0, 0);
    strncat(dir->path, pth, FILENAME_T_MAXLEN);
#ifdef WIN32
    dir->drive = toupper(dir->path[0]) - 'A' + 1;
#endif
}

void Dir_FileName(char* name, const char* str, size_t len)
{
    char                ext[100];

    _splitpath(str, 0, 0, name, ext);
    strncat(name, ext, len);
}

/**
 * Calculate an identifier for the file based on its full path name.
 * The identifier is the MD5 hash of the path.
 */
void Dir_FileID(const char *str, byte identifier[16])
{
    filename_t          temp;
    md5_ctx_t           context;

    // First normalize the name.
    memset(temp, 0, FILENAME_T_MAXLEN);
    _fullpath(temp, str, FILENAME_T_MAXLEN);
    Dir_FixSlashes(temp, FILENAME_T_MAXLEN);

#if defined(WIN32) || defined(MACOSX)
    // This is a case insensitive operation.
    strupr(temp);
#endif

    md5_init(&context);
    md5_update(&context, (byte*) temp, (unsigned int) strlen(temp));
    md5_final(&context, identifier);
}

/**
 * @return              @c true, if the directories are equal.
 */
boolean Dir_IsEqual(directory_t *a, directory_t *b)
{
    if(a->drive != b->drive)
        return false;
    return !stricmp(a->path, b->path);
}

/**
 * @return              @c true, if the given path is absolute
 *                      (starts with \ or / or the second character is
 *                      a ':' (drive).
 */
int Dir_IsAbsolute(const char* str)
{
    if(!str)
        return 0;

    if(str[0] == '\\' || str[0] == '/' || str[1] == ':')
        return true;
#ifdef UNIX
    if(str[0] == '~')
        return true;
#endif
    return false;
}

/**
 * Converts directory slashes to the correct type of slash.
 */
void Dir_FixSlashes(char* path, size_t len)
{
    size_t              i;

    for(i = 0; i < len && path[i]; ++i)
    {
        if(path[i] == DIR_WRONG_SEP_CHAR)
            path[i] = DIR_SEP_CHAR;
    }
}

#ifdef UNIX
/**
 * If the path begins with a tilde, replace it with either the value
 * of the HOME environment variable or a user's home directory (from
 * passwd).
 *
 * @param str  Path to expand. Overwritten by the result.
 * @param len  Maximum length of str.
 */
void Dir_ExpandHome(char* str, size_t len)
{
    ddstring_t      *buf = NULL;

    if(str[0] != '~')
        return;

    buf = Str_New();

    if(str[1] == '/')
    {
        // Replace it with the HOME environment variable.
        Str_Set(buf, getenv("HOME"));
        if(Str_RAt(buf, 0) != '/')
            Str_Append(buf, "/");

        // Append the rest of the original path.
        Str_Append(buf, str + 2);
    }
    else
    {
        char userName[PATH_MAX], *end = NULL;
        struct passwd *pw;

        end = strchr(str + 1, '/');
        strncpy(userName, str, end - str - 1);
        userName[end - str - 1] = 0;

        if((pw = getpwnam(userName)) != NULL)
        {
            Str_Set(buf, pw->pw_dir);
            if(Str_RAt(buf, 0) != '/')
                Str_Append(buf, "/");
        }

        Str_Append(buf, str + 1);
    }

    // Replace the original.
    str[len - 1] = 0;
    strncpy(str, Str_Text(buf), len - 1);

    Str_Free(buf);
}
#endif

/**
 * Appends a backslash, if necessary. Also converts forward slashes into
 * backward ones. Does not check if the directory actually exists, just
 * that it's a well-formed path name.
 */
void Dir_ValidDir(char* str, size_t len)
{
    char*               end;

    if(!len)
        return; // Nothing to do.

    Dir_FixSlashes(str, len);

    // Remove whitespace from the end.
    end = str + strlen(str) - 1;
    while(end >= str && isspace(*end))
        end--;
    memset(end + 1, 0, len - (end - str) - 2);

    // Make sure it ends in a directory separator character.
    if(*end != DIR_SEP_CHAR)
        strncat(end + 1, DIR_SEP_STR, len - (end - str) - 2);

#ifdef UNIX
    Dir_ExpandHome(str, len);
#endif
}

/**
 * Converts a possibly relative path to a full path.
 */
void Dir_MakeAbsolute(char* path, size_t len)
{
    filename_t          buf;

    _fullpath(buf, path, FILENAME_T_MAXLEN);
    strncpy(path, buf, len);
}
