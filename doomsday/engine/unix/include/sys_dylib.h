/**\file sys_dylib.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * Dynamic Libraries.
 *
 * These functions provide roughly the same functionality as the ltdl
 * library.  Since the ltdl library appears to be broken on Mac OS X,
 * these will be used instead when loading plugin libraries.
 */

#ifndef LIBDENG_SYSTEM_UTILS_UDYNAMIC_LIBRARY_H
#define LIBDENG_SYSTEM_UTILS_UDYNAMIC_LIBRARY_H

typedef void* lt_dlhandle;
typedef void* lt_ptr;

void lt_dlinit(void);
void lt_dlexit(void);
const char* lt_dlerror(void);
void lt_dladdsearchdir(const char* searchPath);
int lt_dlforeachfile(const char* searchPath, int (*func) (const char* fileName, lt_ptr data), lt_ptr data);

/**
 * @param libraryName  Name of the library to open (should have the ".bundle" extension).
 */
lt_dlhandle lt_dlopenext(const char* baseFileName);

void* lt_dlsym(lt_dlhandle module, const char* symbolName);
int lt_dlclose(lt_dlhandle module);

#endif /* LIBDENG_SYSTEM_UTILS_UDYNAMIC_LIBRARY_H */
