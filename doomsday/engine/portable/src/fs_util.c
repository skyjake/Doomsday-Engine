/**\file fs_util.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * File System Utility Routines.
 */

#ifdef WIN32
#  include <direct.h>
#endif

#ifdef UNIX
#  include <pwd.h>
#  include <limits.h>
#  include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include "de_base.h"
#include "de_filesys.h"

void F_FileDir(ddstring_t* dst, const ddstring_t* src)
{
    assert(NULL != dst && NULL != src);
    {
    /// \fixme Potentially truncates @a src to FILENAME_T_MAXLEN
    directory_t* dir = Dir_ConstructFromPathDir(Str_Text(src));
    Str_Set(dst, Dir_Path(dir));
    Dir_Delete(dir);
    }
}

void F_FileName(ddstring_t* dst, const char* src)
{
#ifdef WIN32
    char name[_MAX_FNAME];
#else
    char name[NAME_MAX];
#endif
    _splitpath(src, 0, 0, name, 0);
    Str_Set(dst, name);
}

void F_FileNameAndExtension(ddstring_t* dst, const char* src)
{
#ifdef WIN32
    char name[_MAX_FNAME], ext[_MAX_EXT];
#else
    char name[NAME_MAX], ext[NAME_MAX];
#endif
    _splitpath(src, 0, 0, name, ext);
    Str_Clear(dst);
    Str_Appendf(dst, "%s%s", name, ext);
}

int F_FileExists(const char* path)
{
    int result = -1;
    if(NULL != path && path[0])
    {
        ddstring_t buf;
        // Normalize the path into one we can process.
        Str_Init(&buf); Str_Set(&buf, path);
        Str_Strip(&buf);
        F_FixSlashes(&buf, &buf);
        F_ExpandBasePath(&buf, &buf);

        result = !access(Str_Text(&buf), 4); // Read permission?

        Str_Free(&buf);
    }
    return result;
}

uint F_LastModified(const char* path)
{
#ifdef UNIX
    struct stat s;
    stat(path, &s);
    return s.st_mtime;
#endif

#ifdef WIN32
    struct _stat s;
    _stat(path, &s);
    return s.st_mtime;
#endif
}

boolean F_MakePath(const char* path)
{
#if !defined(WIN32) && !defined(UNIX)
#  error F_MakePath has no implementation for this platform.
#endif

    ddstring_t full, buf;
    char* ptr, *endptr;
    boolean result;

    // Convert all backslashes to normal slashes.
    Str_Init(&full); Str_Set(&full, path);
    Str_Strip(&full);
    F_FixSlashes(&full, &full);

    // Does this path already exist?
    if(0 == access(Str_Text(&full), 0))
    {
        Str_Free(&full);
        return true;
    }

    // Check and create the path in segments.
    ptr = Str_Text(&full);
    Str_Init(&buf);
    do
    {
        endptr = strchr(ptr, DIR_SEP_CHAR);
        if(!endptr)
            Str_Append(&buf, ptr);
        else
            Str_PartAppend(&buf, ptr, 0, endptr - ptr);
        if(0 != access(Str_Text(&buf), 0))
        {
            // Path doesn't exist, create it.
#ifdef WIN32
            mkdir(Str_Text(&buf));
#elif UNIX
            mkdir(Str_Text(&buf), 0775);
#endif
        }
        Str_AppendChar(&buf, DIR_SEP_CHAR);
        ptr = endptr + 1;
    } while(NULL != endptr);

    result = (0 == access(Str_Text(&full), 0));
    Str_Free(&buf);
    Str_Free(&full);
    return result;
}

boolean F_FixSlashes(ddstring_t* dstStr, const ddstring_t* srcStr)
{
    assert(dstStr && srcStr);
    {
    boolean result = false;
    if(!Str_IsEmpty(srcStr))
    {
        char* dst = Str_Text(dstStr);
        const char* src = Str_Text(srcStr);

        if(dstStr != srcStr)
        {
            Str_Clear(dstStr);
            Str_Reserve(dstStr, Str_Length(srcStr));
        }

        { size_t i;
        for(i = 0; src[i]; ++i)
        {
            if(src[i] != DIR_WRONG_SEP_CHAR)
            {
                if(dstStr != srcStr)
                    Str_AppendChar(dstStr, src[i]);
                continue;
            }

            if(dstStr != srcStr)
                Str_AppendChar(dstStr, DIR_SEP_CHAR);
            else
                dst[i] = DIR_SEP_CHAR;
            result = true;
        }}
    }
    return result;
    }
}

