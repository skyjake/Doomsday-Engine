/** @file p_switch.cpp  Common playsim routines relating to switches.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 * @authors Copyright © 1993-1996 id Software, Inc.
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
#include "p_switch.h"

#include "d_net.h"
#include "dmu_lib.h"
#include "dmu_archiveindex.h"
#include "p_plat.h"
#include "p_sound.h"
#include "p_saveg.h"
#include <de/legacy/memory.h>

/**
 * This struct is used to provide byte offsets when reading a custom
 * SWITCHES lump thus it must be packed and cannot be altered.
 */
#pragma pack(1)
typedef struct {
    /* Do NOT change these members in any way! */
    char name1[9];
    char name2[9];
#if __JHEXEN__
    int soundID;
#else
    short episode;
#endif
} switchlist_t;
#pragma pack()

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

static world_Material **switchlist; /// @todo fixme: Never free'd!
static int max_numswitches;
static int numswitches;

#if __JHEXEN__
void P_InitSwitchList()
{
    uri_s *uri = Uri_NewWithPath2("Textures:", RC_NULL);

    AutoStr *path = AutoStr_NewStd();

    int index = 0;
    for(int i = 0; ; ++i)
    {
        if(index+1 >= max_numswitches)
        {
            switchlist = (world_Material **) M_Realloc(switchlist, sizeof(*switchlist) *
                (max_numswitches = max_numswitches ? max_numswitches*2 : 8));
        }

        if(!switchInfo[i].soundID) break;

        Str_PercentEncode(Str_StripRight(Str_Set(path, switchInfo[i].name1)));
        Uri_SetPath(uri, Str_Text(path));
        switchlist[index++] = (world_Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));

        Str_PercentEncode(Str_StripRight(Str_Set(path, switchInfo[i].name2)));
        Uri_SetPath(uri, Str_Text(path));
        switchlist[index++] = (world_Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
    }
    Uri_Delete(uri);

    numswitches = index / 2;
    switchlist[index] = 0;
}
#else
/**
 * This routine modified to read its data from a predefined lump or
 * PWAD lump called SWITCHES rather than a static table in this module to
 * allow wad designers to insert or modify switches.
 *
 * Lump format is an array of byte packed switchlist_t structures, terminated
 * by a structure with episode == -0. The lump can be generated from a
 * text source file using SWANTBLS.EXE, distributed with the BOOM utils.
 * The standard list of switches and animations is contained in the example
 * source text file DEFSWANI.DAT also in the BOOM util distribution.
 *
 * @todo Implement a better method for creating new switches.
 */
void P_InitSwitchList()
{
    int episode = 0;
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

    switchlist_t *sList = switchInfo;

    // Has a custom SWITCHES lump been loaded?
    res::File1 *lump = 0;
    if(CentralLumpIndex().contains("SWITCHES.lmp"))
    {
        lump = &CentralLumpIndex()[CentralLumpIndex().findLast("SWITCHES.lmp")];
        App_Log(DE2_RES_VERBOSE, "Processing lump %s::SWITCHES", F_PrettyPath(lump->container().composePath()));
        sList = (switchlist_t *) lump->cache();
    }
    else
    {
        App_Log(DE2_RES_VERBOSE, "Registering default switches...");
    }

    uri_s *uri = Uri_New();
    Uri_SetScheme(uri, "Textures");

    ddstring_t path; Str_Init(&path);
    int index = 0;
    for(int i = 0; ;++i)
    {
        if(index + 1 >= max_numswitches)
        {
            switchlist = (world_Material **) M_Realloc(switchlist, sizeof(*switchlist) * (max_numswitches = max_numswitches ? max_numswitches*2 : 8));
        }

        if(DD_SHORT(sList[i].episode) <= episode)
        {
            if(!DD_SHORT(sList[i].episode)) break;

            Str_PercentEncode(Str_StripRight(Str_Set(&path, sList[i].name1)));
            Uri_SetPath(uri, Str_Text(&path));
            switchlist[index++] = (world_Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));

            Str_PercentEncode(Str_StripRight(Str_Set(&path, sList[i].name2)));
            Uri_SetPath(uri, Str_Text(&path));
            switchlist[index++] = (world_Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));

            App_Log(lump? DE2_RES_VERBOSE : DE2_RES_XVERBOSE,
                    "  %d: Epi:%d A:\"%s\" B:\"%s\"", i, DD_SHORT(sList[i].episode),
                    sList[i].name1, sList[i].name2);
        }
    }

    Str_Free(&path);
    Uri_Delete(uri);

    if(lump)
    {
        lump->unlock();
    }

    numswitches = index / 2;
    switchlist[index] = 0;
}
#endif

static world_Material *findSwitch(world_Material *mat, const switchlist_t** info)
{
    if (!mat) return 0;

    for (int i = 0; i < numswitches * 2; ++i)
    {
        if (switchlist[i] == mat)
        {
            if (info)
            {
                *info = &switchInfo[i / 2];
            }
            return switchlist[i^1];
        }
    }

    return 0;
}

static void playSwitchSound(Side *side, uint sectionFlags, int sound)
{
    if (cfg.common.switchSoundOrigin == 1 /* vanilla behavior */)
    {
        S_SectorSound(reinterpret_cast<Sector *>(P_GetPtrp(side, DMU_SECTOR)), sound);
    }
    else
    {
        const mobj_t *sideEmitter = reinterpret_cast<const mobj_t *>
                (P_GetPtrp(side, DMU_EMITTER | sectionFlags));
        S_StopSound(0, sideEmitter);
        S_StartSound(sound, sideEmitter);
    }
}

