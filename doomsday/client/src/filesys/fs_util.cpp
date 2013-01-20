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
#endif

#ifdef UNIX
#  include <pwd.h>
#  include <limits.h>
#  include <unistd.h>
#endif

#include <cctype>
#include <sys/types.h>
#include <sys/stat.h>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_misc.h"

#include <de/Log>
#include <de/NativePath>

using namespace de;

void F_FileDir(ddstring_t* dst, const ddstring_t* src)
{
    /// @todo Potentially truncates @a src to FILENAME_T_MAXLEN
    directory_t* dir = Dir_FromText(Str_Text(src));
    Str_Set(dst, Dir_Path(dir));
    Dir_Delete(dir);
}

void F_FileName(ddstring_t* dst, const char* src)
{
#ifdef WIN32
    char name[_MAX_FNAME];
#else
    char name[NAME_MAX];
#endif

    if(!dst) return;
    Str_Clear(dst);
    if(!src) return;
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

uint F_GetLastModified(const char* path)
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
    boolean result = false;
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

boolean F_AppendMissingSlash(ddstring_t* pathStr)
{
    if(Str_RAt(pathStr, 0) != '/')
    {
        Str_AppendChar(pathStr, '/');
        return true;
    }
    return false;
}

boolean F_AppendMissingSlashCString(char* path, size_t maxLen)
{
    if(path[strlen(path) - 1] != '/')
    {
        M_StrCat(path, "/", maxLen);
        return true;
    }
    return false;
}

boolean F_ToNativeSlashes(ddstring_t* dstStr, const ddstring_t* srcStr)
{
    boolean result = false;
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
        }
    }
    return result;
}

const char* F_FindFileExtension(const char* path)
{
    if(path && path[0])
    {
        size_t len = strlen(path);
        const char* p = path + len - 1;
        if(p - path > 1 && *p != '/')
        {
            do
            {
                if(*(p - 1) == '/') break;
                if(*p == '.')
                    return (unsigned) (p - path) < len - 1? p + 1 : NULL;
            } while(--p > path);
        }
    }
    return NULL; // Not found.
}

