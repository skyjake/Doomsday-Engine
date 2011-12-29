/**\file library.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_filesys.h"
#include "m_misc.h"
#include "m_args.h"

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>

typedef void* handle_t;

static filename_t appDir; /// @todo  Use ddstring_t
static ddstring_t* lastError;

struct library_s {
    ddstring_t* path;
    handle_t handle;
};

static void getBundlePath(char* path, size_t len)
{
    if(ArgCheckWith("-libdir", 1))
    {
        strncpy(path, ArgNext(), len);
        return;
    }

    if(ArgCheckWith("-appdir", 1))
    {
        dd_snprintf(path, len, "%s/%s", appDir, ArgNext());
        return;
    }

#ifdef MACOSX
    // This is the default location where bundles are.
    dd_snprintf(path, len, "%s/Bundles", appDir);
#endif
#ifdef UNIX
#ifdef DENG_LIBRARY_DIR
    strncpy(path, DENG_LIBRARY_DIR, len);
#else
    // Assume they are in the cwd.
    strncpy(path, appDir, len);
#endif
#endif
}

void Library_Init(void)
{
    lastError = Str_NewStd();
    getcwd(appDir, sizeof(appDir));
}

void Library_Shutdown(void)
{
    Str_Delete(lastError); lastError = 0;

    /// @todo  Unload all remaining libraries.
}

Library* Library_New(const char *fileName)
{
    Library* lib = 0;
    handle_t handle;
    filename_t bundlePath; /// @todo Use ddstring_t
#ifdef MACOSX
    char* ptr;
#endif

    getBundlePath(bundlePath, FILENAME_T_MAXLEN);
    if(bundlePath[strlen(bundlePath) - 1] != '/')
        strncat(bundlePath, "/", FILENAME_T_MAXLEN);

#ifdef MACOSX
    strncat(bundlePath, fileName, FILENAME_T_MAXLEN);
    strncat(bundlePath, "/", FILENAME_T_MAXLEN);
#endif

    strncat(bundlePath, fileName, FILENAME_T_MAXLEN);

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
    Str_Clear(lastError);

    handle = dlopen(bundlePath, RTLD_NOW);
    if(!handle)
    {
        Str_Set(lastError, dlerror());
        printf("Library_New: Error opening \"%s\" (%s).\n", bundlePath, Library_LastError());
        return 0;
    }

    // Create the Library instance.
    lib = calloc(1, sizeof(*lib));
    lib->handle = handle;
    lib->path = Str_NewStd();
    Str_Set(lib->path, bundlePath);
    return lib;
}

void Library_Delete(Library *lib)
{
    assert(lib);
    if(lib->handle)
    {
        dlclose(lib->handle);
    }
    Str_Delete(lib->path);
    free(lib);
}

void* Library_Symbol(Library* lib, const char* symbolName)
{
    assert(lib);
    void* ptr = dlsym(lib->handle, symbolName);
    if(!ptr)
    {
        Str_Set(lastError, dlerror());
    }
    return ptr;
}

const char* Library_LastError(void)
{
    return Str_Text(lastError);
}

void Library_AddSearchDir(const char *dir)
{
    /// @todo  Implement this (and use it in the lookup)
}

int Library_IterateAvailableLibraries(int (*func)(const char *, void *), void *data)
{
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
#ifdef MACOSX
        // Mac plugins are bundled in a subdir.
        if(entry->d_type != DT_REG && entry->d_type != DT_DIR) continue;
        if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
#else
        if(entry->d_type != DT_REG) continue;
#endif
        if(func(entry->d_name, data)) break;
    }
    closedir(dir);
    return 0;
}
