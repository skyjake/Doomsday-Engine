/**
 * @file library.cpp
 * Dynamic libraries. @ingroup base
 *
 * @authors Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifdef UNIX
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  include <dirent.h>
#  include <dlfcn.h>
#  include <string.h>
#  include <stdbool.h>
#endif

#include "de_base.h"
#include "de_filesys.h"
#include "m_misc.h"

#include <de/App>
#include <de/NativeFile>
#include <de/LibraryFile>

#ifdef WIN32
#  include "de_platform.h"
#endif

#define MAX_LIBRARIES   64  /// @todo  Replace with a dynamic list.

#ifdef UNIX
typedef void* handle_t;
#endif
#ifdef WIN32
typedef HINSTANCE handle_t;
#endif

struct library_s {
    ddstring_t* path;
    handle_t handle;
    boolean isGamePlugin;
};

/*
#ifdef UNIX
static filename_t appDir; /// @todo  Use ddstring_t
#endif
*/

static ddstring_t* lastError;
static Library* loadedLibs[MAX_LIBRARIES];

/*
#ifdef UNIX
static void getBundlePath(char* path, size_t len)
{
    if(CommandLine_CheckWith("-libdir", 1))
    {
        strncpy(path, CommandLine_Next(), len);
        return;
    }

    if(CommandLine_CheckWith("-appdir", 1))
    {
        dd_snprintf(path, len, "%s/%s", appDir, CommandLine_Next());
        return;
    }

#ifdef UNIX
# ifdef DENG_LIBRARY_DIR
    strncpy(path, DENG_LIBRARY_DIR, len);
# else
    // Assume they are in the cwd.
    strncpy(path, appDir, len);
# endif

    // Check Unix-specific config files.
    UnixInfo_GetConfigValue("paths", "libdir", path, len);
#endif
}
#endif
*/

static void addToLoaded(Library* lib)
{
    int i;
    for(i = 0; i < MAX_LIBRARIES; ++i)
    {
        if(!loadedLibs[i])
        {
            loadedLibs[i] = lib;
            return;
        }
    }
    assert(false);
}

static void removeFromLoaded(Library* lib)
{
    int i;
    for(i = 0; i < MAX_LIBRARIES; ++i)
    {
        if(loadedLibs[i] == lib)
        {
            loadedLibs[i] = 0;
            return;
        }
    }
    assert(false);
}

void Library_Init(void)
{
    lastError = Str_NewStd();
    /*
#ifdef UNIX
    if(!getcwd(appDir, sizeof(appDir)))
    {
        strcpy(appDir, "");
    }
#endif
    */
}

void Library_Shutdown(void)
{
    Str_Delete(lastError); lastError = 0;

    /// @todo  Unload all remaining libraries?
}

void Library_ReleaseGames(void)
{
#ifdef UNIX
    int i;

    for(i = 0; i < MAX_LIBRARIES; ++i)
    {
        Library* lib = loadedLibs[i];
        if(!lib) continue;
        if(lib->isGamePlugin && lib->handle)
        {
            LegacyCore_PrintfLogFragmentAtLevel(DE2_LOG_DEBUG,
                    "Library_ReleaseGames: Closing '%s'\n", Str_Text(lib->path));

            dlclose(lib->handle);
            lib->handle = 0;
        }
    }
#endif
}

#ifdef UNIX
static void reopenLibraryIfNeeded(Library* lib)
{
    assert(lib);
    if(!lib->handle)
    {
        LegacyCore_PrintfLogFragmentAtLevel(DE2_LOG_DEBUG,
                "reopenLibraryIfNeeded: Opening '%s'\n", Str_Text(lib->path));

        lib->handle = dlopen(Str_Text(lib->path), RTLD_NOW);
        assert(lib->handle);
    }
}
#endif