const char* F_FindFileExtension(const char* path)
{
    if(path && path[0])
    {
        size_t len = strlen(path);
        const char* p = path + len - 1;
        if(p - path > 1 && *p != DIR_SEP_CHAR && *p != DIR_WRONG_SEP_CHAR)
        {
            do
            {
                if(*(p - 1) == DIR_SEP_CHAR ||
                   *(p - 1) == DIR_WRONG_SEP_CHAR)
                    break;
                if(*p == '.')
                    return (unsigned) (p - path) < len - 1? p + 1 : NULL;
            } while(--p > path);
        }
    }
    return NULL; // Not found.
}

void F_ExtractFileBase2(char* dest, const char* path, size_t max, int ignore)
{
    const char* src = path + strlen(path) - 1;

    // Back up until a \ or the start.
    while(src != path && *(src - 1) != '\\' && *(src - 1) != '/')
    {
        src--;
    }

    while(*src && *src != '.' && max-- > 0)
    {
        if(ignore-- > 0)
        {
            src++; // Skip chars.
            max++; // Doesn't count.
        }
        else
            *dest++ = toupper((int) *src++);
    }

    if(max > 0) // Room for a null?
    {
        // End with a terminating null.
        *dest++ = 0;
    }
}

void F_ExtractFileBase(char* dest, const char* path, size_t len)
{
    F_ExtractFileBase2(dest, path, len, 0);
}

void F_ResolveSymbolicPath(ddstring_t* dst, const ddstring_t* src)
{
    assert(dst && src);

    // Src path is base-relative?
    if(Str_At(src, 0) == DIR_SEP_CHAR)
    {
        boolean mustCopy = (dst == src);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf);
            Str_Set(&buf, ddBasePath);
            Str_PartAppend(&buf, Str_Text(src), 1, Str_Length(src)-1);
            Str_Set(dst, Str_Text(&buf));
            return;
        }

        Str_Set(dst, ddBasePath);
        Str_PartAppend(dst, Str_Text(src), 1, Str_Length(src)-1);
        return;
    }

    // Src path is workdir-relative.

    if(dst == src)
    {
        Str_Prepend(dst, ddRuntimePath);
        return;
    }

    Str_Appendf(dst, "%s%s", ddRuntimePath, Str_Text(src));
}

boolean F_IsRelativeToBasePath(const char* path)
{
    assert(path);
    return !strnicmp(path, ddBasePath, strlen(ddBasePath));
}

boolean F_RemoveBasePath(ddstring_t* dst, const ddstring_t* absPath)
{
    assert(dst && absPath);

    if(F_IsRelativeToBasePath(Str_Text(absPath)))
    {
        boolean mustCopy = (dst == absPath);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf);
            Str_PartAppend(&buf, Str_Text(absPath), (int)strlen(ddBasePath), Str_Length(absPath) - (int)strlen(ddBasePath));
            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
            return true;
        }

        Str_Clear(dst);
        Str_PartAppend(dst, Str_Text(absPath), (int)strlen(ddBasePath), Str_Length(absPath) - (int)strlen(ddBasePath));
        return true;
    }

    // Do we need to copy anyway?
    if(dst != absPath)
        Str_Set(dst, Str_Text(absPath));

    // This doesn't appear to be the base path.
    return false;
}

boolean F_IsAbsolute(const ddstring_t* str)
{
    if(!str)
        return false;

    if(Str_At(str, 0) == '\\' || Str_At(str, 0) == '/' || Str_At(str, 1) == ':')
        return true;
#ifdef UNIX
    if(Str_At(str, 0) == '~')
        return true;
#endif
    return false;
}

boolean F_PrependBasePath(ddstring_t* dst, const ddstring_t* src)
{
    assert(dst && src);

    if(!F_IsAbsolute(src))
    {
        if(dst != src)
            Str_Set(dst, Str_Text(src));
        Str_Prepend(dst, ddBasePath);
        return true;
    }

    // Do we need to copy anyway?
    if(dst != src)
        Str_Set(dst, Str_Text(src));

    return false; // Not done.
}

boolean F_PrependWorkPath(ddstring_t* dst, const ddstring_t* src)
{
    assert(dst && src);

    if(!F_IsAbsolute(src))
    {
        ddstring_t dir;
        Str_Init(&dir);
        F_FileDir(&dir, src);
        Str_Prepend(dst, Str_Text(&dir));
        Str_Free(&dir);
        return true;
    }

    // Do we need to copy anyway?
    if(dst != src)
        Str_Set(dst, Str_Text(src));

    return false; // Not done.
}

boolean F_MakeAbsolute(ddstring_t* dst, const ddstring_t* src)
{
    if(F_ExpandBasePath(dst, src)) return true;
    // src is equal to dst
    if(F_PrependBasePath(dst, dst)) return true;
    if(F_PrependWorkPath(dst, dst)) return true;
    return false; // Not done.
}

