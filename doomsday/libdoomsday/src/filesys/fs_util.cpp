/** @file fs_util.cpp
 *
 * Miscellaneous file system utility routines.
 *
 * @ingroup fs
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#define DENG_NO_API_MACROS_FILESYS

#ifdef WIN32
#  include <direct.h>
#  include <io.h>
#endif

#ifdef UNIX
#  include <pwd.h>
#  include <limits.h>
#  include <unistd.h>
#  include <errno.h>
#endif

#include <cctype>
#include <sys/types.h>
#include <sys/stat.h>

#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/fs_util.h"
#include "doomsday/filesys/sys_direc.h"
#include "doomsday/filesys/lumpindex.h"
#include "doomsday/paths.h"

#include <de/Log>
#include <de/NativePath>
#include <de/findfile.h>

using namespace de;

int F_FileExists(char const *path)
{
    int result = -1;
    if(path && path[0])
    {
        ddstring_t buf;
        // Normalize the path into one we can process.
        Str_Init(&buf); Str_Set(&buf, path);
        Str_Strip(&buf);
        F_ExpandBasePath(&buf, &buf);
        F_ToNativeSlashes(&buf, &buf);

        result = !access(Str_Text(&buf), 4); // Read permission?

        Str_Free(&buf);
    }
    return result;
}

uint F_GetLastModified(char const *path)
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

dd_bool F_MakePath(char const *path)
{
#if !defined(WIN32) && !defined(UNIX)
#  error F_MakePath has no implementation for this platform.
#endif

    ddstring_t full, buf;
    char* ptr, *endptr;
    dd_bool result;

    // Convert all backslashes to normal slashes.
    Str_Init(&full); Str_Set(&full, path);
    Str_Strip(&full);
    F_ToNativeSlashes(&full, &full);

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
        endptr = strchr(ptr, DENG_DIR_SEP_CHAR);
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
        Str_AppendChar(&buf, DENG_DIR_SEP_CHAR);
        ptr = endptr + 1;
    } while(NULL != endptr);

    result = (0 == access(Str_Text(&full), 0));
    Str_Free(&buf);
    Str_Free(&full);
    return result;
}

dd_bool F_FixSlashes(ddstring_t* dstStr, const ddstring_t* srcStr)
{
    dd_bool result = false;
    assert(dstStr && srcStr);

    if(!Str_IsEmpty(srcStr))
    {
        char* dst = Str_Text(dstStr);
        const char* src = Str_Text(srcStr);
        size_t i;

        if(dstStr != srcStr)
        {
            Str_Clear(dstStr);
            Str_Reserve(dstStr, Str_Length(srcStr));
        }

        for(i = 0; src[i]; ++i)
        {
            if(src[i] != '\\')
            {
                if(dstStr != srcStr)
                    Str_AppendChar(dstStr, src[i]);
                continue;
            }

            if(dstStr != srcStr)
                Str_AppendChar(dstStr, '/');
            else
                dst[i] = '/';
            result = true;
        }
    }
    return result;
}

/**
 * Appends a slash at the end of @a pathStr if there isn't one.
 * @return @c true if a slash was appended, @c false otherwise.
 */
#ifdef UNIX
static bool F_AppendMissingSlash(ddstring_t *pathStr)
{
    if(Str_RAt(pathStr, 0) != '/')
    {
        Str_AppendChar(pathStr, '/');
        return true;
    }
    return false;
}
#endif

dd_bool F_AppendMissingSlashCString(char* path, size_t maxLen)
{
    if(path[strlen(path) - 1] != '/')
    {
        M_StrCat(path, "/", maxLen);
        return true;
    }
    return false;
}

dd_bool F_ToNativeSlashes(ddstring_t* dstStr, const ddstring_t* srcStr)
{
    dd_bool result = false;
    assert(dstStr && srcStr);

    if(!Str_IsEmpty(srcStr))
    {
        char* dst = Str_Text(dstStr);
        const char* src = Str_Text(srcStr);
        size_t i;

        if(dstStr != srcStr)
        {
            Str_Clear(dstStr);
            Str_Reserve(dstStr, Str_Length(srcStr));
        }

        for(i = 0; src[i]; ++i)
        {
            if(src[i] != DENG_DIR_WRONG_SEP_CHAR)
            {
                if(dstStr != srcStr)
                    Str_AppendChar(dstStr, src[i]);
                continue;
            }

            if(dstStr != srcStr)
                Str_AppendChar(dstStr, DENG_DIR_SEP_CHAR);
            else
                dst[i] = DENG_DIR_SEP_CHAR;
            result = true;
        }
    }
    return result;
}

/**
 * @return  @c true iff the path can be made into a relative path.
 */
static bool F_IsRelativeToBase(char const *path, char const *base)
{
    DENG_ASSERT(path != 0 && base != 0);
    return !qstrnicmp(path, base, strlen(base));
}

/**
 * Attempt to remove the base path if found at the beginning of the path.
 *
 * @param dst  Potential base-relative path written here.
 * @param src  Possibly absolute path.
 *
 * @return  @c true iff the base path was found and removed.
 */
static dd_bool F_RemoveBasePath(ddstring_t *dst, ddstring_t const *absPath)
{
    DENG_ASSERT(dst != 0 && absPath != 0);

    ddstring_t basePath;
    Str_InitStatic(&basePath, DD_BasePath());

    if(F_IsRelativeToBase(Str_Text(absPath), Str_Text(&basePath)))
    {
        dd_bool mustCopy = (dst == absPath);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf);
            Str_PartAppend(&buf, Str_Text(absPath), Str_Length(&basePath), Str_Length(absPath) - Str_Length(&basePath));
            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
            return true;
        }

        Str_Clear(dst);
        Str_PartAppend(dst, Str_Text(absPath), Str_Length(&basePath), Str_Length(absPath) - Str_Length(&basePath));
        return true;
    }

    // Do we need to copy anyway?
    if(dst != absPath)
    {
        Str_Set(dst, Str_Text(absPath));
    }

    // This doesn't appear to be the base path.
    return false;
}