void F_ExtractFileBase2(char* dest, const char* path, size_t max, int ignore)
{
    const char* src;

    if(!dest || !path || max == 0)
        return;

    // Back up until a \ or the start.
    src = path + strlen(path) - 1;
    while(src != path && *(src - 1) != '\\' && *(src - 1) != '/')
    {
        src--;
    }

    max += 1;
    while(*src && *src != '.' && max-- > 1)
    {
        if(ignore-- > 0)
        {
            src++; // Skip chars.
            max++; // Doesn't count.
        }
        else
            *dest++ = toupper((int) *src++);
    }

    if(max > 1) // Room for a null?
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
    if(Str_At(src, 0) == '/')
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

boolean F_IsRelativeToBase(const char* path, const char* base)
{
    assert(path && base);
    return !strnicmp(path, base, strlen(base));
}

boolean F_RemoveBasePath2(ddstring_t* dst, const ddstring_t* absPath, const ddstring_t* basePath)
{
    assert(dst && absPath && basePath);

    if(F_IsRelativeToBase(Str_Text(absPath), Str_Text(basePath)))
    {
        boolean mustCopy = (dst == absPath);
        if(mustCopy)
        {
            ddstring_t buf;
            Str_Init(&buf);
            Str_PartAppend(&buf, Str_Text(absPath), Str_Length(basePath), Str_Length(absPath) - Str_Length(basePath));
            Str_Set(dst, Str_Text(&buf));
            Str_Free(&buf);
            return true;
        }

        Str_Clear(dst);
        Str_PartAppend(dst, Str_Text(absPath), Str_Length(basePath), Str_Length(absPath) - Str_Length(basePath));
        return true;
    }

    // Do we need to copy anyway?
    if(dst != absPath)
        Str_Set(dst, Str_Text(absPath));

    // This doesn't appear to be the base path.
    return false;
}

boolean F_RemoveBasePath(ddstring_t* dst, const ddstring_t* absPath)
{
    ddstring_t base;
    Str_InitStatic(&base, ddBasePath);
    return F_RemoveBasePath2(dst, absPath, &base);
}

boolean F_IsAbsolute(const ddstring_t* str)
{
    if(!str)
        return false;
    /// @todo Should not handle both separators - refactor callers.
    if(Str_At(str, 0) == DIR_SEP_CHAR || Str_At(str, 0) == DIR_WRONG_SEP_CHAR || Str_At(str, 1) == ':')
        return true;
#ifdef UNIX
    if(Str_At(str, 0) == '~')
        return true;
#endif
    return false;
}

boolean F_PrependBasePath2(ddstring_t* dst, const ddstring_t* src, const ddstring_t* base)
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

boolean F_PrependBasePath(ddstring_t* dst, const ddstring_t* src)
{
    ddstring_t base;
    Str_InitStatic(&base, ddBasePath);
    return F_PrependBasePath2(dst, src, &base);
}

boolean F_PrependWorkPath(ddstring_t* dst, const ddstring_t* src)
{
    assert(dst && src);

    if(!F_IsAbsolute(src))
    {
        char* curPath = Dir_CurrentPath();
        Str_Prepend(dst, curPath);
        Dir_CleanPathStr(dst);
        free(curPath);
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

/// @note Part of the Doomsday public API
boolean F_TranslatePath(ddstring_t* dst, const ddstring_t* src)
{
    F_ExpandBasePath(dst, src); // Will copy src to dst if not equal.
    return F_ToNativeSlashes(dst, dst);
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
    if(F_IsRelativeToBase(path, ddBasePath))
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
    if(strchr(path, DIR_WRONG_SEP_CHAR))
    {
        int i;
        if(!buf)
        {
            buf = &buffers[index++ % NUM_BUFS];
            Str_Set(buf, path);
        }
        for(i = 0; i < len; ++i)
        {
            if(buf->str[i] == DIR_WRONG_SEP_CHAR)
               buf->str[i] = DIR_SEP_CHAR;
        }
        path = Str_Text(buf);
    }

    return path;

#undef NUM_BUFS
}

bool F_MatchFileName(de::String const &string, de::String const &pattern)
{
    static QChar const ASTERISK('*');
    static QChar const QUESTION_MARK('?');

    QChar const *in = string.constData(), *st = pattern.constData();

    while(!in->isNull())
    {
        if(*st == ASTERISK)
        {
            st++;
            continue;
        }

        if(*st != QUESTION_MARK && st->toLower() != in->toLower())
        {
            // A mismatch. Hmm. Go back to a previous '*'.
            while(st >= pattern && *st != ASTERISK)
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
    while(*st == ASTERISK)
        st++; // Skip remaining asterisks.

    return st->isNull();
}

boolean F_Dump(void const* data, size_t size, char const* path)
{
    DENG_ASSERT(data);
    DENG_ASSERT(path);

    if(!size) return false;

    AutoStr* nativePath = AutoStr_NewStd();
    Str_Set(nativePath, path);
    F_ToNativeSlashes(nativePath, nativePath);

    FILE* outFile = fopen(Str_Text(nativePath), "wb");
    if(!outFile)
    {
        Con_Message("Warning: Failed to open \"%s\" for writing (error: %s), aborting.\n",
                    F_PrettyPath(Str_Text(nativePath)), strerror(errno));
        return false;
    }

    fwrite(data, 1, size, outFile);
    fclose(outFile);
    return true;
}

static bool dumpLump(de::File1& lump, String path)
{
    String dumpPath = path.isEmpty()? lump.name() : path;
    QByteArray dumpPathUtf8 = dumpPath.toUtf8();
    bool dumpedOk = F_Dump(lump.cache(), lump.info().size, dumpPathUtf8.constData());
    lump.unlock();
    if(!dumpedOk) return false;
    LOG_VERBOSE("%s dumped to \"%s\"") << lump.name() << NativePath(dumpPath).pretty();
    return true;
}

boolean F_DumpLump2(lumpnum_t lumpNum, char const* _path)
{
    try
    {
        de::File1& lump = App_FileSystem()->nameIndex().lump(lumpNum);
        String path = String(_path? _path : "");
        return dumpLump(lump, path);
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore error.
    return false;
}

boolean F_DumpLump(lumpnum_t lumpNum)
{
    return F_DumpLump2(lumpNum, 0);
}