boolean F_ExpandBasePath(ddstring_t* dst, const ddstring_t* src)
{
    assert(dst && src);
    if(Str_At(src, 0) == '>' || Str_At(src, 0) == '}')
    {
        boolean mustCopy = (dst == src);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf); Str_Set(&buf, ddBasePath);
            Str_PartAppend(&buf, Str_Text(src), 1, Str_Length(src)-1);
            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
            return true;
        }

        Str_Set(dst, ddBasePath);
        Str_PartAppend(dst, Str_Text(src), 1, Str_Length(src)-1);
        return true;
    }
#ifdef UNIX
    else if(Str_At(src, 0) == '~')
    {
        if(Str_At(src, 1) == DIR_SEP_CHAR && getenv("HOME"))
        {   // Replace it with the HOME environment variable.
            ddstring_t buf;
            Str_Init(&buf);

            Str_Set(&buf, getenv("HOME"));
            if(Str_RAt(&buf, 0) != DIR_SEP_CHAR)
                Str_AppendChar(&buf, DIR_SEP_CHAR);

            // Append the rest of the original path.
            Str_PartAppend(&buf, Str_Text(src), 2, Str_Length(src)-2);

            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
            return true;
        }

        // Parse the user's home directory (from passwd).
        { boolean result = false;
        ddstring_t userName;
        const char* p = Str_Text(src)+2;

        Str_Init(&userName);
        if((p = Str_CopyDelim2(&userName, p, '/', CDF_OMIT_DELIMITER)))
        {
            ddstring_t buf;
            struct passwd* pw;

            Str_Init(&buf);
            if((pw = getpwnam(Str_Text(&userName))) != NULL)
            {
                Str_Set(&buf, pw->pw_dir);
                if(Str_RAt(&buf, 0) != DIR_SEP_CHAR)
                    Str_AppendChar(&buf, DIR_SEP_CHAR);
                result = true;
            }

            Str_Append(&buf, Str_Text(src) + 1);
            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
        }
        Str_Free(&userName);
        if(result)
            return result;
        }
    }
#endif

    // Do we need to copy anyway?
    if(dst != src)
        Str_Set(dst, Str_Text(src));

    // No expansion done.
    return false;
}

boolean F_TranslatePath(ddstring_t* dst, const ddstring_t* src)
{
    F_FixSlashes(dst, src); // Will copy src to dst if not equal.
    return F_ExpandBasePath(dst, dst);
}

/// @return  @c true if @a path begins with a known directive.
static boolean pathHasDirective(const char* path)
{
    if(NULL != path && path[0])
    {
#ifdef UNIX
        if('~' == path[0]) return true;
#endif
        if('}' == path[0] || '>' == path[0]) return true;
    }
    return false;
}

const char* F_PrettyPath(const char* path)
{
#define NUM_BUFS            8

    static ddstring_t buffers[NUM_BUFS]; // \fixme: never free'd!
    static uint index = 0;

    ddstring_t* buf = NULL;
    int len;

    if(NULL == path || 0 == (len = (int)strlen(path)))
        return path;

    // Hide relative directives like '}'
    if(len > 1 && pathHasDirective(path))
    {
        buf = &buffers[index++ % NUM_BUFS];
        Str_Clear(buf);
        Str_PartAppend(buf, path, 1, len-1);
        path = Str_Text(buf);
    }

    // If within our the base directory cut out the base path.
    if(F_IsRelativeToBasePath(path))
    {
        if(NULL == buf)
        {
            buf = &buffers[index++ % NUM_BUFS];
            Str_Set(buf, path);
        }
        F_RemoveBasePath(buf, buf);
        path = Str_Text(buf);
    }

    // Swap directory separators with their system-specific version.
    if(strchr(path, DIR_WRONG_SEP_CHAR))
    {
        if(NULL == buf)
        {
            buf = &buffers[index++ % NUM_BUFS];
            Str_Set(buf, path);
        }
        F_FixSlashes(buf, buf);
        path = Str_Text(buf);
    }

    return path;

#undef NUM_BUFS
}

int F_MatchFileName(const char* string, const char* pattern)
{
    const char* in = string, *st = pattern;

    while(*in)
    {
        if(*st == '*')
        {
            st++;
            continue;
        }

        if(*st != '?' && (tolower((unsigned char) *st) != tolower((unsigned char) *in)))
        {
            // A mismatch. Hmm. Go back to a previous '*'.
            while(st >= pattern && *st != '*')
                st--;
            if(st < pattern)
                return false; // No match!
            // The asterisk lets us continue.
        }

        // This character of the pattern is OK.
        st++;
        in++;
    }

    // Match is good if the end of the pattern was reached.
    while(*st == '*')
        st++; // Skip remaining asterisks.

    return *st == 0;
}
