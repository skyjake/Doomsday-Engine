/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_switch.c: Switches, buttons. Two-state animation. Exits.
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "d_net.h"
#include "dmu_lib.h"
#include "p_plat.h"
#include "p_switch.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

button_t* buttonlist;

#if __JHEXEN__
switchlist_t switchInfo[] = {
    {"SW_1_UP", "SW_1_DN", SFX_SWITCH1},
    {"SW_2_UP", "SW_2_DN", SFX_SWITCH1},
    {"VALVE1", "VALVE2", SFX_VALVE_TURN},
    {"SW51_OFF", "SW51_ON", SFX_SWITCH2},
    {"SW52_OFF", "SW52_ON", SFX_SWITCH2},
    {"SW53_UP", "SW53_DN", SFX_ROPE_PULL},
    {"PUZZLE5", "PUZZLE9", SFX_SWITCH1},
    {"PUZZLE6", "PUZZLE10", SFX_SWITCH1},
    {"PUZZLE7", "PUZZLE11", SFX_SWITCH1},
    {"PUZZLE8", "PUZZLE12", SFX_SWITCH1},

    {"\0", "\0", 0}
};
#else
// This array is treated as a hardcoded replacement for data that can be loaded
// from a lump, so we need to use little-endian byte ordering.
switchlist_t switchInfo[] = {
# if __JHERETIC__
    {"SW1OFF", "SW1ON", MACRO_SHORT(1)},
    {"SW2OFF", "SW2ON", MACRO_SHORT(1)},
# elif __JDOOM64__
    {"SWXCA", "SWXCB", MACRO_SHORT(1)},
    {"SWXCKA", "SWXCKB", MACRO_SHORT(1)},
    {"SWXCKLA", "SWXCKLB", MACRO_SHORT(1)},
    {"SWXCLA", "SWXCLB", MACRO_SHORT(1)},
    {"SWXHCA", "SWXHCB", MACRO_SHORT(1)},
    {"SWXSAA", "SWXSAB", MACRO_SHORT(1)},
    {"SWXSCA", "SWXSCB", MACRO_SHORT(1)},
    {"SWXSDA", "SWXSDB", MACRO_SHORT(1)},
    {"SWXSEA", "SWXSEB", MACRO_SHORT(1)},
    {"SWXSFA", "SWXSFB", MACRO_SHORT(1)},
    {"SWXSGA", "SWXSGB", MACRO_SHORT(1)},
# elif __JDOOM__
    // Doom shareware episode 1 switches
    {"SW1BRCOM", "SW2BRCOM", MACRO_SHORT(1)},
    {"SW1BRN1", "SW2BRN1", MACRO_SHORT(1)},
    {"SW1BRN2", "SW2BRN2", MACRO_SHORT(1)},
    {"SW1BRNGN", "SW2BRNGN", MACRO_SHORT(1)},
    {"SW1BROWN", "SW2BROWN", MACRO_SHORT(1)},
    {"SW1COMM", "SW2COMM", MACRO_SHORT(1)},
    {"SW1COMP", "SW2COMP", MACRO_SHORT(1)},
    {"SW1DIRT", "SW2DIRT", MACRO_SHORT(1)},
    {"SW1EXIT", "SW2EXIT", MACRO_SHORT(1)},
    {"SW1GRAY", "SW2GRAY", MACRO_SHORT(1)},
    {"SW1GRAY1", "SW2GRAY1", MACRO_SHORT(1)},
    {"SW1METAL", "SW2METAL", MACRO_SHORT(1)},
    {"SW1PIPE", "SW2PIPE", MACRO_SHORT(1)},
    {"SW1SLAD", "SW2SLAD", MACRO_SHORT(1)},
    {"SW1STARG", "SW2STARG", MACRO_SHORT(1)},
    {"SW1STON1", "SW2STON1", MACRO_SHORT(1)},
    {"SW1STON2", "SW2STON2", MACRO_SHORT(1)},
    {"SW1STONE", "SW2STONE", MACRO_SHORT(1)},
    {"SW1STRTN", "SW2STRTN", MACRO_SHORT(1)},

    // Doom registered episodes 2&3 switches
    {"SW1BLUE", "SW2BLUE", MACRO_SHORT(2)},
    {"SW1CMT", "SW2CMT", MACRO_SHORT(2)},
    {"SW1GARG", "SW2GARG", MACRO_SHORT(2)},
    {"SW1GSTON", "SW2GSTON", MACRO_SHORT(2)},
    {"SW1HOT", "SW2HOT", MACRO_SHORT(2)},
    {"SW1LION", "SW2LION", MACRO_SHORT(2)},
    {"SW1SATYR", "SW2SATYR", MACRO_SHORT(2)},
    {"SW1SKIN", "SW2SKIN", MACRO_SHORT(2)},
    {"SW1VINE", "SW2VINE", MACRO_SHORT(2)},
    {"SW1WOOD", "SW2WOOD", MACRO_SHORT(2)},

    // Doom II switches
    {"SW1PANEL", "SW2PANEL", MACRO_SHORT(3)},
    {"SW1ROCK", "SW2ROCK", MACRO_SHORT(3)},
    {"SW1MET2", "SW2MET2", MACRO_SHORT(3)},
    {"SW1WDMET", "SW2WDMET", MACRO_SHORT(3)},
    {"SW1BRIK", "SW2BRIK", MACRO_SHORT(3)},
    {"SW1MOD1", "SW2MOD1", MACRO_SHORT(3)},
    {"SW1ZIM", "SW2ZIM", MACRO_SHORT(3)},
    {"SW1STON6", "SW2STON6", MACRO_SHORT(3)},
    {"SW1TEK", "SW2TEK", MACRO_SHORT(3)},
    {"SW1MARB", "SW2MARB", MACRO_SHORT(3)},
    {"SW1SKULL", "SW2SKULL", MACRO_SHORT(3)},
# endif

    {"\0", "\0", MACRO_SHORT(0)}
};
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static material_t** switchlist;
static int max_numswitches;
static int numswitches;

