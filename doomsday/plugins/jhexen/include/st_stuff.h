/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */
#ifndef __ST_STUFF_H__
#define __ST_STUFF_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "x_player.h"
#include "x_config.h"

// Size of statusbar.
// Now sensitive for scaling.
#define SCREEN_MUL 1
#define ST_HEIGHT   42*SCREEN_MUL
#define ST_WIDTH    SCREENWIDTH
#define ST_Y        (SCREENHEIGHT - ST_HEIGHT)

// Called by main loop.
void    ST_Ticker(void);

// Called by main loop.
void    ST_Drawer(int fullscreenmode, boolean refresh);

// Called when the console player is spawned on each level.
void    ST_Start(void);

void    ST_Stop(void);

// Called by startup code.
void    ST_Register(void);
void    ST_Init(void);

void    ST_updateGraphics(void);

// Called when it might be neccessary for the hud to unhide.
void    ST_HUDUnHide(hueevent_t event);

// Called to execute the change of player class.
void    SB_ChangePlayerClass(player_t *player, int newclass);

// Called in P_inter & P_enemy
void        ST_doPaletteStuff(boolean forceChange);

// States for status bar code.
typedef enum {
    AutomapState,
    FirstPersonState
} st_stateenum_t;

// States for the chat code.
typedef enum {
    StartChatState,
    WaitDestState,
    GetChatState
} st_chatstateenum_t;

#endif