Library* Library_New(const char *filePath)
{
    Library* lib = 0;
    handle_t handle = 0;
    /*
#ifdef UNIX
    filename_t bundlePath; /// @todo Use ddstring_t
#ifdef MACOSX
    char* ptr;
#endif

    getBundlePath(bundlePath, FILENAME_T_MAXLEN);
    if(bundlePath[strlen(bundlePath) - 1] != '/')
        M_StrCat(bundlePath, "/", FILENAME_T_MAXLEN);

#ifdef MACOSX
    M_StrCat(bundlePath, fileName, FILENAME_T_MAXLEN);
    M_StrCat(bundlePath, "/", FILENAME_T_MAXLEN);
#endif

    M_StrCat(bundlePath, fileName, FILENAME_T_MAXLEN);

#ifdef MACOSX
    { const char* ext = F_FindFileExtension(bundlePath);
        if(ext && stricmp(ext, "dylib") && stricmp(ext, "bundle")) {
            // Not a dynamic library... We already know this will fail.
            Str_Set(lastError, "not a dynamic library");
            return NULL;
    }}
    // Get rid of the ".bundle" in the end.
    if(NULL != (ptr = strrchr(bundlePath, '.')))
        *ptr = '\0';
#endif
    */

    Str_Clear(lastError);

#ifdef UNIX
    handle = dlopen(filePath, RTLD_NOW);
    if(!handle)
    {
        Str_Set(lastError, dlerror());
        LOG_WARNING("Library_New: Error opening \"%s\" (%s).") << filePath << Library_LastError();
        return 0;
    }
#endif

#ifdef WIN32
    handle = LoadLibrary(WIN_STRING(filePath));
    if(!handle)
    {
        Str_Set(lastError, DD_Win32_GetLastErrorMessage());
        LOG_WARNING("Library_New: Error opening \"%s\" (%s).") << filePath << Library_LastError();
        return 0;
    }
#endif

    // Create the Library instance.
    lib = (Library*) calloc(1, sizeof(*lib));
    lib->handle = handle;
    lib->path = Str_NewStd();
#ifdef UNIX
    Str_Set(lib->path, filePath);
#endif
#ifdef WIN32
    Str_Set(lib->path, filePath);
#endif

    addToLoaded(lib);

    // Symbols from game plugins conflict with each other, so we have to
    // keep track of them.
    /// @todo  Needs a more generic way to detect the type of plugin
    ///        (see de::Library in libdeng2).
    if(Library_Symbol(lib, "G_RegisterGames"))
    {
        lib->isGamePlugin = true;
    }

    return lib;
}

void Library_Delete(Library *lib)
{
    assert(lib);
    if(lib->handle)
    {
#ifdef UNIX
        dlclose(lib->handle);
#endif
#ifdef WIN32
        FreeLibrary(lib->handle);
#endif
    }
    Str_Delete(lib->path);
    removeFromLoaded(lib);
    free(lib);
}

void* Library_Symbol(Library* lib, const char* symbolName)
{
    void* ptr = 0;

    assert(lib);
#ifdef UNIX
    reopenLibraryIfNeeded(lib);
    ptr = dlsym(lib->handle, symbolName);
    if(!ptr)
    {
        Str_Set(lastError, dlerror());
    }
#endif
#ifdef WIN32
    ptr = (void*)GetProcAddress(lib->handle, symbolName);
    if(!ptr)
    {
        Str_Set(lastError, DD_Win32_GetLastErrorMessage());
    }
#endif
    return ptr;
}

const char* Library_LastError(void)
{
    return Str_Text(lastError);
}

/*
#ifdef UNIX
static boolean isPossiblyLibraryFile(const char* path, const struct dirent* entry)
{
    if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) return false;

#ifdef MACOSX
    DENG_UNUSED(path);

    // Mac plugins are bundled in a subdir.
    if(entry->d_type != DT_REG &&
       entry->d_type != DT_DIR &&
       entry->d_type != DT_LNK) return false;
#else
    {
        struct stat st;
        AutoStr* fn = AutoStr_FromText(path);
        Str_Appendf(fn, "/%s", entry->d_name);
        if(lstat(Str_Text(fn), &st)) return false; // stat failed...
        // Only regular files and symlinks are considered.
        if(!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)) return false;
    }
#endif

    // Could be a shared library...
    return true;
}
#endif
*/

int Library_IterateAvailableLibraries(int (*func)(const char*, const char *, void *), void *data)
{
    const de::FS::Index& libs = DENG2_APP->fileSystem().indexFor(DENG2_TYPE_NAME(de::LibraryFile));

    DENG2_FOR_EACH_CONST(de::FS::Index, i, libs)
    {
        // For now we are not using libdeng2 to actually load the library.
        de::LibraryFile* lib = static_cast<de::LibraryFile*>(i->second);
        const de::NativeFile* src = dynamic_cast<const de::NativeFile*>(lib->source());
        if(src)
        {
            int result = func(src->name().toUtf8().constData(),
                              src->nativePath().toUtf8().constData(), data);
            if(result) return result;
        }
    }

    /*
#ifdef UNIX
    struct dirent* entry = NULL;
    filename_t bundlePath;
    DIR* dir = NULL;

    // This is the default location where bundles are.
    getBundlePath(bundlePath, FILENAME_T_MAXLEN);

    dir = opendir(bundlePath);
    if(!dir)
    {
        Str_Set(lastError, strerror(errno));
        printf("Library_IterateAvailableLibraries: Error opening \"%s\" (%s).\n",
               bundlePath, Library_LastError());
        return 0;
    }

    while((entry = readdir(dir)))
    {
        if(!isPossiblyLibraryFile(bundlePath, entry)) continue;
        if(func(entry->d_name, data)) break;
    }
    closedir(dir);
#endif

#ifdef WIN32
    printf("TODO: a similar routine exists in dd_winit.c; move the code here\n");
#endif
*/
    return 0;
}
