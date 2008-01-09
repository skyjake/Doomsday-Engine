/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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

/**
 * g_update.c: Routines to call when updating the state of the engine
 *
 * \bug Not 64bit clean: In function 'G_RestoreState': cast from pointer to integer of different size
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
#include "hu_menu.h"
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
    int             i, k;
    thinker_t      *it;
    mobj_t         *mo;

    for(it = thinkerCap.next; it != &thinkerCap && it; it = it->next)
    {
        if(it->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) it;
        mo->state = MANGLE_STATE(mo->state);
        mo->info = (mobjinfo_t *) (mo->info - mobjInfo);
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t       *plr = &players[i];

        for(k = 0; k < NUMPSPRITES; ++k)
            plr->pSprites[k].state =
                MANGLE_STATE(plr->pSprites[k].state);
    }
}

void G_RestoreState(void)
{
    int             i, k;
    thinker_t      *it;
    mobj_t         *mo;

    for(it = thinkerCap.next; it != &thinkerCap && it; it = it->next)
    {
        if(it->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) it;
        mo->state = RESTORE_STATE(mo->state);
        mo->info = &mobjInfo[(int) mo->info];
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t       *plr = &players[i];

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
        //// \fixme Detect gamemode changes (GM_DOOM -> GM_DOOM2, for instance).
#if !__JHEXEN__
        XG_Update();
#endif

#if !__JDOOM__
        ST_Init();
#endif

        Cht_Init();
        Hu_MenuInit();

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
