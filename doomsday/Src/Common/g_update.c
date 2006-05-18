/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * Routines to call when updating the state of the engine
 * (when loading/unloading WADs and definitions).
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
# include "doomdef.h"
# include "doomstat.h"
# include "p_setup.h"
# include "p_local.h"
# include "m_menu.h"
# include "dstrings.h"
# include "s_sound.h"
#elif __JHERETIC__
# include "jHeretic/Doomdef.h"
# include "jHeretic/h_stat.h"
# include "jHeretic/P_local.h"
# include "jHeretic/m_menu.h"
# include "jHeretic/Dstrings.h"
# include "jHeretic/Soundst.h"
#elif __JHEXEN__
# include "jHexen/h2def.h"
# include "jHexen/p_local.h"
# include "jHexen/st_stuff.h"
#elif __JSTRIFE__
# include "jStrife/h2def.h"
# include "jStrife/p_local.h"
# include "jStrife/st_stuff.h"
#endif

#include <ctype.h>
#include "hu_pspr.h"

// MACROS ------------------------------------------------------------------

#if __JHEXEN__ || __JSTRIFE__
#  define thinkercap                    (*gi.thinkercap)
#endif

#define MANGLE_STATE(x)     ((state_t*) ((x)? (x)-states : -1))
#define RESTORE_STATE(x)    ((int)(x)==-1? NULL : &states[(int)(x)])

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#if __JHEXEN__
void    S_InitScript(void);
#endif

#ifndef __JDOOM__
void    M_LoadData(void);
void    M_UnloadData(void);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void G_SetGlowing(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Called before the engine re-inits the definitions. After that all the
 * state, info, etc. pointers will be obsolete.
 */
void G_MangleState(void)
{
    thinker_t *it;
    mobj_t *mo;
    int     i, k;

    for(it = thinkercap.next; it != &thinkercap && it; it = it->next)
    {
        if(it->function != P_MobjThinker)
            continue;
        mo = (mobj_t *) it;
        mo->state = MANGLE_STATE(mo->state);
        mo->info = (mobjinfo_t *) (mo->info - mobjinfo);
    }
    for(i = 0; i < MAXPLAYERS; i++)
        for(k = 0; k < NUMPSPRITES; k++)
            players[i].psprites[k].state =
                MANGLE_STATE(players[i].psprites[k].state);
}

void G_RestoreState(void)
{
    thinker_t *it;
    mobj_t *mo;
    int     i, k;

    for(it = thinkercap.next; it != &thinkercap && it; it = it->next)
    {
        if(it->function != P_MobjThinker)
            continue;
        mo = (mobj_t *) it;
        mo->state = RESTORE_STATE(mo->state);
        mo->info = &mobjinfo[(int) mo->info];
    }
    for(i = 0; i < MAXPLAYERS; i++)
        for(k = 0; k < NUMPSPRITES; k++)
            players[i].psprites[k].state =
                RESTORE_STATE(players[i].psprites[k].state);
    HU_UpdatePsprites();
}

/*
 * Handles engine updates and renderer restarts.
 */
void G_UpdateState(int step)
{
    switch (step)
    {
    case DD_GAME_MODE:
        // Set the game mode string.
#ifdef __JDOOM__
        D_IdentifyVersion();
#elif __JHERETIC__
        H_IdentifyVersion();
#else                           // __JHEXEN__
        H2_IdentifyVersion();
#endif
        break;

    case DD_PRE:
        G_MangleState();
        break;

    case DD_POST:
        G_RestoreState();
        P_Init();
#if __JDOOM__
        // FIXME: Detect gamemode changes (doom -> doom2, for instance).
        XG_Update();
        MN_Init();
        S_LevelMusic();
#elif __JHERETIC__
        XG_Update();
        ST_Init();              // Updates the status bar patches.
        MN_Init();
        S_LevelMusic();
#elif __JHEXEN__
        ST_Init();              // Updates the status bar patches.
        MN_Init();
        S_InitScript();
        SN_InitSequenceScript();
#elif __JSTRIFE__
        XG_Update();
        ST_Init();              // Updates the status bar patches.
        MN_Init();
        S_LevelMusic();
#endif
        G_SetGlowing();
        break;

    case DD_RENDER_RESTART_PRE:
        // Free the menufog texture.
        M_UnloadData();
        break;

    case DD_RENDER_RESTART_POST:
        // Reload the menufog texture.
        M_LoadData();
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

/*
 * As long as the -noglow commandline flag has not been set this will
 * retrieve the TXT_RENDER_GLOWFLATS & TXT_RENDER_GLOWTEXTURES strings
 * from the engine and register the flats/textures contained in them
 * as "glowing" textures in Doomsday.
 *
 * Each string contains the lump names of individual FLATS/TEXTURES
 * respectively. Each name is delimited by whitespace.
 */
void G_SetGlowing(void)
{
    char   *ptr;
    char    buf[50];

    if(!ArgCheck("-noglow"))
    {
        ptr = GET_TXT(TXT_RENDER_GLOWFLATS);
        // Set some glowing textures.
        for(ptr = ScanWord(ptr, buf); *buf; ptr = ScanWord(ptr, buf))
        {
            // Is there such a flat?
            if(W_CheckNumForName(buf) == -1)
                continue;
            Set(DD_TEXTURE_GLOW,
                DD_TGLOW_PARM(R_FlatNumForName(buf), false, true));
        }

        ptr = GET_TXT(TXT_RENDER_GLOWTEXTURES);
        for(ptr = ScanWord(ptr, buf); *buf; ptr = ScanWord(ptr, buf))
        {
            // Is there such a texture?
            if(R_CheckTextureNumForName(buf) == -1)
                continue;
            Set(DD_TEXTURE_GLOW,
                DD_TGLOW_PARM(R_TextureNumForName(buf), true, true));
        }
    }
}
