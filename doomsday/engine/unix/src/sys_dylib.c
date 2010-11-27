/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * sys_dylib.c: Dynamic Libraries
 *
 * These functions provide roughly the same functionality as the ltdl
 * library.  Since the ltdl library appears to be broken on Mac OS X,
 * these will be used instead when loading plugin libraries.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "m_args.h"
#include "sys_dylib.h"

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static filename_t appDir;

// CODE --------------------------------------------------------------------

void lt_dlinit(void)
{
    getcwd(appDir, sizeof(appDir));
}

void lt_dlexit(void)
{
}

const char *lt_dlerror(void)
{
    return dlerror();
}

void lt_dladdsearchdir(const char *searchPath)
{
}

static void getBundlePath(char *path)
{
    if(ArgCheckWith("-libdir", 1))
    {
        strcpy(path, ArgNext());
    }
    else if(ArgCheckWith("-appdir", 1))
    {
        sprintf(path, "%s/%s", appDir, ArgNext());
    }
    else
    {
#ifdef MACOSX
        // This is the default location where bundles are.
        sprintf(path, "%s/Bundles", appDir);
#endif
#ifdef UNIX
#ifdef DENG_LIBRARY_DIR
        strcpy(path, DENG_LIBRARY_DIR);
#else
        // Assume they are in the cwd.
        strcpy(path, appDir);
#endif
#endif
    }
}

int lt_dlforeachfile(const char *searchPath,
                     int (*func) (const char *fileName, lt_ptr data),
                     lt_ptr data)
{
    DIR        *dir = NULL;
    struct dirent *entry = NULL;
    filename_t  bundlePath;

    // This is the default location where bundles are.
    getBundlePath(bundlePath);

    if(searchPath == NULL)
        searchPath = bundlePath;

    dir = opendir(searchPath);
    while((entry = readdir(dir)) != NULL)
    {
#ifndef MACOSX
        if(entry->d_type != DT_DIR &&
           !strncmp(entry->d_name, "libdp", 5))
#endif
#ifdef MACOSX
        if(entry->d_type == DT_DIR &&
           !strncmp(entry->d_name, "dp", 2))
#endif

        {

            if(func(entry->d_name, data))
                break;
        }

    }
    closedir(dir);
    return 0;
}

/**
 * The base file name should have the ".bundle" file name extension.
 */
lt_dlhandle lt_dlopenext(const char *baseFileName)
{
    lt_dlhandle handle;
    filename_t  bundleName;
#ifdef MACOSX
    char* ptr;
#endif

    getBundlePath(bundleName);
#ifdef MACOSX
    strcat(bundleName, "/");
    strcat(bundleName, baseFileName);
    strcat(bundleName, "/Contents/MacOS/");
#endif
    strcat(bundleName, baseFileName);
//#ifdef UNIX
//#ifndef MACOSX
//  strcat(bundleName, ".so");
//#endif
//#endif


/*  sprintf(bundleName, "Bundles/%s/Contents/MacOS/%s", baseFileName,
            baseFileName);*/
#ifdef MACOSX
    // Get rid of the ".bundle" in the end.
    if((ptr = strrchr(bundleName, '.')) != NULL)
        *ptr = 0;
#endif
    handle = dlopen(bundleName, RTLD_NOW);
    if(!handle)
    {
        printf("While opening dynamic library\n%s:\n  %s\n", bundleName, dlerror());
    }
    return handle;
}

lt_ptr lt_dlsym(lt_dlhandle module, const char *symbolName)
{
    return dlsym(module, symbolName);
}

int lt_dlclose(lt_dlhandle module)
{
    if(!module)
        return 1;
    dlclose(module);
}
