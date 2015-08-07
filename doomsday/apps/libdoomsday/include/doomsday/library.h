/** @file library.h  Dynamic libraries.
 * @ingroup base
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
 * @authors Copyright © 2006-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2015 Daniel Swanson <danij@dengine.net>
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
 * Loads a dynamic library.
 *
 * @param filePath  Absolute path of the library to open.
 */
LIBDOOMSDAY_PUBLIC Library *Library_New(char const *filePath);

LIBDOOMSDAY_PUBLIC void Library_Delete(Library *lib);

/**
 * Returns the type identifier of the library.
 * @see de::Library
 *
 * @param lib  Library instance.
 *
 * @return Type identifier string, e.g., "deng-plugin/game".
 */
LIBDOOMSDAY_PUBLIC char const *Library_Type(Library const *lib);

/**
 * Looks up a symbol from the library.
 *
 * @param lib         Library instance.
 * @param symbolName  Name of the symbol.
 *
 * @return @c NULL if the symbol is not defined. Otherwise the address of
 * the symbol.
 */
LIBDOOMSDAY_PUBLIC void *Library_Symbol(Library *lib, char const *symbolName);

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus
#include <de/LibraryFile>
#include <functional>

LIBDOOMSDAY_PUBLIC de::LibraryFile &Library_File(Library *lib);

LIBDOOMSDAY_PUBLIC de::LoopResult Library_forAll(std::function<de::LoopResult (de::LibraryFile &)> func);
#endif

#endif  // LIBDOOMSDAY_LIBRARY_H