void T_MaterialChanger(void *materialChangerThinker)
{
    materialchanger_t *mchanger = (materialchanger_t *)materialChangerThinker;

    if(!(--mchanger->timer))
    {
        const uint sectionFlags = DMU_FLAG_FOR_SIDESECTION(mchanger->section);

        P_SetPtrp(mchanger->side, sectionFlags | DMU_MATERIAL, mchanger->material);

        const int sound =
#if __JDOOM__ || __JDOOM64__
            SFX_SWTCHN;
#elif __JHERETIC__
            SFX_SWITCH;
#else
            0;
#endif
        if (sound > 0)
        {
            playSwitchSound(mchanger->side, sectionFlags, sound);
        }
        Thinker_Remove(&mchanger->thinker);
    }
}

void materialchanger_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    // Write a type byte. For future use (e.g., changing plane surface
    // materials as well as side surface materials).
    Writer_WriteByte(writer, 0);
    Writer_WriteInt32(writer, timer);
    Writer_WriteInt32(writer, P_ToIndex(side));
    Writer_WriteByte(writer, (byte) section);
    Writer_WriteInt16(writer, msw->serialIdFor(material));
}

int materialchanger_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    /*int ver =*/ Reader_ReadByte(reader);
    // Note: the thinker class byte has already been read.

    /*byte type =*/ Reader_ReadByte(reader);

    timer = Reader_ReadInt32(reader);

    int sideIndex = (int) Reader_ReadInt32(reader);
    if(mapVersion >= 12)
    {
        side = (Side *)P_ToPtr(DMU_SIDE, sideIndex);
    }
    else
    {
        // Side index is actually a DMU_ARCHIVE_INDEX.
        side = msr->side(sideIndex);
    }
    DE_ASSERT(side != 0);

    section = (SideSection) Reader_ReadByte(reader);
    material = (world_Material *) msr->material(Reader_ReadInt16(reader), 0);

    thinker.function = T_MaterialChanger;

    return true; // Add this thinker.
}

static void spawnMaterialChanger(Side *side, SideSection section, world_Material *mat, int tics)
{
    materialchanger_t *mchanger = (materialchanger_t *)Z_Calloc(sizeof(*mchanger), PU_MAP, 0);
    mchanger->thinker.function = T_MaterialChanger;
    Thinker_Add(&mchanger->thinker);

    mchanger->side     = side;
    mchanger->section  = section;
    mchanger->material = mat;
    mchanger->timer    = tics;
}

struct findmaterialchangerparams_t
{
    Side *side;
    SideSection section;
};

static int findMaterialChanger(thinker_t *th, void *context)
{
    materialchanger_t *mchanger = (materialchanger_t *) th;
    findmaterialchangerparams_t *params = (findmaterialchangerparams_t *) context;

    if(mchanger->side == params->side &&
       mchanger->section == params->section)
        return true; // Stop iteration.

    return false; // Keep looking.
}

static void startButton(Side *side, SideSection section, world_Material *mat, int tics)
{
    findmaterialchangerparams_t parm;
    parm.side    = side;
    parm.section = section;

    // See if a material change has already been queued.
    if (!Thinker_Iterate(T_MaterialChanger, findMaterialChanger, &parm))
    {
        spawnMaterialChanger(side, section, mat, tics);
    }
}

static int chooseDefaultSound(const switchlist_t *info)
{
    /// @todo Get these defaults from switchinfo.
#if __JHEXEN__
    return info->soundID;
#else
#  if __JHERETIC__
    return SFX_SWITCH;
#  else
    return SFX_SWTCHN;
# endif

    DE_UNUSED(info);
#endif
}

dd_bool P_ToggleSwitch2(Side *side, SideSection section, int sound, dd_bool silent, int tics)
{
    const uint sectionFlags = DMU_FLAG_FOR_SIDESECTION(section);
    world_Material *current = reinterpret_cast<world_Material *>
                              (P_GetPtrp(side, sectionFlags | DMU_MATERIAL));

    const switchlist_t *info;
    if (world_Material *mat = findSwitch(current, &info))
    {
        if (!silent)
        {
            // Play the switch sound.
            if (!sound)
            {
                sound = chooseDefaultSound(info);
            }
            playSwitchSound(side, sectionFlags, sound);
        }

        P_SetPtrp(side, sectionFlags | DMU_MATERIAL, mat);

        // Are we changing it back again?
        if (tics > 0)
        {
            // Spawn a deferred material change thinker.
            startButton(side, section, current, tics);
        }

        return true;
    }
    return false;
}

dd_bool P_ToggleSwitch(Side *side, int sound, dd_bool silent, int tics)
{
    if(P_ToggleSwitch2(side, SS_TOP, sound, silent, tics))
        return true;

    if(P_ToggleSwitch2(side, SS_MIDDLE, sound, silent, tics))
        return true;

    if(P_ToggleSwitch2(side, SS_BOTTOM, sound, silent, tics))
        return true;

    return false;
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
dd_bool P_UseSpecialLine(mobj_t *activator, Line *line, int side)
{
    // Extended functionality overrides old.
    if(XL_UseLine(line, side, activator))
    {
        return true;
    }

    return P_UseSpecialLine2(activator, line, side);
}
#endif
