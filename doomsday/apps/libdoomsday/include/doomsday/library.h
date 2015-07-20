/**
 * @file library.h
 * Dynamic libraries. @ingroup base
 *
 * These functions provide roughly the same functionality as the ltdl library.
 * Since the ltdl library appears to be broken on Mac OS X, these will be used
 * instead when loading plugin libraries.
 *
 * During startup Doomsday loads multiple game plugins. However, only one can
 * exist in memory at a time because they contain many of the same globally
 * visible symbols. When a game is started, all game plugins are first released
 * from memory after which the chosen game plugin is reloaded (see
 * Library_ReleaseGames()).
 *
 * @todo Implement and use this class for Windows.
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_LIBRARY_H
#define LIBDOOMSDAY_LIBRARY_H

#include "libdoomsday.h"

#ifdef __cplusplus
extern "C" {
#endif

struct library_s; // The library instance (opaque).
typedef struct library_s Library;

/**
 * Initializes the library loader.
 */
LIBDOOMSDAY_PUBLIC void Library_Init(void);

/**
 * Release all resources associated with dynamic libraries. Must be called
 * when shutting down the engine.
 */
LIBDOOMSDAY_PUBLIC void Library_Shutdown(void);

/**
 * Returns the latest error message.
 */
LIBDOOMSDAY_PUBLIC const char* Library_LastError(void);

/**
 * Closes the library handles of all game plugins. The library will be
 * reopened automatically when needed.
 */
LIBDOOMSDAY_PUBLIC void Library_ReleaseGames(void);

/**
 * Looks for dynamic libraries and calls @a func for each one.
 *
 * Arguments passed to the callback function @a func:
 * - @a libraryFile is a pointer to a de::LibraryFile instance.
 * - @a fileName is the name of the library file, including extension.
 * - @a absPath is the absolute (non-native) path of the file.
 * - @a data is the @a data from the caller.
 *
 * @return If all available libraries were iterated, returns 0. If @a func
 * returns a non-zero value to indicate aborting the iteration at some point,
 * that value is returned instead.
 */
LIBDOOMSDAY_PUBLIC
int Library_IterateAvailableLibraries(int (*func)(void *libraryFile, const char* fileName,
                                                  const char* absPath, void* data), void* data);

/**
 * Loads a dynamic library.
 *
 * @param filePath  Absolute path of the library to open.
 */
LIBDOOMSDAY_PUBLIC Library* Library_New(const char* filePath);

LIBDOOMSDAY_PUBLIC void Library_Delete(Library* lib);

/**
 * Returns the type identifier of the library.
 * @see de::Library
 *
 * @param lib  Library instance.
 *
 * @return Type identifier string, e.g., "deng-plugin/game".
 */
LIBDOOMSDAY_PUBLIC const char* Library_Type(const Library* lib);

/**
 * Looks up a symbol from the library.
 *
 * @param lib         Library instance.
 * @param symbolName  Name of the symbol.
 *
 * @return @c NULL if the symbol is not defined. Otherwise the address of
 * the symbol.
 */
LIBDOOMSDAY_PUBLIC void* Library_Symbol(Library* lib, const char* symbolName);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include <de/LibraryFile>
LIBDOOMSDAY_PUBLIC de::LibraryFile& Library_File(Library* lib);
#endif

#endif /* LIBDOOMSDAY_LIBRARY_H */
