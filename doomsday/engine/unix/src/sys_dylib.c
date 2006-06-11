/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sys_dylib.c: Dynamic Libraries
 *
 * These functions provide roughly the same functionality as the ltdl 
 * library.  Since the ltdl library appears to be broken on Mac OS X, 
 * these will be used instead when loading plugin libraries.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "m_args.h"
#include "Unix/sys_dylib.h"

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
    if(ArgCheckWith("-appdir", 1))
    {
        sprintf(path, "%s/%s", appDir, ArgNext());
    }
	else
    {
        // This is the default location where bundles are.
        sprintf(path, "%s/Bundles", appDir);
    }
}

int lt_dlforeachfile(const char *searchPath, 
                     int (*func) (const char *fileName, lt_ptr data), 
					 lt_ptr data)
{
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	filename_t bundlePath;

	// This is the default location where bundles are.
	getBundlePath(bundlePath);

	if(searchPath == NULL)
		searchPath = bundlePath;
	
	dir = opendir(searchPath);
	while((entry = readdir(dir)) != NULL)
	{
		if(entry->d_type == DT_DIR && 
		   !strncmp(entry->d_name, "dp", 2))
		{
			if(func(entry->d_name, data))
				break;
		}
	}
	closedir(dir);
	return 0;
}

/*
 * The base file name should have the ".bundle" file name extension.
 */
lt_dlhandle lt_dlopenext(const char *baseFileName)
{
    lt_dlhandle handle;
	filename_t bundleName;
	char* ptr;
	
	getBundlePath(bundleName);
	strcat(bundleName, "/");
	strcat(bundleName, baseFileName);
	strcat(bundleName, "/Contents/MacOS/");
	strcat(bundleName, baseFileName);
/*  sprintf(bundleName, "Bundles/%s/Contents/MacOS/%s", baseFileName, 
            baseFileName);*/
	
	// Get rid of the ".bundle" in the end.
	if((ptr = strrchr(bundleName, '.')) != NULL)
		*ptr = 0;
	
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

void lt_dlclose(lt_dlhandle module)
{
	dlclose(module);
}
