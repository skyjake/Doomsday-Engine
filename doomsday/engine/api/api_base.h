/** @file de_api.h Public Base API.
 * @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_API_BASE_H
#define DOOMSDAY_API_BASE_H

#include "apis.h"
#include "api_uri.h"
#include "resourceclass.h"

/// @addtogroup game
/// @{

/**
 * Defines the numerous high-level properties of a logical game component.
 * Note that this is POD; no construction or destruction is needed.
 * @see DD_DefineGame() @ingroup game
 */
typedef struct gamedef_s {
   /**
    * Unique game mode key/identifier, 16 chars max (e.g., "doom1-ultimate").
    * - Used during resource location for mode-specific assets.
    * - Sent out in netgames (a client can't connect unless mode strings match).
    */
    const char* identityKey;

    /// Name of the config directory.
    const char* configDir;

    /// Default title. May be overridden later.
    const char* defaultTitle;

    /// Default author. May be overridden later.
    /// Used for (e.g.) the map author name if not specified in a Map Info definition.
    const char* defaultAuthor;
} GameDef;

/**
 * Extended info about a registered game component.
 * @see DD_GameInfo() @ingroup game
 */
typedef struct gameinfo_s {
    const char* title;
    const char* author;
    const char* identityKey;
} GameInfo;

/// @}

// The Base API.
DENG_API_TYPEDEF(Base) // v1
{
    de_api_t api;

    void (*Quit)(void);

    int (*GetInteger)(int ddvalue);
    void (*SetInteger)(int ddvalue, int parm);
    void* (*GetVariable)(int ddvalue);
    void (*SetVariable)(int ddvalue, void* ptr);

    /**
     * Register a new game.
     *
     * @param definition  GameDef structure defining the new game.
     *
     * @return  Unique identifier/name assigned to resultant game.
     *
     * @note Game registration order defines the order of the automatic game
     * identification/selection logic.
     */
    gameid_t (*DefineGame)(GameDef const* definition);

    /**
     * Retrieves the game identifier for a previously defined game.
     * @see DD_DefineGame().
     *
     * @param identityKey  Identity key of the game.
     *
     * @return Game identifier.
     */
    gameid_t (*GameIdForKey)(char const* identityKey);

    /**
     * Adds a new resource to the list for the identified @a game.
     *
     * @note Resource order defines the load order of resources (among those of
     * the same type). Resources are loaded from most recently added to least
     * recent.
     *
     * @param game      Unique identifier/name of the game.
     * @param classId   Class of resource being defined.
     * @param fFlags    File flags (see @ref fileFlags).
     * @param names     One or more known potential names, seperated by semicolon
     *                  (e.g., <pre> "name1;name2" </pre>). Valid names include
     *                  absolute or relative file paths, possibly with encoded
     *                  symbolic tokens, or predefined symbols into the virtual
     *                  file system.
     * @param params    Additional parameters. Usage depends on resource type.
     *                  For package resources this may be C-String containing a
     *                  semicolon delimited list of identity keys.
     */
    void (*AddGameResource)(gameid_t game, resourceclassid_t classId, int fFlags,
                            const char* names, void* params);

    /**
     * Retrieve extended info about the current game.
     *
     * @param info      Info structure to be populated.
     *
     * @return          @c true if successful else @c false (i.e., no game loaded).
     */
    boolean (*gameInfo)(GameInfo* info);

    /**
     * Determines whether the current run of the thinkers should be considered a
     * "sharp" tick. Sharp ticks occur exactly 35 times per second. Thinkers may be
     * called at any rate faster than this; in order to retain compatibility with
     * the original Doom engine game logic that ran at 35 Hz, such logic should
     * only be executed on sharp ticks.
     *
     * @return @c true, if a sharp tick is currently in effect.
     *
     * @ingroup playsim
     */
    boolean (*IsSharpTick)(void);

    /**
     * Send a packet over the network.
     *
     * @param to_player  Player number to send to. The server is number zero.
     *                   May include @ref netSendFlags.
     * @param type       Type of the packet.
     * @param data       Data of the packet.
     * @param length     Length of the data.
     */
    void (*SendPacket)(int to_player, int type, const void* data, size_t length);
}
DENG_API_T(Base);

#ifndef DENG_NO_API_MACROS_BASE
#define Sys_Quit            _api_Base.Quit
#define DD_GetInteger       _api_Base.GetInteger
#define DD_SetInteger       _api_Base.SetInteger
#define DD_GetVariable      _api_Base.GetVariable
#define DD_SetVariable      _api_Base.SetVariable
#define DD_DefineGame       _api_Base.DefineGame
#define DD_GameIdForKey     _api_Base.GameIdForKey
#define DD_AddGameResource  _api_Base.AddGameResource
#define DD_GameInfo         _api_Base.gameInfo
#define DD_IsSharpTick      _api_Base.IsSharpTick
#define Net_SendPacket      _api_Base.SendPacket
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Base);
#endif

#endif // DOOMSDAY_API_BASE_H
