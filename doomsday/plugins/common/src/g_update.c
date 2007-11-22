/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * \bug Not 64bit clean: In function 'G_RestoreState': cast from pointer to integer of different size
 */

/**
 * g_update.c: Routines to call when updating the state of the engine
 * (when loading/unloading WADs and definitions).
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#  include "m_cheat.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "hu_pspr.h"
#include "am_map.h"

// MACROS ------------------------------------------------------------------

#define MANGLE_STATE(x)     ((state_t*) ((x)? (x)-states : -1))
#define RESTORE_STATE(x)    ((int)(x)==-1? NULL : &states[(int)(x)])

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#if __JHEXEN__
void    S_InitScript(void);
void    M_LoadData(void);
void    M_UnloadData(void);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Called before the engine re-inits the definitions. After that all the
 * state, info, etc. pointers will be obsolete.
 */
void G_MangleState(void)
{
    thinker_t  *it;
    mobj_t     *mo;
    int         i, k;

    for(it = thinkercap.next; it != &thinkercap && it; it = it->next)
    {
        if(it->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) it;
        mo->state = MANGLE_STATE(mo->state);
        mo->info = (mobjinfo_t *) (mo->info - mobjinfo);
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        for(k = 0; k < NUMPSPRITES; ++k)
            players[i].psprites[k].state =
                MANGLE_STATE(players[i].psprites[k].state);
    }
}

void G_RestoreState(void)
{
    thinker_t  *it;
    mobj_t     *mo;
    int         i, k;

    for(it = thinkercap.next; it != &thinkercap && it; it = it->next)
    {
        if(it->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) it;
        mo->state = RESTORE_STATE(mo->state);
        mo->info = &mobjinfo[(int) mo->info];
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        for(k = 0; k < NUMPSPRITES; ++k)
            players[i].psprites[k].state =
                RESTORE_STATE(players[i].psprites[k].state);
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
        P_Init();
        //// \fixme Detect gamemode changes (GM_DOOM -> GM_DOOM2, for instance).
#if !__JHEXEN__
        XG_Update();
#endif

#if !__JDOOM__
        ST_Init();
#endif

        Cht_Init();
        MN_Init();

#if __JHEXEN__
        S_InitScript();
#endif

#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__
        S_LevelMusic();
#endif
        break;

    case DD_RENDER_RESTART_PRE:
        // Free the menufog texture.
        M_UnloadData();
        // Free the automap marker patches.
        AM_UnloadData();
        break;

    case DD_RENDER_RESTART_POST:
        // Reload the menufog texture.
        M_LoadData();
        // Reload the automap marker patches.
        AM_LoadData();
        break;
    }
}


static char *ScanWord(char *ptr, char *buf)
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
