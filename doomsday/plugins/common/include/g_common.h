/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 */

/**
 * g_common.h:
 */

#ifndef __COMMON_GAME_H__
#define __COMMON_GAME_H__

#include "dd_share.h"
#include "g_controls.h"

#if __JDOOM__
# include "jdoom.h"
#elif __JDOOM64__
# include "jdoom64.h"
#elif __JHERETIC__
# include "jheretic.h"
#elif __JHEXEN__
# include "jhexen.h"
#elif __JSTRIFE__
# include "jstrife.h"
#endif

#define OBSOLETE        CVF_HIDE|CVF_NO_ARCHIVE

enum {
    JOYAXIS_NONE,
    JOYAXIS_MOVE,
    JOYAXIS_TURN,
    JOYAXIS_STRAFE,
    JOYAXIS_LOOK
};

extern boolean singledemo;

void            G_Register(void);
void            G_PreInit(void);
void            G_PostInit(void);
void            G_StartTitle(void);

gamestate_t     G_GetGameState(void);
void            G_ChangeGameState(gamestate_t state);

gameaction_t    G_GetGameAction(void);
void            G_SetGameAction(gameaction_t action);

// Spawn player at a dummy place.
void            G_DummySpawnPlayer(int playernum);
void            G_DeathMatchSpawnPlayer(int playernum);

int             P_CameraXYMovement(mobj_t* mo);
int             P_CameraZMovement(mobj_t* mo);
void            P_Thrust3D(struct player_s* player, angle_t angle,
                           float lookdir, int forwardmove, int sidemove);

DEFCC( CCmdMakeLocal );
DEFCC( CCmdSetCamera );
DEFCC( CCmdSetViewLock );
DEFCC( CCmdLocalMessage );
DEFCC( CCmdExitLevel );
#endif
