/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
    int i, index;
    ddstring_t path;
    Uri* uri = Uri_New();
    Uri_SetScheme(uri, MN_TEXTURES_NAME);

    Str_Init(&path);

    for(index = 0, i = 0; ; ++i)
    {
        if(index+1 >= max_numswitches)
        {
            switchlist = realloc(switchlist, sizeof(*switchlist) *
                (max_numswitches = max_numswitches ? max_numswitches*2 : 8));
        }

        if(!switchInfo[i].soundID) break;

        Str_PercentEncode(Str_StripRight(Str_Set(&path, switchInfo[i].name1)));
        Uri_SetPath(uri, Str_Text(&path));
        switchlist[index++] = P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));

        Str_PercentEncode(Str_StripRight(Str_Set(&path, switchInfo[i].name2)));
        Uri_SetPath(uri, Str_Text(&path));
        switchlist[index++] = P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    }
    Str_Free(&path);
    Uri_Delete(uri);

    numswitches = index / 2;
    switchlist[index] = 0;
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
    int i, index, episode;
    lumpnum_t lumpNum = W_CheckLumpNumForName2("SWITCHES", true);
    switchlist_t* sList = switchInfo;
    ddstring_t path;
    Uri* uri;

# if __JHERETIC__
    if(gameMode == heretic_shareware)
        episode = 1;
    else
        episode = 2;
# else
#  if __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM^GM_DOOM_SHAREWARE))
        episode = 2;
    else if(gameModeBits & GM_ANY_DOOM2)
        episode = 3;
    else
#  endif
        episode = 1;
# endif

    // Has a custom SWITCHES lump been loaded?
    if(lumpNum > 0)
    {
        VERBOSE( Con_Message("Processing lump %s::SWITCHES...\n", F_PrettyPath(W_LumpSourceFile(lumpNum))) );
        sList = (switchlist_t*) W_CacheLump(lumpNum, PU_GAMESTATIC);
    }
    else
    {
        VERBOSE( Con_Message("Registering default switches...\n") );
    }

    uri = Uri_New();
    Uri_SetScheme(uri, MN_TEXTURES_NAME);

    Str_Init(&path);
    for(index = 0, i = 0; ; ++i)
    {
        if(index+1 >= max_numswitches)
        {
            switchlist = realloc(switchlist, sizeof(*switchlist) * (max_numswitches = max_numswitches ? max_numswitches*2 : 8));
        }

        if(SHORT(sList[i].episode) <= episode)
        {
            if(!SHORT(sList[i].episode)) break;

            Str_PercentEncode(Str_StripRight(Str_Set(&path, sList[i].name1)));
            Uri_SetPath(uri, Str_Text(&path));
            switchlist[index++] = P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));

            Str_PercentEncode(Str_StripRight(Str_Set(&path, sList[i].name2)));
            Uri_SetPath(uri, Str_Text(&path));
            switchlist[index++] = P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
            if(verbose > (lumpNum > 0? 1 : 2))
            {
                Con_Message("  %d: Epi:%d A:\"%s\" B:\"%s\"\n", i, SHORT(sList[i].episode), sList[i].name1, sList[i].name2);
            }
        }
    }

    Str_Free(&path);
    Uri_Delete(uri);

    if(lumpNum > 0)
        W_CacheChangeTag(lumpNum, PU_CACHE);

    numswitches = index / 2;
    switchlist[index] = 0;
}
#endif

material_t* P_GetSwitch(material_t* mat, const switchlist_t** info)
{
    int                 i;

    if(!mat)
        return NULL;

    for(i = 0; i < numswitches * 2; ++i)
    {
        if(switchlist[i] == mat)
        {
            if(info)
                *info = &switchInfo[i / 2];

            return switchlist[i^1];
        }
    }

    return NULL;
}

void T_MaterialChanger(materialchanger_t* mchanger)
{
    if(!(--mchanger->timer))
    {
        P_SetPtrp(mchanger->side,
                  mchanger->ssurfaceID == SID_MIDDLE? DMU_MIDDLE_MATERIAL :
                  mchanger->ssurfaceID == SID_TOP? DMU_TOP_MATERIAL :
                  DMU_BOTTOM_MATERIAL, mchanger->material);
#if __JDOOM__ || __JDOOM64__
        S_StartSound(SFX_SWTCHN, P_GetPtrp(
            P_GetPtrp(mchanger->side, DMU_SECTOR), DMU_BASE));
#elif __JHERETIC__
        S_StartSound(SFX_SWITCH, P_GetPtrp(
            P_GetPtrp(mchanger->side, DMU_SECTOR), DMU_BASE));
#endif

        DD_ThinkerRemove(&mchanger->thinker);
    }
}

