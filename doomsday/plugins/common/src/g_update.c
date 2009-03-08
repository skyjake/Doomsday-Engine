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
 * g_update.c: Routines to call when updating the state of the engine
 *
 * \bug Not 64bit clean: In function 'G_RestoreState': cast from pointer to integer of different size
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#  include "m_cheat.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "hu_pspr.h"
#include "hu_menu.h"
#include "rend_automap.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

#define MANGLE_STATE(x)     ((state_t*) ((x)? (x)-STATES : -1))
#define RESTORE_STATE(x)    ((int)(x)==-1? NULL : &STATES[(int)(x)])

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static boolean mangleMobj(thinker_t* th, void* context)
{
    mobj_t*             mo = (mobj_t *) th;

    mo->state = MANGLE_STATE(mo->state);
    mo->info = (mobjinfo_t *) (mo->info - MOBJINFO);

    return true; // Continue iteration.
}

static boolean restoreMobj(thinker_t* th, void* context)
{
    mobj_t*             mo = (mobj_t *) th;

    mo->state = RESTORE_STATE(mo->state);
    mo->info = &MOBJINFO[(int) mo->info];

    return true; // Continue iteration.
}

/**
 * Called before the engine re-inits the definitions. After that all the
 * state, info, etc. pointers will be obsolete.
 */
void G_MangleState(void)
{
    int                 i;

    P_IterateThinkers(P_MobjThinker, mangleMobj, NULL);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           plr = &players[i];
        int                 k;

        for(k = 0; k < NUMPSPRITES; ++k)
            plr->pSprites[k].state =
                MANGLE_STATE(plr->pSprites[k].state);
    }
}

void G_RestoreState(void)
{
    int                 i;

    P_IterateThinkers(P_MobjThinker, restoreMobj, NULL);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           plr = &players[i];
        int                 k;

        for(k = 0; k < NUMPSPRITES; ++k)
            plr->pSprites[k].state =
                RESTORE_STATE(plr->pSprites[k].state);
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
    case DD_GAME_MODE:
        // Set the game mode string.
        G_IdentifyVersion();
        break;

    case DD_PRE:
        G_MangleState();
        break;

    case DD_POST:
        G_RestoreState();
        R_InitRefresh();
        P_Init();
        //// \fixme Detect gameMode changes (GM_DOOM -> GM_DOOM2, for instance).
#if !__JHEXEN__
        XG_Update();
#endif

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
        P_InitInventory();
#endif

#if __JHERETIC__ || __JHEXEN__
        ST_Init();
#endif

        Cht_Init();
        Hu_MenuInit();

#if __JHEXEN__
        S_ParseSndInfoLump();
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        S_MapMusic();
#endif
        break;

    case DD_RENDER_RESTART_PRE:
        // Free the background fog effect texture.
        Hu_UnloadData();

        // Free the automap mask texture.
        Rend_AutomapUnloadData();
        break;

    case DD_RENDER_RESTART_POST:
        // Reload the background fog effect texture.
        Hu_LoadData();

        // Reload the automap mask texture.
        Rend_AutomapLoadData();
        break;
    }
}

static char* ScanWord(char* ptr, char* buf)
{
    if(ptr)
    {
        // Skip whitespace at beginning.
        while(*ptr && isspace(*ptr))
            ptr++;
        while(*ptr && !isspace(*ptr))
            *buf++ = *ptr++;
    }

    *buf = 0;
    return ptr;
}
