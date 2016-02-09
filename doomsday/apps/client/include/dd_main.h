/** @file dd_main.h
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_MAIN_H
#define DENG_MAIN_H

#include <QList>
#include <QMap>
#include <de/LibraryFile>
#include <de/String>
#include <doomsday/resource/resourceclass.h>
#include <doomsday/gameapi.h>
#include <doomsday/plugins.h>
#include <doomsday/Games>

#include "audio/system.h"
#include "resource/resourcesystem.h"
#include "world/clientserverworld.h"
#include "ui/infine/infinesystem.h"

namespace de { class File1; }

extern de::dint verbose;
extern de::dint isDedicated;
extern de::dint gameDataFormat;
#ifdef __CLIENT__
extern de::dint symbolicEchoMode;
#endif

de::dint DD_EarlyInit();
void DD_FinishInitializationAfterWindowReady();

void DD_ConsoleRegister();

/**
 * Print an error message and quit.
 */
void App_Error(char const *error, ...);

void App_AbnormalShutdown(char const *error);

/// Returns the application's global audio::System.
audio::System &App_AudioSystem();

/// Returns the application's global InFineSystem.
InFineSystem &App_InFineSystem();

/// Returns the application's global ResourceSystem.
ResourceSystem &App_ResourceSystem();

/// Returns the application's global WorldSystem.
ClientServerWorld &App_World();

#undef Con_Open

/**
 * Attempt to change the 'open' state of the console.
 * @note In dedicated mode the console cannot be closed.
 */
void Con_Open(de::dint yes);

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
Game const &App_CurrentGame();

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
AutoStr *DD_MaterialSchemeNameForTextureScheme(Str const *textureSchemeName);

/// @overload
de::String DD_MaterialSchemeNameForTextureScheme(de::String textureSchemeName);

fontschemeid_t DD_ParseFontSchemeName(char const *str);

#endif  // DENG_MAIN_H