// CODE --------------------------------------------------------------------

/**
 * Called at game initialization or when the engine's state must be updated
 * (eg a new WAD is loaded at runtime). This routine will populate the list
 * of known switches and buttons. This enables their texture to change when
 * activated, and in the case of buttons, change back after a timeout.
 */
#if __JHEXEN__
void P_InitSwitchList(void)
{
    int     i;
    int     index;

    for(index = 0, i = 0; ; ++i)
    {
        if(index+1 >= max_numswitches)
        {
            switchlist = realloc(switchlist, sizeof(*switchlist) *
                (max_numswitches = max_numswitches ? max_numswitches*2 : 8));
        }

        if(!switchInfo[i].soundID)
            break;

        switchlist[index++] = P_ToPtr(DMU_MATERIAL,
            P_MaterialCheckNumForName(switchInfo[i].name1, MN_TEXTURES));
        switchlist[index++] = P_ToPtr(DMU_MATERIAL,
            P_MaterialCheckNumForName(switchInfo[i].name2, MN_TEXTURES));
    }

    numswitches = index / 2;
    switchlist[index] = NULL;
}
#else

/**
 * Called at game initialization or when the engine's state must be updated
 * (eg a new WAD is loaded at runtime). This routine will populate the list
 * of known switches and buttons. This enables their texture to change when
 * activated, and in the case of buttons, change back after a timeout.
 *
 * This routine modified to read its data from a predefined lump or
 * PWAD lump called SWITCHES rather than a static table in this module to
 * allow wad designers to insert or modify switches.
 *
 * Lump format is an array of byte packed switchlist_t structures, terminated
 * by a structure with episode == -0. The lump can be generated from a
 * text source file using SWANTBLS.EXE, distributed with the BOOM utils.
 * The standard list of switches and animations is contained in the example
 * source text file DEFSWANI.DAT also in the BOOM util distribution.
 */

/*
 * DJS - We'll support this BOOM extension but we should discourage it's use
 *       and instead implement a better method for creating new switches.
 */
void P_InitSwitchList(void)
{
    int         i, index, episode;
    int         lump = W_CheckNumForName("SWITCHES");
    switchlist_t *sList = switchInfo;

# if __JHERETIC__
    if(gs.gameMode == shareware)
        episode = 1;
    else
        episode = 2;
# else
#  if __JDOOM__
    if(gs.gameMode == registered || gs.gameMode == retail)
        episode = 2;
    else if(gs.gameMode == commercial)
        episode = 3;
    else
#  endif
        episode = 1;
# endif

    // Has a custom SWITCHES lump been loaded?
    if(lump > 0)
    {
        Con_Message("P_InitSwitchList: \"SWITCHES\" lump found. Reading switches...\n");
        sList = (switchlist_t *) W_CacheLumpNum(lump, PU_STATIC);
    }

    for(index = 0, i = 0; ; ++i)
    {
        if(index+1 >= max_numswitches)
        {
            switchlist = realloc(switchlist, sizeof(*switchlist) *
                (max_numswitches = max_numswitches ? max_numswitches*2 : 8));
        }

        if(SHORT(sList[i].episode) <= episode)
        {
            if(!SHORT(sList[i].episode))
                break;

            switchlist[index++] = P_ToPtr(DMU_MATERIAL,
                P_MaterialNumForName(sList[i].name1, MN_TEXTURES));
            switchlist[index++] = P_ToPtr(DMU_MATERIAL,
                P_MaterialNumForName(sList[i].name2, MN_TEXTURES));
            VERBOSE(Con_Message("P_InitSwitchList: ADD (\"%s\" | \"%s\" #%d)\n",
                                sList[i].name1, sList[i].name2,
                                SHORT(sList[i].episode)));
        }
    }

    numswitches = index / 2;
    switchlist[index] = NULL;
}
#endif