void P_SpawnMaterialChanger(SideDef* side, sidedefsurfaceid_t ssurfaceID,
                            material_t* mat, int tics)
{
    materialchanger_t*  mchanger;

    mchanger = Z_Calloc(sizeof(*mchanger), PU_MAP, 0);
    mchanger->thinker.function = T_MaterialChanger;
    DD_ThinkerAdd(&mchanger->thinker);

    mchanger->side = side;
    mchanger->ssurfaceID = ssurfaceID;
    mchanger->material = mat;
    mchanger->timer = tics;
}

typedef struct {
    SideDef*            side;
    sidedefsurfaceid_t  ssurfaceID;
} findmaterialchangerparams_t;

int findMaterialChanger(thinker_t* th, void* data)
{
    materialchanger_t*  mchanger = (materialchanger_t*) th;
    findmaterialchangerparams_t* params =
        (findmaterialchangerparams_t*) data;

    if(mchanger->side == params->side &&
       mchanger->ssurfaceID == params->ssurfaceID)
        return true; // Stop iteration.

    return false; // Keep looking.
}

void P_StartButton(SideDef* side, sidedefsurfaceid_t ssurfaceID,
                   material_t* mat, int tics)
{
    findmaterialchangerparams_t params;

    params.side = side;
    params.ssurfaceID = ssurfaceID;

    // See if a material change has already been queued.
    if(DD_IterateThinkers(T_MaterialChanger, findMaterialChanger, &params))
        return;

    P_SpawnMaterialChanger(side, ssurfaceID, mat, tics);
}

/**
 * @param side          Sidedef where the surface to be changed is found.
 * @param ssurfaceID    Id of the sidedef surface to be changed.
 * @param sound         If non-zero, play this sound, ELSE the sound to
 *                      play will be taken from the switchinfo. Note that
 *                      a sound will play iff a switch state change occurs
 *                      and param silent is non-zero.
 * @param silent        @c true = no sound will be played.
 * @param tics          @c <= 0 = A permanent change.
 *                      @c  > 0 = Change back after this many tics.
 */
boolean P_ToggleSwitch2(SideDef* side, sidedefsurfaceid_t ssurfaceID,
                        int sound, boolean silent, int tics)
{
    material_t*         mat, *current;
    const switchlist_t* info;

    current = P_GetPtrp(side,
                        ssurfaceID == SID_MIDDLE? DMU_MIDDLE_MATERIAL :
                        ssurfaceID == SID_TOP? DMU_TOP_MATERIAL :
                        DMU_BOTTOM_MATERIAL);

    if((mat = P_GetSwitch(current, &info)))
    {
        if(!silent)
        {
            // \todo Get these defaults from switchinfo.
            if(!sound)
            {
#if __JHEXEN__
                sound = info->soundID;
#elif __JHERETIC__
                sound = SFX_SWITCH;
#else
                sound = SFX_SWTCHN;
#endif
            }

            S_StartSound(sound, P_GetPtrp(P_GetPtrp(side, DMU_SECTOR), DMU_BASE));
        }

        P_SetPtrp(side, ssurfaceID == SID_MIDDLE? DMU_MIDDLE_MATERIAL :
                        ssurfaceID == SID_TOP? DMU_TOP_MATERIAL :
                        DMU_BOTTOM_MATERIAL, mat);
        if(tics > 0)
            P_StartButton(side, ssurfaceID, current, tics);

        return true;
    }

    return false;
}

/**
 * @param side          Sidedef where the switch to be changed is found.
 * @param sound         If non-zero, play this sound, ELSE the sound to
 *                      play will be taken from the switchinfo. Note that
 *                      a sound will play iff a switch state change occurs
 *                      and param silent is non-zero.
 * @param silent        @c true = no sound will be played.
 * @param tics          @c <= 0 = A permanent change.
 *                      @c  > 0 = Change back after this many tics.
 */
boolean P_ToggleSwitch(SideDef* side, int sound, boolean silent, int tics)
{
    if(P_ToggleSwitch2(side, SID_TOP, sound, silent, tics))
        return true;

    if(P_ToggleSwitch2(side, SID_MIDDLE, sound, silent, tics))
        return true;

    if(P_ToggleSwitch2(side, SID_BOTTOM, sound, silent, tics))
        return true;

    return false;
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
/**
 * Called when a mobj "uses" a linedef.
 */
boolean P_UseSpecialLine(mobj_t* mo, LineDef* line, int side)
{
    // Extended functionality overrides old.
    if(XL_UseLine(line, side, mo))
        return true;

    return P_UseSpecialLine2(mo, line, side);
}
#endif
