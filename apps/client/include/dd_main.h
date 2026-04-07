/** @file dd_main.h
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_MAIN_H
#define DE_MAIN_H

#include <de/string.h>
#include <doomsday/res/resources.h>
#include <doomsday/gameapi.h>
#include <doomsday/plugins.h>
#include <doomsday/games.h>

#ifdef __CLIENT__
#  include "resource/clientresources.h"
#  include "world/clientworld.h"
#else
#  include "serverworld.h"
#endif
#include "audio/audiosystem.h"
#include "ui/infine/infinesystem.h"

namespace res { class File1; }

extern int isDedicated; // true if __SERVER__
#ifdef __CLIENT__
extern int symbolicEchoMode;
#endif

int     DD_EarlyInit();
void    DD_FinishInitializationAfterWindowReady();
void    DD_ConsoleRegister();

/**
 * Print an error message and quit.
 */
DE_NORETURN void App_Error(const char *error, ...);

DE_NORETURN void App_AbnormalShutdown(const char *error);

/// Returns the application's global audio subsystem.
AudioSystem &App_AudioSystem();

/// Returns the application's global InFineSystem.
InFineSystem &App_InFineSystem();

#ifdef __CLIENT__
/// Returns the application's resources.
ClientResources &App_Resources();
/// Returns the application's global WorldSystem.
ClientWorld &App_World();
#else
/// Returns the application's resources.
Resources &App_Resources();
/// Returns the application's global WorldSystem.
ServerWorld &App_World();
#endif

#undef Con_Open

/**
 * Attempt to change the 'open' state of the console.
 * @note In dedicated mode the console cannot be closed.
 */
void Con_Open(int yes);

void DD_CheckTimeDemo();
void DD_UpdateEngineState();

//
// Game modules -------------------------------------------------------------------
//

int DD_ActivateGameWorker(void *context);

/**
 * Returns the application's global Games (collection).
 */
Games &App_Games();

/**
 * Returns the current game from the application's global collection.
 */
const Game &App_CurrentGame();

/**
 * Frees the info structures for all registered games.
 */
void App_ClearGames();

//
// Misc/utils ---------------------------------------------------------------------
//

/**
 * Attempts to read help strings from the game-specific help file.
 */
void DD_ReadGameHelp();

/// @return  Symbolic name of the material scheme associated with @a textureSchemeName.
AutoStr *DD_MaterialSchemeNameForTextureScheme(const Str *textureSchemeName);

/// @overload
de::String DD_MaterialSchemeNameForTextureScheme(const de::String& textureSchemeName);

#ifdef __CLIENT__
fontschemeid_t DD_ParseFontSchemeName(const char *str);
#endif

#endif  // DE_MAIN_H
