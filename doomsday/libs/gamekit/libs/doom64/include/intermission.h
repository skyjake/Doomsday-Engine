/** @file intermission.h  DOOM64 specific intermission screens.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#ifndef LIBDOOM64_INTERMISSION_H
#define LIBDOOM64_INTERMISSION_H
#ifdef __cplusplus

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "d_player.h"
#include <doomsday/uri.h>

// Global locations
#define WI_TITLEY               (2)
#define WI_SPACINGY             (33)

// Single-player stuff
#define SP_STATSX               (50)
#define SP_STATSY               (50)
#define SP_TIMEX                (16)
#define SP_TIMEY                (SCREENHEIGHT-32)

// Net game stuff
#define NG_STATSY               (50)
#define NG_STATSX               (32)
#define NG_SPACINGX             (64)

// Deathmatch stuff
#define DM_MATRIXX              (42)
#define DM_MATRIXY              (68)
#define DM_SPACINGX             (40)
#define DM_TOTALSX              (269)
#define DM_KILLERSX             (10)
#define DM_KILLERSY             (100)
#define DM_VICTIMSX             (5)
#define DM_VICTIMSY             (50)

// States for single-player
#define SP_KILLS                (0)
#define SP_ITEMS                (2)
#define SP_SECRET               (4)
#define SP_FRAGS                (6)
#define SP_TIME                 (8)
#define SP_PAR                  (ST_TIME)
#define SP_PAUSE                (1)

// States for the intermission
enum interludestate_t
{
    ILS_NONE = -1,
    ILS_SHOW_STATS,
    ILS_UNUSED /// dj: DOOM64 has no "show next map" state as Doom does however
               /// the DOOM64TC did not update the actual state progression.
               /// Instead it had to pass through a this state requring an extra
               /// key press to skip. This should be addressed by updating the
               /// relevant state progressions.
};

/**
 * Structure passed to IN_Begin(), etc...
 */
struct wbplayerstruct_t
{
    dd_bool inGame;  ///< Whether the player is in game.

    int kills;
    int items;
    int secret;
    int time;
    int frags[MAXPLAYERS];
    int score;       ///< Current score on entry, modified on return.
};

struct wbstartstruct_t
{
    res::Uri currentMap;
    res::Uri nextMap;
    bool didSecret;      /**< @c true= the secret map has been visited during the
                              game session. Used to generate the visited maps info
                              for backward compatibility purposes. */
    int maxKills;
    int maxItems;
    int maxSecret;
    int maxFrags;
    int parTime;
    int pNum;           ///< Index of this player in game.
    wbplayerstruct_t plyr[MAXPLAYERS];
};

/// To be called to register the console commands and variables of this module.
void IN_ConsoleRegister();

/**
 * Begin the intermission using the given game session and player configuration.
 *
 * @param wbstartstruct  Configuration to use for the intermission. Ownership is
 *                       @em not given to IN_Begin() however it is assumed that
 *                       this structure is @em not modified while the intermission
 *                       is in progress.
 */
void IN_Begin(const wbstartstruct_t &wbstartstruct);

/**
 * Process game tic for the intermission.
 *
 * @note Handles user input due to timing issues in netgames.
 */
void IN_Ticker();

/**
 * Draw the intermission.
 */
void IN_Drawer();

/**
 * Change the current intermission state.
 */
void IN_SetState(interludestate_t st);

/**
 * End the current intermission.
 */
void IN_End();

/**
 * Skip to the next state in the intermission.
 */
void IN_SkipToNext();

#endif // __cplusplus
#endif // LIBDOOM64_INTERMISSION_H
