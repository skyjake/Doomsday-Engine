/** @file st_stuff.h  Doom 64 specific HUD.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#ifndef LIBDOOM64_STUFF_H
#define LIBDOOM64_STUFF_H

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "d_config.h"

// Palette indices.
// For damage/bonus red-/gold-shifts
extern const int STARTREDPALS;
extern const int NUMREDPALS;

extern const int STARTBONUSPALS;
extern const int NUMBONUSPALS;

// Statusbar width/height -- these are here to make the widget library behave
extern const int ST_WIDTH;
extern const int ST_HEIGHT;

#ifdef __cplusplus
#  include "hu_lib.h"

class AutomapWidget;
class ChatWidget;
class PlayerLogWidget;

AutomapWidget*      ST_TryFindAutomapWidget(int);
ChatWidget*         ST_TryFindChatWidget(int);
PlayerLogWidget*    ST_TryFindLogWidget(int);

extern "C" {
#endif

/*
 * HUD Lifecycle
 */

void    ST_Register(void);
void    ST_Init(void);
void    ST_Shutdown(void);

/*
 * HUD Runtime Callbacks
 */

int     ST_Responder(event_t*);
void    ST_Ticker(timespan_t);
void    ST_Drawer(int);

/*
 * HUD Control
 */

/**
 * Returns the unique identifier of the active HUD configuration.
 *
 * (Each independent HUD configuration is attributed a unique identifier. The
 * statusbar and fullscreen-HUD are examples of HUD configurations).
 *
 * @param localPlayer  Player to lookup the active HUD for.
 */
int     ST_ActiveHud(int);
void    ST_Start(int);
void    ST_Stop(int);
void    ST_CloseAll(int, dd_bool);
void    HU_WakeWidgets(int);

// XXX libcommon expects to link against things we implement (?????????????????????????????????????????????????????)
dd_bool ST_StatusBarIsActive(int);
float   ST_StatusBarShown(int); 

/*
 * Chat
 */

dd_bool ST_ChatIsActive(int);
dd_bool ST_StatusBarIsActive(int);

/*
 * Log
 */

/**
 * Post a message to the specified player's log.
 *
 * @param localPlayer  Player number whose log to post to.
 * @param flags        @ref logMessageFlags
 * @param text         Message Text to be posted. Messages may use the same
 * parameter control blocks as with the engine's Text rendering API.
 */
void    ST_LogPost(int, byte, const char *);

/**
 * Rewind the message log of the specified player, making the last few messages
 * visible once again.
 *
 * @param localPlayer  Player number whose message log to refresh.
 */
void    ST_LogRefresh(int);

/**
 * Empty the message log of the specified player.
 *
 * @param localPlayer  Player number whose message log to empty.
 */
void    ST_LogEmpty(int);

void    ST_LogUpdateAlignment(void);

/*
 * HUD Map
 */

/**
 * Start the automap.
 */
void    ST_AutomapOpen(int, dd_bool, dd_bool);
dd_bool ST_AutomapIsOpen(int);
void    ST_AutomapFollowMode(int);
void    ST_AutomapZoomMode(int);
float   ST_AutomapOpacity(int);

/**
 * Does the player's automap obscure this region completely?
 * @pre Window dimensions use the fixed coordinate space {x} 0 - 320, {y} 0 - 200.
 *
 * @param localPlayer  Player number whose automap to check.
 * @param region       Window region.
 *
 * @return  @true= there is no point even partially visible.
 */
dd_bool ST_AutomapObscures(int, int, int, int, int);
dd_bool ST_AutomapObscures2(int, const RectRaw *);
int     ST_AutomapAddPoint(int, coord_t, coord_t, coord_t);
void    ST_AutomapClearPoints(int);
void    ST_SetAutomapCameraRotation(int, dd_bool);
void    ST_SetAutomapCheatLevel(int, int);
int     ST_AutomapCheatLevel(int);
void    ST_CycleAutomapCheatLevel(int);
void    ST_RevealAutomap(int, dd_bool);
dd_bool ST_AutomapIsRevealed(int);

/**
 * Unhides the current HUD display if hidden.
 *
 * @param localPlayer  Player whoose HUD to (maybe) unhide.
 * @param event        Event type trigger.
 */
void    ST_HUDUnHide(int, hueevent_t);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LIBDOOM64_STUFF_H
