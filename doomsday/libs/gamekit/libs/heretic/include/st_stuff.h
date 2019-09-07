/** @file st_stuff.h  Heretic specific HUD.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHERETIC_STUFF_H
#define LIBHERETIC_STUFF_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "h_config.h"

// Size of statusbar, now sensitive for scaling.
#define ST_HEIGHT                   (42 * SCREEN_MUL)
#define ST_WIDTH                    (SCREENWIDTH)
#define ST_Y                        (SCREENHEIGHT - ST_HEIGHT)

#define ST_AUTOMAP_OBSCURE_TOLERANCE (.9999f)

#define HUD_ITEM_FLASH_TICS         ( 4 )

#ifdef __cplusplus
#  include "hu_lib.h"

class AutomapWidget;
class ChatWidget;
class PlayerLogWidget;

AutomapWidget *ST_TryFindAutomapWidget(int localPlayer);
ChatWidget *ST_TryFindChatWidget(int localPlayer);
PlayerLogWidget *ST_TryFindPlayerLogWidget(int localPlayer);

extern "C" {
#endif

/// Register the console commands, variables, etc..., of this module.
void ST_Register(void);

void ST_Init(void);
void ST_Shutdown(void);

int ST_Responder(event_t *ev);
void ST_Ticker(timespan_t ticLength);
void ST_Drawer(int localPlayer);

/**
 * Returns the unique identifier of the active HUD configuration.
 *
 * (Each independent HUD configuration is attributed a unique identifier. The
 * statusbar and fullscreen-HUD are examples of HUD configurations).
 *
 * @param localPlayer  Player to lookup the active HUD for.
 */
int ST_ActiveHud(int localPlayer);

void ST_Start(int localPlayer);
void ST_Stop(int localPlayer);
void HU_WakeWidgets(int localPlayer);

void ST_CloseAll(int localPlayer, dd_bool fast);

dd_bool ST_ChatIsActive(int localPlayer);
dd_bool ST_StatusBarIsActive(int localPlayer);
float ST_StatusBarShown(int localPlayer);

/**
 * Post a message to the specified player's log.
 *
 * @param localPlayer  Player number whose log to post to.
 * @param flags        @ref logMessageFlags
 * @param text         Message Text to be posted. Messages may include encoded
 * parameters to control how the messae is presented.
 */
void ST_LogPost(int localPlayer, byte flags, const char *text);

/**
 * Rewind the message log of the specified player, making the last few messages
 * visible once again.
 *
 * @param localPlayer  Player number whose message log to refresh.
 */
void ST_LogRefresh(int localPlayer);

/**
 * Empty the message log of the specified player.
 *
 * @param localPlayer  Player number whose message log to empty.
 */
void ST_LogEmpty(int localPlayer);

void ST_LogUpdateAlignment(void);

/**
 * Start the automap.
 */
void ST_AutomapOpen(int localPlayer, dd_bool yes, dd_bool fast);

dd_bool ST_AutomapIsOpen(int localPlayer);

void ST_AutomapFollowMode(int localPlayer);
void ST_AutomapZoomMode(int localPlayer);

float ST_AutomapOpacity(int localPlayer);

/**
 * Does the player's automap obscure this region completely?
 * @pre Window dimensions use the fixed coordinate space {x} 0 - 320, {y} 0 - 200.
 *
 * @param localPlayer  Player number whose automap to check.
 * @param region       Window region.
 *
 * @return  @true= there is no point even partially visible.
 */
dd_bool ST_AutomapObscures2(int localPlayer, const RectRaw *region);
dd_bool ST_AutomapObscures(int localPlayer, int x, int y, int width, int height);

int ST_AutomapAddPoint(int localPlayer, coord_t x, coord_t y, coord_t z);
void ST_AutomapClearPoints(int localPlayer);

void ST_SetAutomapCameraRotation(int localPlayer, dd_bool on);

int ST_AutomapCheatLevel(int localPlayer);
void ST_SetAutomapCheatLevel(int localPlayer, int level);
void ST_CycleAutomapCheatLevel(int localPlayer);

void ST_RevealAutomap(int localPlayer, dd_bool on);
dd_bool ST_AutomapIsRevealed(int localPlayer);

/**
 * Unhides the current HUD display if hidden.
 *
 * @param localPlayer  Player whoose HUD to (maybe) unhide.
 * @param event        Event type trigger.
 */
void ST_HUDUnHide(int localPlayer, hueevent_t ev);

void ST_FlashCurrentItem(int localPlayer);
int ST_ReadyItemFlashCounter(int localPlayer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // LIBHERETIC_STUFF_H
