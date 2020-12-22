/** @file api_base.h Public Base API.
 * @ingroup base
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include <de/legacy/str.h>
#include <doomsday/resourceclass.h>
#include <doomsday/game.h>
#include "apis.h"
#include "api_uri.h"

/// @addtogroup game
/// @{

/**
 * Extended info about a registered game component.
 * @see DD_GameInfo() @ingroup game
 */
typedef struct gameinfo_s {
    AutoStr *title;
    AutoStr *author;
    AutoStr *identityKey;
} GameInfo;

/// @}

// The Base API.
DE_API_TYPEDEF(Base)
{
    de_api_t api;

    void (*Quit)(void);

    int (*GetInteger)(int ddvalue);
    void (*SetInteger)(int ddvalue, int parm);
    void *(*GetVariable)(int ddvalue);
    void (*SetVariable)(int ddvalue, void *ptr);

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
    //gameid_t (*DefineGame)(const GameDef *definition);

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
    //void (*AddGameResource)(gameid_t game, resourceclassid_t classId, int fFlags,
    //                        const char *names, const void *params);

    /**
     * Retrieve extended info about the current game.
     *
     * @param info      Info structure to be populated.
     *
     * @return          @c true if successful else @c false (i.e., no game loaded).
     */
    dd_bool (*GameInfo_)(GameInfo *info);

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
    dd_bool (*IsSharpTick)(void);

    /**
     * Send a packet over the network.
     *
     * @param to_player  Player number to send to. The server is number zero.
     *                   May include @ref netSendFlags.
     * @param type       Type of the packet.
     * @param data       Data of the packet.
     * @param length     Length of the data.
     */
    void (*SendPacket)(int to_player, int type, const void *data, size_t length);

    /**
     * To be called by the game after loading a save state to instruct the engine
     * perform map setup once more.
     */
    void (*SetupMap)(int mode, int flags);
}
DE_API_T(Base);

#ifndef DE_NO_API_MACROS_BASE
#define Sys_Quit                  _api_Base.Quit
#define DD_GetInteger             _api_Base.GetInteger
#define DD_SetInteger             _api_Base.SetInteger
#define DD_GetVariable            _api_Base.GetVariable
#define DD_SetVariable            _api_Base.SetVariable
//#define DD_DefineGame             _api_Base.DefineGame
//#define DD_GameIdForKey           _api_Base.GameIdForKey
//#define DD_AddGameResource        _api_Base.AddGameResource
#define DD_GameInfo               _api_Base.GameInfo_
#define DD_IsSharpTick            _api_Base.IsSharpTick
#define Net_SendPacket            _api_Base.SendPacket
#define R_SetupMap                _api_Base.SetupMap
#endif

#ifdef __DOOMSDAY__
DE_USING_API(Base);
#endif

#endif // DOOMSDAY_API_BASE_H
