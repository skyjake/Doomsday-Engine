/** @file api_player.h  Public API for players.
 * @ingroup playsim
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_API_PLAYER_H
#define DOOMSDAY_API_PLAYER_H

#include "apis.h"
#include "dd_share.h" // DDMAXPLAYERS
#include <de/legacy/smoother.h>
#include <doomsday/player.h>

/**
 * @defgroup player Player
 * @ingroup playsim
 */

/// Built-in impulse identifiers. @ingroup player
enum {
    CTL_WALK = 1,           ///< Forward/backwards.
    CTL_SIDESTEP = 2,       ///< Left/right sideways movement.
    CTL_ZFLY = 3,           ///< Up/down movement.
    CTL_TURN = 4,           ///< Turning horizontally.
    CTL_LOOK = 5,           ///< Turning up and down.
    CTL_MODIFIER_1 = 6,
    CTL_MODIFIER_2 = 7,
    CTL_MODIFIER_3 = 8,
    CTL_MODIFIER_4 = 9,
    CTL_FIRST_GAME_CONTROL = 1000
};

/**
 * Logical impulse types:
 */
typedef enum impulsetype_e {
    IT_ANALOG,            ///< A numeric value determined by current device-control state.
    IT_ANALOG_TRIGGERED,  ///< Analog, but accepts triggered states as well.
    IT_BINARY             ///< Always accepts triggered states.
} impulsetype_t;

#define IMPULSETYPE_IS_TRIGGERABLE(t) ((t) == IT_ANALOG_TRIGGERED || (t) == IT_BINARY)

typedef impulsetype_t controltype_t;

#define CTLT_NUMERIC           IT_ANALOG
#define CTLT_NUMERIC_TRIGGERED IT_ANALOG_TRIGGERED
#define CTLT_IMPULSE           IT_BINARY

/**
 * @defgroup playerFlags Player Flags
 * @ingroup player apiFlags
 * @{
 */
#define DDPF_FIXANGLES          0x0001 ///< Server: send angle/pitch to client.
#define DDPF_FIXORIGIN          0x0004 ///< Server: send coords to client.
#define DDPF_DEAD               0x0008 ///< Cl & Sv: player is dead.
#define DDPF_CAMERA             0x0010 ///< Player is a cameraman.
#define DDPF_LOCAL              0x0020 ///< Player is local (e.g. player zero).
#define DDPF_FIXMOM             0x0040 ///< Server: send momentum to client.
#define DDPF_NOCLIP             0x0080 ///< Client: don't clip movement.
#define DDPF_CHASECAM           0x0100 ///< Chase camera mode (third person view).
#define DDPF_INTERYAW           0x0200 ///< Interpolate view yaw angles (used with locking).
#define DDPF_INTERPITCH         0x0400 ///< Interpolate view pitch angles (used with locking).
#define DDPF_VIEW_FILTER        0x0800 ///< Cl & Sv: Draw the current view filter.
#define DDPF_REMOTE_VIEW_FILTER 0x1000 ///< Client: Draw the view filter (has been set remotely).
#define DDPF_USE_VIEW_FILTER    (DDPF_VIEW_FILTER | DDPF_REMOTE_VIEW_FILTER)
#define DDPF_UNDEFINED_ORIGIN   0x2000 ///< Origin of the player is undefined (view not drawn).
#define DDPF_UNDEFINED_WEAPON   0x4000 ///< Weapon of the player is undefined (not sent yet).
///@}

DE_API_TYPEDEF(Player)
{
    de_api_t api;

    /**
     * @return The name of player @a player.
     */
    const char *(*GetPlayerName)(int player);

    /**
     * @return Client identifier for player @a player.
     */
    ident_t (*GetPlayerID)(int player);

    /**
     * Provides access to the player's movement smoother.
     */
    Smoother *(*GetSmoother)(int player);

    /**
     * Gets the data of a player.
     *
     * @param number  Console/player number.
     *
     * @return Player data.
     */
    ddplayer_t *(*GetPlayer)(int number);

    /**
     * Register a new impulse for controlling a player (e.g., shoot, walk, etc...).
     *
     * @param id           Unique identifier for the impulse.
     * @param type         Logical impulse type (boolean, numeric...).
     * @param name         A unique, symbolic name for the impulse, for use when
     *                     binding to control events.
     * @param bindContext  Symbolic name of the binding context in which the impulse
     *                     is effective.
     */
    void (*NewControl)(int id, impulsetype_t type, const char *name, const char *bindContext);

    /**
     * Determines if one or more bindings exist for a player and impulse Id in
     * the associated binding context.
     *
     * @param playerNum  Console/player number.
     * @param impulseId  Unique identifier of the impulse to lookup bindings for.
     *
     * @return  @c true if one or more bindings exist.
     */
    int (*IsControlBound)(int playerNum, int impulseId);

    /**
     * Lookup the current state of a non-boolean impulse for a player.
     *
     * @param playerNum  Console/player number.
     * @param impulseId  Unique identifier of the impulse to lookup the state of.
     * @param pos        Current pos/strength of the impulse is written here.
     * @param relOffset  Base-relative offset of the impulse is written here.
     */
    void (*GetControlState)(int playerNum, int impulseId, float *pos, float *relOffset);

    /**
     * @return  Number of times a @em boolean impulse has been triggered since the last call.
     */
    int (*GetImpulseControlState)(int playerNum, int impulseId);

    /**
     * Trigger a @em boolean impulse for a player.
     *
     * @param playerNum  Console/player number.
     * @param impulseId  Unique identifier of the impulse to trigger.
     */
    void (*Impulse)(int playerNum, int impulseId);
}
DE_API_T(Player);

#ifndef DE_NO_API_MACROS_PLAYER
#define Net_GetPlayerName           _api_Player.GetPlayerName
#define Net_GetPlayerID             _api_Player.GetPlayerID
#define Net_PlayerSmoother          _api_Player.GetSmoother
#define DD_GetPlayer                _api_Player.GetPlayer
#define P_NewPlayerControl          _api_Player.NewControl
#define P_GetControlState           _api_Player.GetControlState
#define P_IsControlBound            _api_Player.IsControlBound
#define P_GetImpulseControlState    _api_Player.GetImpulseControlState
#define P_Impulse                   _api_Player.Impulse
#endif

#ifdef __DOOMSDAY__
DE_USING_API(Player);
#endif

#endif // DOOMSDAY_API_PLAYER_H
