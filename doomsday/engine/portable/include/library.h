/**\file library.h
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

/**
 * Dynamic Libraries.
 *
 * These functions provide roughly the same functionality as the ltdl
 * library.  Since the ltdl library appears to be broken on Mac OS X,
 * these will be used instead when loading plugin libraries.
 */

#ifndef LIBDENG_SYSTEM_UTILS_DYNAMIC_LIBRARY_H
#define LIBDENG_SYSTEM_UTILS_DYNAMIC_LIBRARY_H

struct library_s; // The library instance (opaque).
typedef struct library_s Library;

/**
 * Initializes the library loader.
 */
void Library_Init(void);

/**
 * Release all resources associated with dynamic libraries. Must be called
 * when shutting down the engine.
 */
void Library_Shutdown(void);

/**
 * Closes the library handles of all game plugins. The library will be
 * reopened automatically when needed.
 */
void Library_ReleaseGames(void);

/**
 * Defines an additional library @a dir where to look for dynamic libraries.
 */
void Library_AddSearchDir(const char* dir);

/**
 * Looks for dynamic libraries and calls @a func for each one.
 */
int Library_IterateAvailableLibraries(int (*func)(const char* fileName, void* data), void* data);

/**
 * Loads a dynamic library.
 *
 * @param fileName  Name of the library to open.
 */
Library* Library_New(const char* fileName);

void Library_Delete(Library* lib);

/**
 * Looks up a symbol from the library.
 *
 * @param symbolName  Name of the symbol.
 *
 * @return @c NULL if the symbol is not defined. Otherwise the address of
 * the symbol.
 */
void* Library_Symbol(Library* lib, const char* symbolName);

/**
 * Returns the latest error message.
 */
const char* Library_LastError(void);

#endif /* LIBDENG_SYSTEM_UTILS_DYNAMIC_LIBRARY_H */
