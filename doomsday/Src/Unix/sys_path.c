/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * sys_path.c: File Path Processing
 */

// HEADER FILES ------------------------------------------------------------

#include <unistd.h>
#include <string.h>

#include "de_base.h"
#include "de_console.h"
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

/*
 * Removes references to the current (.) and parent (..) directories.
 * The given path should be an absolute path.
 */
void DD_ResolvePath(char *path)
{
	char *ch   = path;
	char *end  = path + strlen(path);
	char *prev = path; // Assume an absolute path.

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
		if(*ch == '/') prev = ch;
	}
}

/*
 * Convert the given path to an absolute path.
 */
char *_fullpath(char *full, const char *original, int maxLen)
{
	ddstring_t dir;
	char workDir[512];		// Fixed-size array...

	Str_Init(&dir);

	// FIXME: Check for '~'.
	
	if(original[0] != '/')	// A relative path?
	{
		getcwd(workDir, sizeof(workDir)); // FIXME: Check for ERANGE.
		Str_Set(&dir, workDir);
		Str_Append(&dir, "/");
		Str_Append(&dir, original);
	}
	else
	{
		Str_Set(&dir, original);
	}

	// Remove "."s and ".."s.
	DD_ResolvePath(Str_Text(&dir));

	// Clear the given buffer and copy the full path there.
	memset(full, 0, maxLen);
	strncpy(full, Str_Text(&dir), maxLen - 1);
	Str_Free(&dir);
	return full;
}

void strzncpy(char *dest, const char *src, int count)
{
	char *out = dest;
	const char *in = src;

	while(count-- > 0)
	{
		*out++ = *in++;
		if(!*in) break;
	}
	*out = 0;
}

void _splitpath(const char *path, char *drive, char *dir, char *name,
				char *ext)
{
	char *lastPeriod, *lastSlash;
	
	if(drive) strcpy(drive, ""); // There is never a drive letter.

	lastPeriod = strrchr(path, '.');
	lastSlash  = strrchr(path, '/');
	if(lastPeriod < lastSlash) lastPeriod = NULL;

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
