/** @file g_update.cpp  Engine reset => game update logic.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "g_update.h"

#include <cctype>
#include "animdefs.h"
#include "g_common.h"
#include "gamesession.h"
#include "hu_menu.h"
#include "hu_pspr.h"
#include "hu_stuff.h"
#include "p_inventory.h"
#include "p_sound.h"
#include "p_start.h"
#include "r_common.h"
#include "saveslots.h"

using namespace common;

#define MANGLE_STATE(x)     (INT2PTR(state_t, ((x)? (x)-STATES : -1)))
#define RESTORE_STATE(x)    (PTR2INT(x) < 0? NULL : &STATES[PTR2INT(x)])

static int mangleMobj(thinker_t *th, void * /*context*/)
{
    mobj_t *mo = (mobj_t *) th;

    mo->state = MANGLE_STATE(mo->state);
    mo->info  = INT2PTR(mobjinfo_t, mo->info - MOBJINFO);

    return false; // Continue iteration.
}

static int restoreMobj(thinker_t *th, void * /*context*/)
{
    mobj_t *mo = (mobj_t *) th;

    mo->state = RESTORE_STATE(mo->state);
    mo->info  = &MOBJINFO[PTR2INT(mo->info)];

    return false; // Continue iteration.
}

/**
 * Called before the engine re-inits the definitions. After that all the
 * state, info, etc. pointers will be obsolete.
 */
void G_MangleState()
{
    Thinker_Iterate(P_MobjThinker, mangleMobj, NULL);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr = &players[i];
        for(int k = 0; k < NUMPSPRITES; ++k)
        {
            plr->pSprites[k].state = MANGLE_STATE(plr->pSprites[k].state);
        }
    }
}

void G_RestoreState()
{
    Thinker_Iterate(P_MobjThinker, restoreMobj, NULL);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr = &players[i];
        for(int k = 0; k < NUMPSPRITES; ++k)
        {
            plr->pSprites[k].state = RESTORE_STATE(plr->pSprites[k].state);
        }
    }

    HU_UpdatePsprites();
}

/**
 * Handles engine updates and renderer restarts.
 */
void G_UpdateState(int step)
{
    switch(step)
    {
    case DD_PRE:
        G_MangleState();
        P_InitPicAnims(); // Redefine texture animations.
        break;

    case DD_POST:
        G_RestoreState();
        R_InitRefresh();
        R_LoadColorPalettes();
        P_Update();
#if !__JHEXEN__
        XG_Update();
#endif

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
        P_InitInventory();
#endif

        Hu_MenuInit();
        G_SaveSlots().updateAll();

#if __JHEXEN__
        SndInfoParser(AutoStr_FromText("Lumps:SNDINFO"));
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        S_MapMusic(gfw_Session()->mapUri());
#endif
        break;

    case DD_RENDER_RESTART_PRE:
        Hu_UnloadData();
        GUI_ReleaseResources();
        break;

    case DD_RENDER_RESTART_POST:
        Hu_LoadData();
        GUI_LoadResources();
        break;
    }
}