dd_bool F_IsAbsolute(const ddstring_t* str)
{
    if(!str)
        return false;
    /// @todo Should not handle both separators - refactor callers.
    if(Str_At(str, 0) == DENG_DIR_SEP_CHAR || Str_At(str, 0) == DENG_DIR_WRONG_SEP_CHAR || Str_At(str, 1) == ':')
        return true;
#ifdef UNIX
    if(Str_At(str, 0) == '~')
        return true;
#endif
    return false;
}

dd_bool F_PrependBasePath2(ddstring_t* dst, const ddstring_t* src, const ddstring_t* base)
{
    assert(dst && src && base);

    if(!F_IsAbsolute(src))
    {
        if(dst != src)
            Str_Set(dst, Str_Text(src));
        Str_Prepend(dst, Str_Text(base));
        return true;
    }

    // Do we need to copy anyway?
    if(dst != src)
        Str_Set(dst, Str_Text(src));

    return false; // Not done.
}

dd_bool F_PrependBasePath(ddstring_t* dst, const ddstring_t* src)
{
    ddstring_t base;
    Str_InitStatic(&base, DD_BasePath());
    return F_PrependBasePath2(dst, src, &base);
}

dd_bool F_ExpandBasePath(ddstring_t* dst, const ddstring_t* src)
{
    assert(dst && src);
    if(Str_At(src, 0) == '>' || Str_At(src, 0) == '}')
    {
        dd_bool mustCopy = (dst == src);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf); Str_Set(&buf, DD_BasePath());
            Str_PartAppend(&buf, Str_Text(src), 1, Str_Length(src)-1);
            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
            return true;
        }

        Str_Set(dst, DD_BasePath());
        Str_PartAppend(dst, Str_Text(src), 1, Str_Length(src)-1);
        return true;
    }
#ifdef UNIX
    else if(Str_At(src, 0) == '~')
    {
        if(Str_At(src, 1) == '/' && getenv("HOME"))
        {   // Replace it with the HOME environment variable.
            ddstring_t buf;
            ddstring_t homeStr;
            Str_Init(&buf);
            Str_Init(&homeStr);

            Str_Set(&homeStr, getenv("HOME"));
            F_FixSlashes(&buf, &homeStr);
            F_AppendMissingSlash(&buf);

            // Append the rest of the original path.
            Str_PartAppend(&buf, Str_Text(src), 2, Str_Length(src)-2);

            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
            Str_Free(&homeStr);
            return true;
        }

        // Parse the user's home directory (from passwd).
        { dd_bool result = false;
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
                ddstring_t pwStr;
                Str_Init(&pwStr);
                Str_Set(&pwStr, pw->pw_dir);
                F_FixSlashes(&buf, &pwStr);
                F_AppendMissingSlash(&buf);
                result = true;
                Str_Free(&pwStr);
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

/// @return  @c true if @a path begins with a known directive.
static dd_bool pathHasDirective(const char* path)
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

    static ddstring_t buffers[NUM_BUFS]; // @todo: never free'd!
    static uint index = 0;
    ddstring_t* buf = NULL;
    int len;

    if(!path || 0 == (len = (int)strlen(path)))
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
    if(F_IsRelativeToBase(path, DD_BasePath()))
    {
        if(!buf)
        {
            buf = &buffers[index++ % NUM_BUFS];
            Str_Set(buf, path);
        }
        F_RemoveBasePath(buf, buf);
        path = Str_Text(buf);
    }

    // Swap directory separators with their system-specific version.
    if(strchr(path, DENG_DIR_WRONG_SEP_CHAR))
    {
        int i;
        if(!buf)
        {
            buf = &buffers[index++ % NUM_BUFS];
            Str_Set(buf, path);
        }
        for(i = 0; i < len; ++i)
        {
            if(buf->str[i] == DENG_DIR_WRONG_SEP_CHAR)
               buf->str[i] = DENG_DIR_SEP_CHAR;
        }
        path = Str_Text(buf);
    }

    return path;

#undef NUM_BUFS
}

dd_bool F_Dump(void const *data, size_t size, char const *path)
{
    DENG2_ASSERT(data != 0 && path != 0);

    if(!size) return false;

    AutoStr* nativePath = AutoStr_NewStd();
    Str_Set(nativePath, path);
    F_ToNativeSlashes(nativePath, nativePath);

    FILE *outFile = fopen(Str_Text(nativePath), "wb");
    if(!outFile)
    {
        LOG_RES_WARNING("Failed to open \"%s\" for writing: %s")
                << F_PrettyPath(Str_Text(nativePath)) << strerror(errno);
        return false;
    }

    fwrite(data, 1, size, outFile);
    fclose(outFile);
    return true;
}

dd_bool F_DumpFile(de::File1 &file, char const *outputPath)
{
    String dumpPath = ((!outputPath || !outputPath[0])? file.name() : String(outputPath));
    QByteArray dumpPathUtf8 = dumpPath.toUtf8();
    bool dumpedOk = F_Dump(file.cache(), file.info().size, dumpPathUtf8.constData());
    if(dumpedOk)
    {
        LOG_RES_VERBOSE("%s dumped to \"%s\"") << file.name() << NativePath(dumpPath).pretty();
    }
    file.unlock();
    return dumpedOk;
}