/**
 * Start a button (retriggerable switch) counting down till it turns off.
 *
 * Passed the linedef the button is on, which texture on the sidedef contains
 * the button, the texture number of the button, and the time the button is
 * to remain active in gametics.
 */
void P_StartButton(linedef_t* line, linesection_t section, material_t* mat,
                   int time)
{
    button_t*           button;

    // See if button is already pressed
    for(button = buttonlist; button; button = button->next)
    {
        if(button->timer && button->line == line)
            return;
    }

    for(button = buttonlist; button; button = button->next)
    {
        if(!button->timer) // use first unused element of list
        {
            button->line = line;
            button->section = section;
            button->material = mat;
            button->timer = time;
            button->soundOrg =
                P_GetPtrp(P_GetPtrp(line, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
            return;
        }
    }

    button = malloc(sizeof(button_t));
    button->line = line;
    button->section = section;
    button->material = mat;
    button->timer = time;
    button->soundOrg =
        P_GetPtrp(P_GetPtrp(line, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);

    if(buttonlist)
        button->next = buttonlist;
    else
        button->next = NULL;

    buttonlist = button;
}

/**
 * Function that changes wall texture.
 * Tell it if switch is ok to use again (1=yes, it's a button).
 */
void P_ChangeSwitchMaterial(linedef_t* line, int useAgain)
{
    int                 i;
    material_t*         topMaterial, *middleMaterial, *bottomMaterial;
#if !__JHEXEN__
    int                 sound;
#endif
    sidedef_t*          sdef = P_GetPtrp(line, DMU_SIDEDEF0);
    sector_t*           frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);

#if !__JHEXEN__
    if(!useAgain)
        P_ToXLine(line)->special = 0;
#endif

    topMaterial = P_GetPtrp(sdef, DMU_TOP_MATERIAL);
    middleMaterial = P_GetPtrp(sdef, DMU_MIDDLE_MATERIAL);
    bottomMaterial = P_GetPtrp(sdef, DMU_BOTTOM_MATERIAL);

#if !__JHEXEN__
# if __JHERETIC__
    sound = SFX_SWITCH;
# else
    sound = SFX_SWTCHN;
#endif

    // EXIT SWITCH?
# if !__JHERETIC__
    if(P_ToXLine(line)->special == 11)
    {
        sound = SFX_SWTCHX;
    }
# endif
#endif

    for(i = 0; i < numswitches * 2; ++i)
    {
        if(switchlist[i] == topMaterial)
        {
#if __JHEXEN__
            S_StartSound(switchInfo[i / 2].soundID,
                         P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));
#else
            S_StartSound(sound, P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));
#endif
            P_SetPtrp(sdef, DMU_TOP_MATERIAL, switchlist[i^1]);

            if(useAgain)
                P_StartButton(line, LS_TOP, switchlist[i], BUTTONTIME);

            return;
        }
        else if(switchlist[i] == middleMaterial)
        {
#if __JHEXEN__
            S_StartSound(switchInfo[i / 2].soundID,
                         P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));
#else
            S_StartSound(sound, P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));
#endif
            P_SetPtrp(sdef, DMU_MIDDLE_MATERIAL, switchlist[i^1]);

            if(useAgain)
                P_StartButton(line, LS_MIDDLE, switchlist[i], BUTTONTIME);

            return;
        }
        else if(switchlist[i] == bottomMaterial)
        {
#if __JHEXEN__
            S_StartSound(switchInfo[i / 2].soundID,
                         P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));
#else
            S_StartSound(sound, P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));
#endif
            P_SetPtrp(sdef, DMU_BOTTOM_MATERIAL, switchlist[i^1]);

            if(useAgain)
                P_StartButton(line, LS_BOTTOM, switchlist[i], BUTTONTIME);

            return;
        }
    }
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
/**
 * Called when a mobj "uses" a linedef.
 */
boolean P_UseSpecialLine(mobj_t* mo, linedef_t* line, int side)
{
    // Extended functionality overrides old.
    if(XL_UseLine(line, side, mo))
        return true;

    return P_UseSpecialLine2(mo, line, side);
}
#endif
