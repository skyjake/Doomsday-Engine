/** @file p_saveg.cpp Common game-save state management. 
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <lzss.h>
#include <cstdio>
#include <cstring>

#include "common.h"

#include <de/memory.h>

#include "g_common.h"
#include "d_net.h"
#include "dmu_lib.h"
#include "fi_lib.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_player.h"
#include "mobj.h"
#include "p_inventory.h"
#include "am_map.h"
#include "p_tick.h"
#include "p_actor.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_plat.h"
#include "p_scroll.h"
#include "p_switch.h"
#include "hu_log.h"
#if __JHERETIC__ || __JHEXEN__
#include "hu_inventory.h"
#endif
#include "r_common.h"
#include "api_materialarchive.h"
#include "p_savedef.h"
#include "dmu_archiveindex.h"
#include "polyobjs.h"
#if __JHEXEN__
#  include "acscript.h"
#endif
#include "p_saveg.h"

using namespace dmu_lib;

#define MAX_HUB_MAPS        99

#define FF_FULLBRIGHT       0x8000 ///< Used to be a flag in thing->frame.
#define FF_FRAMEMASK        0x7fff

#if __JHEXEN__
/// Symbolic identifier used to mark references to players in map states.
static ThingSerialId const TargetPlayerId = -2;
#endif

typedef struct playerheader_s {
    int numPowers;
    int numKeys;
    int numFrags;
    int numWeapons;
    int numAmmoTypes;
    int numPSprites;
#if __JDOOM64__ || __JHERETIC__ || __JHEXEN__
    int numInvItemTypes;
#endif
#if __JHEXEN__
    int numArmorTypes;
#endif
} playerheader_t;

// Thinker Save flags
#define TSF_SERVERONLY          0x01 ///< Only saved by servers.

struct ThinkerClassInfo
{
    thinkerclass_t thinkclass;
    thinkfunc_t function;
    int flags;
    WriteThinkerFunc writeFunc;
    ReadThinkerFunc readFunc;
    size_t size;
};

typedef enum {
    sc_normal,
    sc_ploff, ///< plane offset
#if !__JHEXEN__
    sc_xg1,
#endif
    NUM_SECTORCLASSES
} sectorclass_t;

typedef enum {
    lc_normal,
#if !__JHEXEN__
    lc_xg1,
#endif
    NUM_LINECLASSES
} lineclass_t;

static bool recogniseGameState(Str const *path, SaveInfo *info);

static void SV_WriteMobj(mobj_t const *mobj);
static int SV_ReadMobj(thinker_t *th, int mapVersion);

#if __JHEXEN__
static void SV_WriteMovePoly(polyevent_t const *movepoly);
static int SV_ReadMovePoly(polyevent_t *movepoly, int mapVersion);
#endif

#if __JHEXEN__
static void readMapState(Reader *reader, Str const *path);
#else
static void readMapState(Reader *reader);
#endif

static bool inited = false;

static int cvarLastSlot; // -1 = Not yet loaded/saved in this game session.
static int cvarQuickSlot; // -1 = Not yet chosen/determined.

static SaveInfo **saveInfo;
static SaveInfo *autoSaveInfo;
#if __JHEXEN__
static SaveInfo *baseSaveInfo;
#endif
static SaveInfo *nullSaveInfo;

#if __JHEXEN__
static int mapVersion;
#endif
static saveheader_t const *hdr;

static playerheader_t playerHeader;
static dd_bool playerHeaderOK;

static mobj_t **thingArchive;
static uint thingArchiveSize;
static bool thingArchiveExcludePlayers;

static int saveToRealPlayerNum[MAXPLAYERS];
#if __JHEXEN__
static targetplraddress_t *targetPlayerAddrs;
static byte *saveBuffer;
#else
static int numSoundTargets;
#endif

static MaterialArchive *materialArchive;
static SideArchive *sideArchive;

template <typename Type>
static void writeThinkerAs(thinker_t const *th, Writer *writer)
{
    Type *t = (Type*)th;
    t->write(writer);
}

template <typename Type>
static int readThinkerAs(thinker_t *th, Reader *reader, int mapVersion)
{
    Type *t = (Type *)th;
    return t->read(reader, mapVersion);
}

static ThinkerClassInfo thinkerInfo[] = {
    {
      TC_MOBJ,
      (thinkfunc_t) P_MobjThinker,
      TSF_SERVERONLY,
      (WriteThinkerFunc) SV_WriteMobj,
      (ReadThinkerFunc) SV_ReadMobj,
      sizeof(mobj_t)
    },
#if !__JHEXEN__
    {
      TC_XGMOVER,
      (thinkfunc_t) XS_PlaneMover,
      0,
      (WriteThinkerFunc)writeThinkerAs<xgplanemover_t>,
      (ReadThinkerFunc)readThinkerAs<xgplanemover_t>,
      sizeof(xgplanemover_t)
    },
#endif
    {
      TC_CEILING,
      T_MoveCeiling,
      0,
      (WriteThinkerFunc)writeThinkerAs<ceiling_t>,
      (ReadThinkerFunc)readThinkerAs<ceiling_t>,
      sizeof(ceiling_t)
    },
    {
      TC_DOOR,
      T_Door,
      0,
      (WriteThinkerFunc)writeThinkerAs<door_t>,
      (ReadThinkerFunc)readThinkerAs<door_t>,
      sizeof(door_t)
    },
    {
      TC_FLOOR,
      T_MoveFloor,
      0,
      (WriteThinkerFunc)writeThinkerAs<floor_t>,
      (ReadThinkerFunc)readThinkerAs<floor_t>,
      sizeof(floor_t)
    },
    {
      TC_PLAT,
      T_PlatRaise,
      0,
      (WriteThinkerFunc)writeThinkerAs<plat_t>,
      (ReadThinkerFunc)readThinkerAs<plat_t>,
      sizeof(plat_t)
    },
#if __JHEXEN__
    {
     TC_INTERPRET_ACS,
     (thinkfunc_t) ACScript_Thinker,
     0,
     (WriteThinkerFunc)writeThinkerAs<ACScript>,
     (ReadThinkerFunc)readThinkerAs<ACScript>,
     sizeof(ACScript)
    },
    {
     TC_FLOOR_WAGGLE,
     (thinkfunc_t) T_FloorWaggle,
     0,
     (WriteThinkerFunc)writeThinkerAs<waggle_t>,
     (ReadThinkerFunc)readThinkerAs<waggle_t>,
     sizeof(waggle_t)
    },
    {
     TC_LIGHT,
     (thinkfunc_t) T_Light,
     0,
     (WriteThinkerFunc)writeThinkerAs<light_t>,
     (ReadThinkerFunc)readThinkerAs<light_t>,
     sizeof(light_t)
    },
    {
     TC_PHASE,
     (thinkfunc_t) T_Phase,
     0,
     (WriteThinkerFunc)writeThinkerAs<phase_t>,
     (ReadThinkerFunc)readThinkerAs<phase_t>,
     sizeof(phase_t)
    },
    {
     TC_BUILD_PILLAR,
     (thinkfunc_t) T_BuildPillar,
     0,
     (WriteThinkerFunc)writeThinkerAs<pillar_t>,
     (ReadThinkerFunc)readThinkerAs<pillar_t>,
     sizeof(pillar_t)
    },
    {
     TC_ROTATE_POLY,
     T_RotatePoly,
     0,
     (WriteThinkerFunc)writeThinkerAs<polyevent_t>,
     (ReadThinkerFunc)readThinkerAs<polyevent_t>,
     sizeof(polyevent_t)
    },
    {
     TC_MOVE_POLY,
     T_MovePoly,
     0,
     (WriteThinkerFunc)SV_WriteMovePoly,
     (ReadThinkerFunc)SV_ReadMovePoly,
     sizeof(polyevent_t)
    },
    {
     TC_POLY_DOOR,
     T_PolyDoor,
     0,
     (WriteThinkerFunc)writeThinkerAs<polydoor_t>,
     (ReadThinkerFunc)readThinkerAs<polydoor_t>,
     sizeof(polydoor_t)
    },
#else
    {
      TC_FLASH,
      (thinkfunc_t) T_LightFlash,
      0,
      (WriteThinkerFunc)writeThinkerAs<lightflash_t>,
      (ReadThinkerFunc)readThinkerAs<lightflash_t>,
      sizeof(lightflash_t)
    },
    {
      TC_STROBE,
      (thinkfunc_t) T_StrobeFlash,
      0,
      (WriteThinkerFunc)writeThinkerAs<strobe_t>,
      (ReadThinkerFunc)readThinkerAs<strobe_t>,
      sizeof(strobe_t)
    },
    {
      TC_GLOW,
      (thinkfunc_t) T_Glow,
      0,
      (WriteThinkerFunc)writeThinkerAs<glow_t>,
      (ReadThinkerFunc)readThinkerAs<glow_t>,
      sizeof(glow_t)
    },
# if __JDOOM__ || __JDOOM64__
    {
      TC_FLICKER,
      (thinkfunc_t) T_FireFlicker,
      0,
      (WriteThinkerFunc)writeThinkerAs<fireflicker_t>,
      (ReadThinkerFunc)readThinkerAs<fireflicker_t>,
      sizeof(fireflicker_t)
    },
# endif
# if __JDOOM64__
    {
      TC_BLINK,
      (thinkfunc_t) T_LightBlink,
      0,
      (WriteThinkerFunc)writeThinkerAs<lightblink_t>,
      (ReadThinkerFunc)readThinkerAs<lightblink_t>,
      sizeof(lightblink_t)
    },
# endif
#endif
    {
      TC_MATERIALCHANGER,
      T_MaterialChanger,
      0,
      (WriteThinkerFunc)writeThinkerAs<materialchanger_t>,
      (ReadThinkerFunc)readThinkerAs<materialchanger_t>,
      sizeof(materialchanger_t)
    },
    {
      TC_SCROLL,
      (thinkfunc_t) T_Scroll,
      0,
      (WriteThinkerFunc)writeThinkerAs<scroll_t>,
      (ReadThinkerFunc)readThinkerAs<scroll_t>,
      sizeof(scroll_t)
    },
    { TC_NULL, NULL, 0, NULL, NULL, 0 }
};

void SV_Register()
{
#if !__JHEXEN__
    C_VAR_BYTE("game-save-auto-loadonreborn",   &cfg.loadAutoSaveOnReborn,  0, 0, 1);
#endif
    C_VAR_BYTE("game-save-confirm",             &cfg.confirmQuickGameSave,  0, 0, 1);
    C_VAR_BYTE("game-save-confirm-loadonreborn",&cfg.confirmRebornLoad,     0, 0, 1);
    C_VAR_BYTE("game-save-last-loadonreborn",   &cfg.loadLastSaveOnReborn,  0, 0, 1);
    C_VAR_INT ("game-save-last-slot",           &cvarLastSlot, CVF_NO_MIN|CVF_NO_MAX|CVF_NO_ARCHIVE|CVF_READ_ONLY, 0, 0);
    C_VAR_INT ("game-save-quick-slot",          &cvarQuickSlot, CVF_NO_MAX|CVF_NO_ARCHIVE, -1, 0);

    // Aliases for obsolete cvars:
    C_VAR_BYTE("menu-quick-ask",                &cfg.confirmQuickGameSave, 0, 0, 1);
}

/**
 * Compose the (possibly relative) path to the game-save associated
 * with the logical save @a slot.
 *
 * @param slot  Logical save slot identifier.
 * @param map   If @c >= 0 include this logical map index in the composed path.
 * @return  The composed path if reachable (else a zero-length string).
 */
static AutoStr *composeGameSavePathForSlot2(int slot, int map)
{
    DENG_ASSERT(inited);

    AutoStr *path = AutoStr_NewStd();

    // A valid slot?
    if(!SV_IsValidSlot(slot)) return path;

    // Do we have a valid path?
    if(!F_MakePath(SV_SavePath())) return path;

    // Compose the full game-save path and filename.
    if(map >= 0)
    {
        Str_Appendf(path, "%s" SAVEGAMENAME "%i%02i." SAVEGAMEEXTENSION, SV_SavePath(), slot, map);
    }
    else
    {
        Str_Appendf(path, "%s" SAVEGAMENAME "%i." SAVEGAMEEXTENSION, SV_SavePath(), slot);
    }
    F_TranslatePath(path, path);
    return path;
}

static AutoStr *composeGameSavePathForSlot(int slot)
{
    return composeGameSavePathForSlot2(slot, -1);
}

#if !__JHEXEN__
/**
 * Compose the (possibly relative) path to the game-save associated
 * with @a gameId.
 *
 * @param gameId  Unique game identifier.
 * @return  File path to the reachable save directory. If the game-save path
 *          is unreachable then a zero-length string is returned instead.
 */
static AutoStr *composeGameSavePathForClientGameId(uint gameId)
{
    AutoStr *path = AutoStr_NewStd();
    // Do we have a valid path?
    if(!F_MakePath(SV_ClientSavePath())) return path; // return zero-length string.
    // Compose the full game-save path and filename.
    Str_Appendf(path, "%s" CLIENTSAVEGAMENAME "%08X." SAVEGAMEEXTENSION, SV_ClientSavePath(), gameId);
    F_TranslatePath(path, path);
    return path;
}
#endif

static void clearSaveInfo()
{
    if(saveInfo)
    {
        for(int i = 0; i < NUMSAVESLOTS; ++i)
        {
            SaveInfo_Delete(saveInfo[i]);
        }
        M_Free(saveInfo); saveInfo = 0;
    }

    if(autoSaveInfo)
    {
        SaveInfo_Delete(autoSaveInfo); autoSaveInfo = 0;
    }
#if __JHEXEN__
    if(baseSaveInfo)
    {
        SaveInfo_Delete(baseSaveInfo); baseSaveInfo = 0;
    }
#endif
    if(nullSaveInfo)
    {
        SaveInfo_Delete(nullSaveInfo); nullSaveInfo = 0;
    }
}

static void updateSaveInfo(Str const *path, SaveInfo *info)
{
    if(!info) return;

    if(!path || Str_IsEmpty(path))
    {
        // The save path cannot be accessed for some reason. Perhaps its a
        // network path? Clear the info for this slot.
        SaveInfo_SetName(info, 0);
        SaveInfo_SetGameId(info, 0);
        return;
    }

    // Is this a recognisable save state?
    if(!recogniseGameState(path, info))
    {
        // Clear the info for this slot.
        SaveInfo_SetName(info, 0);
        SaveInfo_SetGameId(info, 0);
        return;
    }

    // Ensure we have a valid name.
    if(Str_IsEmpty(SaveInfo_Name(info)))
    {
        SaveInfo_SetName(info, AutoStr_FromText("UNNAMED"));
    }
}

/// Re-build game-save info by re-scanning the save paths and populating the list.
static void buildSaveInfo()
{
    DENG_ASSERT(inited);

    if(!saveInfo)
    {
        // Not yet been here. We need to allocate and initialize the game-save info list.
        saveInfo = reinterpret_cast<SaveInfo **>(M_Malloc(NUMSAVESLOTS * sizeof(*saveInfo)));

        // Initialize.
        for(int i = 0; i < NUMSAVESLOTS; ++i)
        {
            saveInfo[i] = SaveInfo_New();
        }
        autoSaveInfo = SaveInfo_New();
#if __JHEXEN__
        baseSaveInfo = SaveInfo_New();
#endif
        nullSaveInfo = SaveInfo_New();
    }

    /// Scan the save paths and populate the list.
    /// @todo We should look at all files on the save path and not just those
    /// which match the default game-save file naming convention.
    for(int i = 0; i < NUMSAVESLOTS; ++i)
    {
        SaveInfo *info = saveInfo[i];
        updateSaveInfo(composeGameSavePathForSlot(i), info);
    }
    updateSaveInfo(composeGameSavePathForSlot(AUTO_SLOT), autoSaveInfo);
#if __JHEXEN__
    updateSaveInfo(composeGameSavePathForSlot(BASE_SLOT), baseSaveInfo);
#endif
}

/// Given a logical save slot identifier retrieve the assciated game-save info.
static SaveInfo *findSaveInfoForSlot(int slot)
{
    DENG_ASSERT(inited);

    if(!SV_IsValidSlot(slot)) return nullSaveInfo;

    // On first call - automatically build and populate game-save info.
    if(!saveInfo)
    {
        buildSaveInfo();
    }

    // Retrieve the info for this slot.
    if(slot == AUTO_SLOT) return autoSaveInfo;
#if __JHEXEN__
    if(slot == BASE_SLOT) return baseSaveInfo;
#endif
    return saveInfo[slot];
}

static void replaceSaveInfo(int slot, SaveInfo *newInfo)
{
    DENG_ASSERT(SV_IsValidSlot(slot));

    SaveInfo **destAdr;
    if(slot == AUTO_SLOT)
    {
        destAdr = &autoSaveInfo;
    }
#if __JHEXEN__
    else if(slot == BASE_SLOT)
    {
        destAdr = &baseSaveInfo;
    }
#endif
    else
    {
        destAdr = &saveInfo[slot];
    }

    if(*destAdr) SaveInfo_Delete(*destAdr);
    *destAdr = newInfo;
}

AutoStr *SV_ComposeSlotIdentifier(int slot)
{
    AutoStr *str = AutoStr_NewStd();
    if(slot < 0) return Str_Set(str, "(invalid slot)");
    if(slot == AUTO_SLOT) return Str_Set(str, "<auto>");
#if __JHEXEN__
    if(slot == BASE_SLOT) return Str_Set(str, "<base>");
#endif
    return Str_Appendf(str, "%i", slot);
}

/**
 * Determines whether to announce when the specified @a slot is cleared.
 */
static bool announceOnClearingSlot(int slot)
{
#if _DEBUG
    return true; // Always.
#endif
#if __JHEXEN__
    return (slot != AUTO_SLOT && slot != BASE_SLOT);
#else
    return (slot != AUTO_SLOT);
#endif
}

void SV_ClearSlot(int slot)
{
    DENG_ASSERT(inited);

    if(!SV_IsValidSlot(slot)) return;

    if(announceOnClearingSlot(slot))
    {
        AutoStr *ident = SV_ComposeSlotIdentifier(slot);
        App_Log(DE2_RES_MSG, "Clearing save slot %s", Str_Text(ident));
    }

    for(int i = 0; i < MAX_HUB_MAPS; ++i)
    {
        AutoStr *path = composeGameSavePathForSlot2(slot, i);
        SV_RemoveFile(path);
    }

    AutoStr *path = composeGameSavePathForSlot(slot);
    SV_RemoveFile(path);

    // Update save info for this slot.
    updateSaveInfo(path, findSaveInfoForSlot(slot));
}

dd_bool SV_IsValidSlot(int slot)
{
    if(slot == AUTO_SLOT) return true;
#if __JHEXEN__
    if(slot == BASE_SLOT) return true;
#endif
    return (slot >= 0  && slot < NUMSAVESLOTS);
}

dd_bool SV_IsUserWritableSlot(int slot)
{
    if(slot == AUTO_SLOT) return false;
#if __JHEXEN__
    if(slot == BASE_SLOT) return false;
#endif
    return SV_IsValidSlot(slot);
}

static void SV_SaveInfo_Read(SaveInfo *info, Reader *reader)
{
#if __JHEXEN__
    // Read the magic byte to determine the high-level format.
    int magic = Reader_ReadInt32(reader);
    SV_HxSavePtr()->b -= 4; // Rewind the stream.

    if((!IS_NETWORK_CLIENT && magic != MY_SAVE_MAGIC) ||
       ( IS_NETWORK_CLIENT && magic != MY_CLIENT_SAVE_MAGIC))
    {
        // Perhaps the old v9 format?
        SaveInfo_Read_Hx_v9(info, reader);
    }
    else
#endif
    {
        SaveInfo_Read(info, reader);
    }
}

static bool recogniseNativeState(Str const *path, SaveInfo *info)
{
    DENG_ASSERT(path != 0 && info != 0);

    if(!SV_ExistingFile(path)) return false;

#if __JHEXEN__
    /// @todo Do not buffer the whole file.
    byte *saveBuffer;
    size_t fileSize = M_ReadFile(Str_Text(path), (char **) &saveBuffer);
    if(!fileSize) return false;
    // Set the save pointer.
    SV_HxSavePtr()->b = saveBuffer;
    SV_HxSetSaveEndPtr(saveBuffer + fileSize);
#else
    if(!SV_OpenFile(path, "rp"))
        return false;
#endif

    Reader *reader = SV_NewReader();
    SV_SaveInfo_Read(info, reader);
    Reader_Delete(reader);

#if __JHEXEN__
    Z_Free(saveBuffer);
#else
    SV_CloseFile();
#endif

    // Magic must match.
    if(info->header.magic != MY_SAVE_MAGIC &&
       info->header.magic != MY_CLIENT_SAVE_MAGIC) return false;

    /*
     * Check for unsupported versions.
     */
    // A future version?
    if(info->header.version > MY_SAVE_VERSION) return false;

#if __JHEXEN__
    // We are incompatible with v3 saves due to an invalid test used to determine
    // present sides (ver3 format's sides contain chunks of junk data).
    if(info->header.version == 3) return false;
#endif

    return true;
}

static bool recogniseGameState(Str const *path, SaveInfo *info)
{
    if(path && info)
    {
        if(recogniseNativeState(path, info))
            return true;

        // Perhaps an original game state?
#if __JDOOM__
        if(SV_RecogniseState_Dm_v19(path, info))
            return true;
#endif
#if __JHERETIC__
        if(SV_RecogniseState_Hr_v13(path, info))
            return true;
#endif
    }
    return false;
}

SaveInfo *SV_SaveInfoForSlot(int slot)
{
    DENG_ASSERT(inited);
    return findSaveInfoForSlot(slot);
}

void SV_UpdateAllSaveInfo()
{
    DENG_ASSERT(inited);
    buildSaveInfo();
}

int SV_ParseSlotIdentifier(char const *str)
{
    // Try game-save name match.
    int slot = SV_SlotForSaveName(str);
    if(slot >= 0) return slot;

    // Try keyword identifiers.
    if(!stricmp(str, "last") || !stricmp(str, "<last>"))
    {
        return Con_GetInteger("game-save-last-slot");
    }
    if(!stricmp(str, "quick") || !stricmp(str, "<quick>"))
    {
        return Con_GetInteger("game-save-quick-slot");
    }
    if(!stricmp(str, "auto") || !stricmp(str, "<auto>"))
    {
        return AUTO_SLOT;
    }

    // Try logical slot identifier.
    if(M_IsStringValidInt(str))
    {
        return atoi(str);
    }

    // Unknown/not found.
    return -1;
}

int SV_SlotForSaveName(char const *name)
{
    DENG_ASSERT(inited);

    int saveSlot = -1;
    if(name && name[0])
    {
        // On first call - automatically build and populate game-save info.
        if(!saveInfo)
        {
            buildSaveInfo();
        }

        int i = 0;
        do
        {
            SaveInfo *info = saveInfo[i];
            if(!Str_CompareIgnoreCase(SaveInfo_Name(info), name))
            {
                // This is the one!
                saveSlot = i;
            }
        } while(-1 == saveSlot && ++i < NUMSAVESLOTS);
    }
    return saveSlot;
}

dd_bool SV_IsSlotUsed(int slot)
{
    DENG_ASSERT(inited);
    if(SV_ExistingFile(composeGameSavePathForSlot(slot)))
    {
        SaveInfo *info = SV_SaveInfoForSlot(slot);
        return SaveInfo_IsLoadable(info);
    }
    return false;
}

#if __JHEXEN__
dd_bool SV_HxHaveMapStateForSlot(int slot, uint map)
{
    AutoStr *path = composeGameSavePathForSlot2(slot, (int)map+1);
    if(!path || Str_IsEmpty(path)) return false;
    return SV_ExistingFile(path);
}
#endif

void SV_CopySlot(int sourceSlot, int destSlot)
{
    DENG_ASSERT(inited);

    if(!SV_IsValidSlot(sourceSlot))
    {
        DENG_ASSERT(!"SV_CopySlot: Source slot invalid");
        return;
    }

    if(!SV_IsValidSlot(destSlot))
    {
        DENG_ASSERT(!"SV_CopySlot: Dest slot invalid");
        return;
    }

    // Clear all save files at destination slot.
    SV_ClearSlot(destSlot);

    AutoStr *src, *dst;
    for(int i = 0; i < MAX_HUB_MAPS; ++i)
    {
        src = composeGameSavePathForSlot2(sourceSlot, i);
        dst = composeGameSavePathForSlot2(destSlot, i);
        SV_CopyFile(src, dst);
    }

    src = composeGameSavePathForSlot(sourceSlot);
    dst = composeGameSavePathForSlot(destSlot);
    SV_CopyFile(src, dst);

    // Copy saveinfo too.
    replaceSaveInfo(destSlot, SaveInfo_NewCopy(findSaveInfoForSlot(sourceSlot)));
}

#if __JHEXEN__
void SV_HxInitBaseSlot()
{
    SV_ClearSlot(BASE_SLOT);
}
#endif

uint SV_GenerateGameId()
{
    return Timer_RealMilliseconds() + (mapTime << 24);
}

/**
 * Returns the info for the specified thinker @a tClass; otherwise @c 0 if not found.
 */
static ThinkerClassInfo *infoForThinkerClass(thinkerclass_t tClass)
{
    for(ThinkerClassInfo *info = thinkerInfo; info->thinkclass != TC_NULL; info++)
    {
        if(info->thinkclass == tClass)
            return info;
    }
    return 0; // Not found.
}

/**
 * Returns the info for the specified thinker; otherwise @c 0 if not found.
 */
static ThinkerClassInfo *infoForThinker(thinker_t const &thinker)
{
    for(ThinkerClassInfo *info = thinkerInfo; info->thinkclass != TC_NULL; info++)
    {
        if(info->function == thinker.function)
            return info;
    }
    return 0; // Not found.
}

static void initThingArchiveForLoad(uint size)
{
    thingArchiveSize = size;
    thingArchive = reinterpret_cast<mobj_t **>(M_Calloc(thingArchiveSize * sizeof(*thingArchive)));
}

struct countMobjThinkersToArchiveParms
{
    uint count;
    bool excludePlayers;
};

static int countMobjThinkersToArchive(thinker_t *th, void *context)
{
    countMobjThinkersToArchiveParms *parms = (countMobjThinkersToArchiveParms *) context;
    if(!(Mobj_IsPlayer((mobj_t *) th) && parms->excludePlayers))
    {
        parms->count++;
    }
    return false; // Continue iteration.
}

static void initThingArchiveForSave(bool excludePlayers = false)
{
    // Count the number of things we'll be writing.
    countMobjThinkersToArchiveParms parms;
    parms.count = 0;
    parms.excludePlayers = excludePlayers;
    Thinker_Iterate((thinkfunc_t) P_MobjThinker, countMobjThinkersToArchive, &parms);

    thingArchiveSize = parms.count;
    thingArchive = reinterpret_cast<mobj_t **>(M_Calloc(thingArchiveSize * sizeof(*thingArchive)));
    thingArchiveExcludePlayers = excludePlayers;
}

static void insertThingInArchive(mobj_t const *mo, ThingSerialId thingId)
{
    DENG_ASSERT(mo != 0);

#if __JHEXEN__
    if(mapVersion >= 4)
#endif
    {
        thingId -= 1;
    }

#if __JHEXEN__
    // Only signed in Hexen.
    DENG2_ASSERT(thingId >= 0);
    if(thingId < 0) return; // Does this ever occur?
#endif

    DENG_ASSERT(thingArchive != 0);    
    DENG_ASSERT((unsigned)thingId < thingArchiveSize);
    thingArchive[thingId] = const_cast<mobj_t *>(mo);
}

static void clearThingArchive()
{
    if(thingArchive)
    {
        M_Free(thingArchive); thingArchive = 0;
        thingArchiveSize = 0;
    }
}

ThingSerialId SV_ThingArchiveId(mobj_t const *mo)
{
    DENG_ASSERT(inited);
    DENG_ASSERT(thingArchive != 0);

    if(!mo) return 0;

    // We only archive mobj thinkers.
    if(((thinker_t *) mo)->function != (thinkfunc_t) P_MobjThinker)
        return 0;

#if __JHEXEN__
    if(mo->player && thingArchiveExcludePlayers)
        return TargetPlayerId;
#endif

    uint firstUnused = 0;
    bool found = false;
    for(uint i = 0; i < thingArchiveSize; ++i)
    {
        if(!thingArchive[i] && !found)
        {
            firstUnused = i;
            found = true;
            continue;
        }
        if(thingArchive[i] == mo)
            return i + 1;
    }

    if(!found)
    {
        Con_Error("SV_ThingArchiveId: Thing archive exhausted!");
        return 0; // No number available!
    }

    // Insert it in the archive.
    thingArchive[firstUnused] = const_cast<mobj_t *>(mo);
    return firstUnused + 1;
}

static void clearMaterialArchive()
{
    MaterialArchive_Delete(materialArchive); materialArchive = 0;
}

Material *SV_GetArchiveMaterial(materialarchive_serialid_t serialId, int group)
{
    DENG_ASSERT(inited);
    DENG_ASSERT(materialArchive != 0);
    return MaterialArchive_Find(materialArchive, serialId, group);
}

#if __JHEXEN__
static void initTargetPlayers()
{
    targetPlayerAddrs = 0;
}

static void clearTargetPlayers()
{
    while(targetPlayerAddrs)
    {
        targetplraddress_t *next = targetPlayerAddrs->next;
        M_Free(targetPlayerAddrs);
        targetPlayerAddrs = next;
    }
}
#endif // __JHEXEN__

mobj_t *SV_GetArchiveThing(ThingSerialId thingId, void *address)
{
    DENG_ASSERT(inited);
#if !__JHEXEN__
    DENG_UNUSED(address);
#endif

#if __JHEXEN__
    if(thingId == TargetPlayerId)
    {
        targetplraddress_t *tpa = reinterpret_cast<targetplraddress_t *>(M_Malloc(sizeof(targetplraddress_t)));

        tpa->address = (void **)address;

        tpa->next = targetPlayerAddrs;
        targetPlayerAddrs = tpa;

        return 0;
    }
#endif

    DENG_ASSERT(thingArchive != 0);

#if __JHEXEN__
    if(mapVersion < 4)
    {
        // Old format (base 0).

        // A NULL reference?
        if(thingId == -1) return 0;

        if(thingId < 0 || (unsigned) thingId > thingArchiveSize - 1)
            return 0;
    }
    else
#endif
    {
        // New format (base 1).

        // A NULL reference?
        if(thingId == 0) return 0;

        if(thingId < 1 || (unsigned) thingId > thingArchiveSize)
        {
            App_Log(DE2_RES_WARNING, "SV_GetArchiveThing: Invalid thing Id %i", thingId);
            return 0;
        }

        thingId -= 1;
    }

    return thingArchive[thingId];
}

static playerheader_t *getPlayerHeader()
{
    DENG_ASSERT(playerHeaderOK);
    return &playerHeader;
}

/**
 * Returns the material archive version for the save state which is
 * @em presently being read.
 */
static inline int materialArchiveVersion()
{
#if __JHEXEN__
    if(mapVersion < 6)
#else
    if(hdr->version < 6)
#endif
    {
        return 0;
    }
    return -1;
}

/**
 * Writes the given player's data (not including the ID number).
 */
static void SV_WritePlayer(int playernum)
{
    int i, numPSprites = getPlayerHeader()->numPSprites;
    player_t temp, *p = &temp;
    ddplayer_t ddtemp, *dp = &ddtemp;

    // Make a copy of the player.
    std::memcpy(p, &players[playernum], sizeof(temp));
    std::memcpy(dp, players[playernum].plr, sizeof(ddtemp));
    temp.plr = &ddtemp;

    // Convert the psprite states.
    for(i = 0; i < numPSprites; ++i)
    {
        pspdef_t *pspDef = &temp.pSprites[i];

        if(pspDef->state)
        {
            pspDef->state = (state_t *) (pspDef->state - STATES);
        }
    }

    // Version number. Increase when you make changes to the player data
    // segment format.
    SV_WriteByte(6);

#if __JHEXEN__
    // Class.
    SV_WriteByte(cfg.playerClass[playernum]);
#endif

    SV_WriteLong(p->playerState);
#if __JHEXEN__
    SV_WriteLong(p->class_);    // 2nd class...?
#endif
    SV_WriteLong(FLT2FIX(p->viewZ));
    SV_WriteLong(FLT2FIX(p->viewHeight));
    SV_WriteLong(FLT2FIX(p->viewHeightDelta));
#if !__JHEXEN__
    SV_WriteFloat(dp->lookDir);
#endif
    SV_WriteLong(FLT2FIX(p->bob));
#if __JHEXEN__
    SV_WriteLong(p->flyHeight);
    SV_WriteFloat(dp->lookDir);
    SV_WriteLong(p->centering);
#endif
    SV_WriteLong(p->health);

#if __JHEXEN__
    for(i = 0; i < getPlayerHeader()->numArmorTypes; ++i)
    {
        SV_WriteLong(p->armorPoints[i]);
    }
#else
    SV_WriteLong(p->armorPoints);
    SV_WriteLong(p->armorType);
#endif

#if __JDOOM64__ || __JHEXEN__
    for(i = 0; i < getPlayerHeader()->numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRST + i);

        SV_WriteLong(type);
        SV_WriteLong(P_InventoryCount(playernum, type));
    }
    SV_WriteLong(P_InventoryReadyItem(playernum));
#endif

    for(i = 0; i < getPlayerHeader()->numPowers; ++i)
    {
        SV_WriteLong(p->powers[i]);
    }

#if __JHEXEN__
    SV_WriteLong(p->keys);
#else
    for(i = 0; i < getPlayerHeader()->numKeys; ++i)
    {
        SV_WriteLong(p->keys[i]);
    }
#endif

#if __JHEXEN__
    SV_WriteLong(p->pieces);
#else
    SV_WriteLong(p->backpack);
#endif

    for(i = 0; i < getPlayerHeader()->numFrags; ++i)
    {
        SV_WriteLong(p->frags[i]);
    }

    SV_WriteLong(p->readyWeapon);
    SV_WriteLong(p->pendingWeapon);

    for(i = 0; i < getPlayerHeader()->numWeapons; ++i)
    {
        SV_WriteLong(p->weapons[i].owned);
    }

    for(i = 0; i < getPlayerHeader()->numAmmoTypes; ++i)
    {
        SV_WriteLong(p->ammo[i].owned);
#if !__JHEXEN__
        SV_WriteLong(p->ammo[i].max);
#endif
    }

    SV_WriteLong(p->attackDown);
    SV_WriteLong(p->useDown);

    SV_WriteLong(p->cheats);

    SV_WriteLong(p->refire);

    SV_WriteLong(p->killCount);
    SV_WriteLong(p->itemCount);
    SV_WriteLong(p->secretCount);

    SV_WriteLong(p->damageCount);
    SV_WriteLong(p->bonusCount);
#if __JHEXEN__
    SV_WriteLong(p->poisonCount);
#endif

    SV_WriteLong(dp->extraLight);
    SV_WriteLong(dp->fixedColorMap);
    SV_WriteLong(p->colorMap);

    for(i = 0; i < numPSprites; ++i)
    {
        pspdef_t       *psp = &p->pSprites[i];

        SV_WriteLong(PTR2INT(psp->state));
        SV_WriteLong(psp->tics);
        SV_WriteLong(FLT2FIX(psp->pos[VX]));
        SV_WriteLong(FLT2FIX(psp->pos[VY]));
    }

#if !__JHEXEN__
    SV_WriteLong(p->didSecret);

    // Added in ver 2 with __JDOOM__
    SV_WriteLong(p->flyHeight);
#endif

#if __JHERETIC__
    for(i = 0; i < getPlayerHeader()->numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRST + i);

        SV_WriteLong(type);
        SV_WriteLong(P_InventoryCount(playernum, type));
    }
    SV_WriteLong(P_InventoryReadyItem(playernum));
    SV_WriteLong(p->chickenPeck);
#endif

#if __JHERETIC__ || __JHEXEN__
    SV_WriteLong(p->morphTics);
#endif

    SV_WriteLong(p->airCounter);

#if __JHEXEN__
    SV_WriteLong(p->jumpTics);
    SV_WriteLong(p->worldTimer);
#elif __JHERETIC__
    SV_WriteLong(p->flameCount);

    // Added in ver 2
    SV_WriteByte(p->class_);
#endif
}

/**
 * Reads a player's data (not including the ID number).
 */
static void SV_ReadPlayer(player_t *p)
{
    int plrnum = p - players;
    int numPSprites = getPlayerHeader()->numPSprites;
    ddplayer_t *dp = p->plr;

    byte ver = SV_ReadByte();

#if __JHEXEN__
    cfg.playerClass[plrnum] = playerclass_t(SV_ReadByte());

    memset(p, 0, sizeof(*p)); // Force everything NULL,
    p->plr = dp;              // but restore the ddplayer pointer.
#endif

    p->playerState     = playerstate_t(SV_ReadLong());
#if __JHEXEN__
    p->class_          = playerclass_t(SV_ReadLong()); // 2nd class?? (ask Raven...)
#endif

    p->viewZ           = FIX2FLT(SV_ReadLong());
    p->viewHeight      = FIX2FLT(SV_ReadLong());
    p->viewHeightDelta = FIX2FLT(SV_ReadLong());
#if !__JHEXEN__
    dp->lookDir        = SV_ReadFloat();
#endif
    p->bob             = FIX2FLT(SV_ReadLong());
#if __JHEXEN__
    p->flyHeight       = SV_ReadLong();
    dp->lookDir        = SV_ReadFloat();
    p->centering       = SV_ReadLong();
#endif

    p->health          = SV_ReadLong();

#if __JHEXEN__
    for(int i = 0; i < getPlayerHeader()->numArmorTypes; ++i)
    {
        p->armorPoints[i] = SV_ReadLong();
    }
#else
    p->armorPoints = SV_ReadLong();
    p->armorType = SV_ReadLong();
#endif

#if __JDOOM64__ || __JHEXEN__
    P_InventoryEmpty(plrnum);
    for(int i = 0; i < getPlayerHeader()->numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(SV_ReadLong());
        int count = SV_ReadLong();

        for(int j = 0; j < count; ++j)
            P_InventoryGive(plrnum, type, true);
    }

    P_InventorySetReadyItem(plrnum, inventoryitemtype_t(SV_ReadLong()));
# if __JHEXEN__
    Hu_InventorySelect(plrnum, P_InventoryReadyItem(plrnum));
    if(ver < 5)
    {
        SV_ReadLong(); // Current inventory item count?
    }
    if(ver < 6)
    /*p->inventorySlotNum =*/ SV_ReadLong();
# endif
#endif

    for(int i = 0; i < getPlayerHeader()->numPowers; ++i)
    {
        p->powers[i] = SV_ReadLong();
    }
    if(p->powers[PT_ALLMAP])
        ST_RevealAutomap(plrnum, true);

#if __JHEXEN__
    p->keys = SV_ReadLong();
#else
    for(int i = 0; i < getPlayerHeader()->numKeys; ++i)
    {
        p->keys[i] = SV_ReadLong();
    }
#endif

#if __JHEXEN__
    p->pieces = SV_ReadLong();
#else
    p->backpack = SV_ReadLong();
#endif

    for(int i = 0; i < getPlayerHeader()->numFrags; ++i)
    {
        p->frags[i] = SV_ReadLong();
    }

    p->readyWeapon = weapontype_t(SV_ReadLong());
#if __JHEXEN__
    if(ver < 5)
        p->pendingWeapon = WT_NOCHANGE;
    else
#endif
        p->pendingWeapon = weapontype_t(SV_ReadLong());

    for(int i = 0; i < getPlayerHeader()->numWeapons; ++i)
    {
        p->weapons[i].owned = (SV_ReadLong()? true : false);
    }

    for(int i = 0; i < getPlayerHeader()->numAmmoTypes; ++i)
    {
        p->ammo[i].owned = SV_ReadLong();

#if !__JHEXEN__
        p->ammo[i].max = SV_ReadLong();
#endif
    }

    p->attackDown  = SV_ReadLong();
    p->useDown     = SV_ReadLong();
    p->cheats      = SV_ReadLong();
    p->refire      = SV_ReadLong();
    p->killCount   = SV_ReadLong();
    p->itemCount   = SV_ReadLong();
    p->secretCount = SV_ReadLong();

#if __JHEXEN__
    if(ver <= 1)
    {
        /*p->messageTics     =*/ SV_ReadLong();
        /*p->ultimateMessage =*/ SV_ReadLong();
        /*p->yellowMessage   =*/ SV_ReadLong();
    }
#endif

    p->damageCount = SV_ReadLong();
    p->bonusCount  = SV_ReadLong();
#if __JHEXEN__
    p->poisonCount = SV_ReadLong();
#endif

    dp->extraLight = SV_ReadLong();
    dp->fixedColorMap = SV_ReadLong();
    p->colorMap    = SV_ReadLong();

    for(int i = 0; i < numPSprites; ++i)
    {
        pspdef_t *psp = &p->pSprites[i];

        psp->state = (state_t *) SV_ReadLong();
        psp->tics = SV_ReadLong();
        psp->pos[VX] = FIX2FLT(SV_ReadLong());
        psp->pos[VY] = FIX2FLT(SV_ReadLong());
    }

#if !__JHEXEN__
    p->didSecret = SV_ReadLong();

# if __JDOOM__ || __JDOOM64__
    if(ver == 2) // nolonger used in >= ver 3
        /*p->messageTics =*/ SV_ReadLong();

    if(ver >= 2)
        p->flyHeight = SV_ReadLong();

# elif __JHERETIC__
    if(ver < 3) // nolonger used in >= ver 3
        /*p->messageTics =*/ SV_ReadLong();

    p->flyHeight = SV_ReadLong();

    P_InventoryEmpty(plrnum);
    for(int i = 0; i < getPlayerHeader()->numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(SV_ReadLong());
        int count = SV_ReadLong();

        for(int j = 0; j < count; ++j)
            P_InventoryGive(plrnum, type, true);
    }

    P_InventorySetReadyItem(plrnum, (inventoryitemtype_t) SV_ReadLong());
    Hu_InventorySelect(plrnum, P_InventoryReadyItem(plrnum));
    if(ver < 5)
    {
        SV_ReadLong(); // Current inventory item count?
    }
    if(ver < 6)
    /*p->inventorySlotNum =*/ SV_ReadLong();

    p->chickenPeck = SV_ReadLong();
# endif
#endif

#if __JHERETIC__ || __JHEXEN__
    p->morphTics = SV_ReadLong();
#endif

    if(ver >= 2)
        p->airCounter = SV_ReadLong();

#if __JHEXEN__
    p->jumpTics = SV_ReadLong();
    p->worldTimer = SV_ReadLong();
#elif __JHERETIC__
    p->flameCount = SV_ReadLong();

    if(ver >= 2)
        p->class_ = playerclass_t(SV_ReadByte());
#endif

#if !__JHEXEN__
    // Will be set when unarc thinker.
    p->plr->mo = NULL;
    p->attacker = NULL;
#endif

    // Demangle it.
    for(int i = 0; i < numPSprites; ++i)
        if(p->pSprites[i].state)
        {
            p->pSprites[i].state = &STATES[PTR2INT(p->pSprites[i].state)];
        }

    // Mark the player for fixpos and fixangles.
    dp->flags |= DDPF_FIXORIGIN | DDPF_FIXANGLES | DDPF_FIXMOM;
    p->update |= PSF_REBORN;
}

#if __JHEXEN__
# define MOBJ_SAVEVERSION 8
#elif __JHERETIC__
# define MOBJ_SAVEVERSION 10
#else
# define MOBJ_SAVEVERSION 10
#endif

static void SV_WriteMobj(mobj_t const *original)
{
    mobj_t temp, *mo = &temp;

    std::memcpy(mo, original, sizeof(*mo));
    // Mangle it!
    mo->state = (state_t *) (mo->state - STATES);
    if(mo->player)
        mo->player = (player_t *) ((mo->player - players) + 1);

    // Version.
    // JHEXEN
    // 2: Added the 'translucency' byte.
    // 3: Added byte 'vistarget'
    // 4: Added long 'tracer'
    // 4: Added long 'lastenemy'
    // 5: Added flags3
    // 6: Floor material removed.
    //
    // JDOOM || JHERETIC || JDOOM64
    // 4: Added byte 'translucency'
    // 5: Added byte 'vistarget'
    // 5: Added tracer in jDoom
    // 5: Added dropoff fix in jHeretic
    // 5: Added long 'floorclip'
    // 6: Added proper respawn data
    // 6: Added flags 2 in jDoom
    // 6: Added damage
    // 7: Added generator in jHeretic
    // 7: Added flags3
    //
    // JDOOM
    // 9: Revised mapspot flag interpretation
    //
    // JHERETIC
    // 8: Added special3
    // 9: Revised mapspot flag interpretation
    //
    // JHEXEN
    // 7: Removed superfluous info ptr
    // 8: Added 'onMobj'
    SV_WriteByte(MOBJ_SAVEVERSION);

#if !__JHEXEN__
    // A version 2 features: archive number and target.
    SV_WriteShort(SV_ThingArchiveId((mobj_t*) original));
    SV_WriteShort(SV_ThingArchiveId(mo->target));

# if __JDOOM__ || __JDOOM64__
    // Ver 5 features: Save tracer (fixes Archvile, Revenant bug)
    SV_WriteShort(SV_ThingArchiveId(mo->tracer));
# endif
#endif

    SV_WriteShort(SV_ThingArchiveId(mo->onMobj));

    // Info for drawing: position.
    SV_WriteLong(FLT2FIX(mo->origin[VX]));
    SV_WriteLong(FLT2FIX(mo->origin[VY]));
    SV_WriteLong(FLT2FIX(mo->origin[VZ]));

    //More drawing info: to determine current sprite.
    SV_WriteLong(mo->angle); // Orientation.
    SV_WriteLong(mo->sprite); // Used to find patch_t and flip value.
    SV_WriteLong(mo->frame);

#if !__JHEXEN__
    // The closest interval over all contacted Sectors.
    SV_WriteLong(FLT2FIX(mo->floorZ));
    SV_WriteLong(FLT2FIX(mo->ceilingZ));
#endif

    // For movement checking.
    SV_WriteLong(FLT2FIX(mo->radius));
    SV_WriteLong(FLT2FIX(mo->height));

    // Momentums, used to update position.
    SV_WriteLong(FLT2FIX(mo->mom[MX]));
    SV_WriteLong(FLT2FIX(mo->mom[MY]));
    SV_WriteLong(FLT2FIX(mo->mom[MZ]));

    // If == VALIDCOUNT, already checked.
    SV_WriteLong(mo->valid);

    SV_WriteLong(mo->type);
    SV_WriteLong(mo->tics); // State tic counter.
    SV_WriteLong(PTR2INT(mo->state));

#if __JHEXEN__
    SV_WriteLong(mo->damage);
#endif

    SV_WriteLong(mo->flags);
#if __JHEXEN__
    SV_WriteLong(mo->flags2);
    SV_WriteLong(mo->flags3);

    if(mo->type == MT_KORAX)
        SV_WriteLong(0); // Searching index.
    else
        SV_WriteLong(mo->special1);

    switch(mo->type)
    {
    case MT_LIGHTNING_FLOOR:
    case MT_LIGHTNING_ZAP:
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(mo->flags & MF_CORPSE)
            SV_WriteLong(0);
        else
            SV_WriteLong(SV_ThingArchiveId(INT2PTR(mobj_t, mo->special2)));
        break;

    default:
        SV_WriteLong(mo->special2);
        break;
    }
#endif
    SV_WriteLong(mo->health);

    // Movement direction, movement generation (zig-zagging).
    SV_WriteLong(mo->moveDir); // 0-7
    SV_WriteLong(mo->moveCount); // When 0, select a new dir.

#if __JHEXEN__
    if(mo->flags & MF_CORPSE)
        SV_WriteLong(0);
    else
        SV_WriteLong((int) SV_ThingArchiveId(mo->target));
#endif

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    SV_WriteLong(mo->reactionTime);

    // If >0, the target will be chased no matter what (even if shot).
    SV_WriteLong(mo->threshold);

    // Additional info record for player avatars only (only valid if type
    // == MT_PLAYER).
    SV_WriteLong(PTR2INT(mo->player));

    // Player number last looked for.
    SV_WriteLong(mo->lastLook);

#if !__JHEXEN__
    // For nightmare/multiplayer respawn.
    SV_WriteLong(FLT2FIX(mo->spawnSpot.origin[VX]));
    SV_WriteLong(FLT2FIX(mo->spawnSpot.origin[VY]));
    SV_WriteLong(FLT2FIX(mo->spawnSpot.origin[VZ]));
    SV_WriteLong(mo->spawnSpot.angle);
    SV_WriteLong(mo->spawnSpot.flags);

    SV_WriteLong(mo->intFlags); // $dropoff_fix: internal flags.
    SV_WriteLong(FLT2FIX(mo->dropOffZ)); // $dropoff_fix
    SV_WriteLong(mo->gear); // Used in torque simulation.

    SV_WriteLong(mo->damage);
    SV_WriteLong(mo->flags2);
    SV_WriteLong(mo->flags3);
# ifdef __JHERETIC__
    SV_WriteLong(mo->special1);
    SV_WriteLong(mo->special2);
    SV_WriteLong(mo->special3);
# endif

    SV_WriteByte(mo->translucency);
    SV_WriteByte((byte)(mo->visTarget +1));
#endif

    SV_WriteLong(FLT2FIX(mo->floorClip));
#if __JHEXEN__
    SV_WriteLong(SV_ThingArchiveId((mobj_t*) original));
    SV_WriteLong(mo->tid);
    SV_WriteLong(mo->special);
    SV_Write(mo->args, sizeof(mo->args));
    SV_WriteByte(mo->translucency);
    SV_WriteByte((byte)(mo->visTarget +1));

    switch(mo->type)
    {
    case MT_BISH_FX:
    case MT_HOLY_FX:
    case MT_DRAGON:
    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
    case MT_MINOTAUR:
    case MT_SORCFX1:
    case MT_MSTAFF_FX2:
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(mo->flags & MF_CORPSE)
            SV_WriteLong(0);
        else
            SV_WriteLong(SV_ThingArchiveId(mo->tracer));
        break;

    default:
        DENG_ASSERT(mo->tracer == NULL); /// @todo Tracer won't be saved correctly?
        SV_WriteLong(PTR2INT(mo->tracer));
        break;
    }

    SV_WriteLong(PTR2INT(mo->lastEnemy));
#elif __JHERETIC__
    // Ver 7 features: generator
    SV_WriteShort(SV_ThingArchiveId(mo->generator));
#endif
}

#if !__JDOOM64__
void SV_TranslateLegacyMobjFlags(mobj_t* mo, int ver)
{
#if __JDOOM__ || __JHERETIC__
    if(ver < 6)
    {
        // mobj.flags
# if __JDOOM__
        // switched values for MF_BRIGHTSHADOW <> MF_BRIGHTEXPLODE
        if((mo->flags & MF_BRIGHTEXPLODE) != (mo->flags & MF_BRIGHTSHADOW))
        {
            if(mo->flags & MF_BRIGHTEXPLODE) // previously MF_BRIGHTSHADOW
            {
                mo->flags |= MF_BRIGHTSHADOW;
                mo->flags &= ~MF_BRIGHTEXPLODE;
            }
            else // previously MF_BRIGHTEXPLODE
            {
                mo->flags |= MF_BRIGHTEXPLODE;
                mo->flags &= ~MF_BRIGHTSHADOW;
            }
        } // else they were both on or off so it doesn't matter.
# endif
        // Remove obsoleted flags in earlier save versions.
        mo->flags &= ~MF_V6OBSOLETE;

        // mobj.flags2
# if __JDOOM__
        // jDoom only gained flags2 in ver 6 so all we can do is to
        // apply the values as set in the mobjinfo.
        // Non-persistent flags might screw things up a lot worse otherwise.
        mo->flags2 = mo->info->flags2;
# endif
    }
#endif

#if __JDOOM__ || __JHERETIC__
    if(ver < 9)
    {
        mo->spawnSpot.flags &= ~MASK_UNKNOWN_MSF_FLAGS;
        // Spawn on the floor by default unless the mobjtype flags override.
        mo->spawnSpot.flags |= MSF_Z_FLOOR;
    }
#endif

#if __JHEXEN__
    if(ver < 5)
#else
    if(ver < 7)
#endif
    {
        // flags3 was introduced in a latter version so all we can do is to
        // apply the values as set in the mobjinfo.
        // Non-persistent flags might screw things up a lot worse otherwise.
        mo->flags3 = mo->info->flags3;
    }
}
#endif

static void RestoreMobj(mobj_t *mo, int ver)
{
#if __JDOOM64__
    DENG_UNUSED(ver);
#endif

    mo->info = &MOBJINFO[mo->type];

    Mobj_SetState(mo, PTR2INT(mo->state));
#if __JHEXEN__
    if(mo->flags2 & MF2_DORMANT)
        mo->tics = -1;
#endif

    if(mo->player)
    {
        // The player number translation table is used to find out the
        // *current* (actual) player number of the referenced player.
        int pNum = saveToRealPlayerNum[PTR2INT(mo->player) - 1];

#if __JHEXEN__
        if(pNum < 0)
        {
            // This saved player does not exist in the current game!
            // Destroy this mobj.
            Z_Free(mo);

            return;  // Don't add this thinker.
        }
#endif

        mo->player = &players[pNum];
        mo->dPlayer = mo->player->plr;
        mo->dPlayer->mo = mo;
        //mo->dPlayer->clAngle = mo->angle; /* $unifiedangles */
        mo->dPlayer->lookDir = 0; /* $unifiedangles */
    }

    mo->visAngle = mo->angle >> 16;

#if !__JHEXEN__
    if(mo->dPlayer && !mo->dPlayer->inGame)
    {
        if(mo->dPlayer)
            mo->dPlayer->mo = NULL;
        Mobj_Destroy(mo);

        return;
    }
#endif

#if !__JDOOM64__
    // Do we need to update this mobj's flag values?
    if(ver < MOBJ_SAVEVERSION)
        SV_TranslateLegacyMobjFlags(mo, ver);
#endif

    P_MobjLink(mo);
    mo->floorZ   = P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT);
    mo->ceilingZ = P_GetDoublep(Mobj_Sector(mo), DMU_CEILING_HEIGHT);

    return;
}

/**
 * Always returns @c false as a thinker will have already been allocated in
 * the mobj creation process.
 */
static int SV_ReadMobj(thinker_t* th, int /*mapVersion*/)
{
    int ver;
    mobj_t* mo = (mobj_t*) th;

    ver = SV_ReadByte();

#if !__JHEXEN__
    if(ver >= 2) // Version 2 has mobj archive numbers.
        insertThingInArchive(mo, SV_ReadShort());
#endif

#if !__JHEXEN__
    mo->target = NULL;
    if(ver >= 2)
    {
        mo->target = INT2PTR(mobj_t, SV_ReadShort());
    }
#endif

#if __JDOOM__ || __JDOOM64__
    // Tracer for enemy attacks (updated after all mobjs are loaded).
    mo->tracer = NULL;
    if(ver >= 5)
    {
        mo->tracer = INT2PTR(mobj_t, SV_ReadShort());
    }
#endif

    // mobj this one is on top of (updated after all mobjs are loaded).
    mo->onMobj = NULL;
#if __JHEXEN__
    if(ver >= 8)
#else
    if(ver >= 5)
#endif
    {
        mo->onMobj = INT2PTR(mobj_t, SV_ReadShort());
    }

    // Info for drawing: position.
    mo->origin[VX] = FIX2FLT(SV_ReadLong());
    mo->origin[VY] = FIX2FLT(SV_ReadLong());
    mo->origin[VZ] = FIX2FLT(SV_ReadLong());

    //More drawing info: to determine current sprite.
    mo->angle = SV_ReadLong();  // orientation
    mo->sprite = SV_ReadLong(); // used to find patch_t and flip value
    mo->frame = SV_ReadLong();  // might be ORed with FF_FULLBRIGHT
    if(mo->frame & FF_FULLBRIGHT)
        mo->frame &= FF_FRAMEMASK; // not used anymore.

#if __JHEXEN__
    if(ver < 6)
        SV_ReadLong(); // Used to be floorflat.
#else
    // The closest interval over all contacted Sectors.
    mo->floorZ = FIX2FLT(SV_ReadLong());
    mo->ceilingZ = FIX2FLT(SV_ReadLong());
#endif

    // For movement checking.
    mo->radius = FIX2FLT(SV_ReadLong());
    mo->height = FIX2FLT(SV_ReadLong());

    // Momentums, used to update position.
    mo->mom[MX] = FIX2FLT(SV_ReadLong());
    mo->mom[MY] = FIX2FLT(SV_ReadLong());
    mo->mom[MZ] = FIX2FLT(SV_ReadLong());

    // If == VALIDCOUNT, already checked.
    mo->valid = SV_ReadLong();
    mo->type = SV_ReadLong();
#if __JHEXEN__
    if(ver < 7)
        /*mo->info = (mobjinfo_t *)*/ SV_ReadLong();
#endif
    mo->info = &MOBJINFO[mo->type];

    if(mo->info->flags2 & MF2_FLOATBOB)
        mo->mom[MZ] = 0;

    if(mo->info->flags & MF_SOLID)
        mo->ddFlags |= DDMF_SOLID;
    if(mo->info->flags2 & MF2_DONTDRAW)
        mo->ddFlags |= DDMF_DONTDRAW;

    mo->tics = SV_ReadLong();   // state tic counter
    mo->state = (state_t *) SV_ReadLong();

#if __JHEXEN__
    mo->damage = SV_ReadLong();
#endif

    mo->flags = SV_ReadLong();

#if __JHEXEN__
    mo->flags2 = SV_ReadLong();
    if(ver >= 5)
        mo->flags3 = SV_ReadLong();
    mo->special1 = SV_ReadLong();
    mo->special2 = SV_ReadLong();
#endif

    mo->health = SV_ReadLong();
#if __JHERETIC__
    if(ver < 8)
    {
        // Fix a bunch of kludges in the original Heretic.
        switch(mo->type)
        {
        case MT_MACEFX1:
        case MT_MACEFX2:
        case MT_MACEFX3:
        case MT_HORNRODFX2:
        case MT_HEADFX3:
        case MT_WHIRLWIND:
        case MT_TELEGLITTER:
        case MT_TELEGLITTER2:
            mo->special3 = mo->health;
            if(mo->type == MT_HORNRODFX2 && mo->special3 > 16)
                mo->special3 = 16;
            mo->health = MOBJINFO[mo->type].spawnHealth;
            break;

        default:
            break;
        }
    }
#endif

    // Movement direction, movement generation (zig-zagging).
    mo->moveDir = SV_ReadLong();    // 0-7
    mo->moveCount = SV_ReadLong();  // when 0, select a new dir

#if __JHEXEN__
    mo->target = (mobj_t *) SV_ReadLong();
#endif

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactionTime = SV_ReadLong();

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold = SV_ReadLong();

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player = (player_t *) SV_ReadLong();

    // Player number last looked for.
    mo->lastLook = SV_ReadLong();

#if __JHEXEN__
    mo->floorClip = FIX2FLT(SV_ReadLong());
    insertThingInArchive(mo, SV_ReadLong());
    mo->tid = SV_ReadLong();
#else
    // For nightmare respawn.
    if(ver >= 6)
    {
        mo->spawnSpot.origin[VX] = FIX2FLT(SV_ReadLong());
        mo->spawnSpot.origin[VY] = FIX2FLT(SV_ReadLong());
        mo->spawnSpot.origin[VZ] = FIX2FLT(SV_ReadLong());
        mo->spawnSpot.angle = SV_ReadLong();
        if(ver < 10)
        /* mo->spawnSpot.type = */ SV_ReadLong();
        mo->spawnSpot.flags = SV_ReadLong();
    }
    else
    {
        mo->spawnSpot.origin[VX] = (float) SV_ReadShort();
        mo->spawnSpot.origin[VY] = (float) SV_ReadShort();
        mo->spawnSpot.origin[VZ] = 0; // Initialize with "something".
        mo->spawnSpot.angle = (angle_t) (ANG45 * (SV_ReadShort() / 45));
        /*mo->spawnSpot.type = (int)*/ SV_ReadShort();
        mo->spawnSpot.flags = (int) SV_ReadShort();
    }

# if __JDOOM__ || __JDOOM64__
    if(ver >= 3)
# elif __JHERETIC__
    if(ver >= 5)
# endif
    {
        mo->intFlags = SV_ReadLong();   // killough $dropoff_fix: internal flags
        mo->dropOffZ = FIX2FLT(SV_ReadLong());   // killough $dropoff_fix
        mo->gear = SV_ReadLong();   // killough used in torque simulation
    }

# if __JDOOM__ || __JDOOM64__
    if(ver >= 6)
    {
        mo->damage = SV_ReadLong();
        mo->flags2 = SV_ReadLong();
    }// Else flags2 will be applied from the defs.
    else
        mo->damage = DDMAXINT; // Use the value set in mo->info->damage

# elif __JHERETIC__
    mo->damage = SV_ReadLong();
    mo->flags2 = SV_ReadLong();
# endif

    if(ver >= 7)
        mo->flags3 = SV_ReadLong();
    // Else flags3 will be applied from the defs.
#endif

#if __JHEXEN__
    mo->special = SV_ReadLong();
    SV_Read(mo->args, 1 * 5);
#elif __JHERETIC__
    mo->special1 = SV_ReadLong();
    mo->special2 = SV_ReadLong();
    if(ver >= 8)
        mo->special3 = SV_ReadLong();
#endif

#if __JHEXEN__
    if(ver >= 2)
#else
    if(ver >= 4)
#endif
        mo->translucency = SV_ReadByte();

#if __JHEXEN__
    if(ver >= 3)
#else
    if(ver >= 5)
#endif
        mo->visTarget = (short) (SV_ReadByte()) -1;

#if __JHEXEN__
    if(ver >= 4)
        mo->tracer = (mobj_t *) SV_ReadLong();

    if(ver >= 4)
        mo->lastEnemy = (mobj_t *) SV_ReadLong();
#else
    if(ver >= 5)
        mo->floorClip = FIX2FLT(SV_ReadLong());
#endif

#if __JHERETIC__
    if(ver >= 7)
        mo->generator = INT2PTR(mobj_t, SV_ReadShort());
    else
        mo->generator = NULL;
#endif

    // Restore! (unmangle)
    RestoreMobj(mo, ver);

    return false;
}

/**
 * Prepare and write the player header info.
 */
static void writePlayerHeader()
{
    playerheader_t *ph = &playerHeader;

    SV_BeginSegment(ASEG_PLAYER_HEADER);
    SV_WriteByte(2); // version byte

    ph->numPowers = NUM_POWER_TYPES;
    ph->numKeys = NUM_KEY_TYPES;
    ph->numFrags = MAXPLAYERS;
    ph->numWeapons = NUM_WEAPON_TYPES;
    ph->numAmmoTypes = NUM_AMMO_TYPES;
    ph->numPSprites = NUMPSPRITES;
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    ph->numInvItemTypes = NUM_INVENTORYITEM_TYPES;
#endif
#if __JHEXEN__
    ph->numArmorTypes = NUMARMOR;
#endif

    SV_WriteLong(ph->numPowers);
    SV_WriteLong(ph->numKeys);
    SV_WriteLong(ph->numFrags);
    SV_WriteLong(ph->numWeapons);
    SV_WriteLong(ph->numAmmoTypes);
    SV_WriteLong(ph->numPSprites);
#if __JDOOM64__ || __JHERETIC__ || __JHEXEN__
    SV_WriteLong(ph->numInvItemTypes);
#endif
#if __JHEXEN__
    SV_WriteLong(ph->numArmorTypes);
#endif

    playerHeaderOK = true;
}

/**
 * Read player header info from the game state.
 */
static void readPlayerHeader()
{
#if __JHEXEN__
    if(hdr->version >= 4)
#else
    if(hdr->version >= 5)
#endif
    {
        SV_AssertSegment(ASEG_PLAYER_HEADER);
        int ver = SV_ReadByte();
#if !__JHERETIC__
        DENG_UNUSED(ver);
#endif

        playerHeader.numPowers      = SV_ReadLong();
        playerHeader.numKeys        = SV_ReadLong();
        playerHeader.numFrags       = SV_ReadLong();
        playerHeader.numWeapons     = SV_ReadLong();
        playerHeader.numAmmoTypes   = SV_ReadLong();
        playerHeader.numPSprites    = SV_ReadLong();
#if __JHERETIC__
        if(ver >= 2)
            playerHeader.numInvItemTypes = SV_ReadLong();
        else
            playerHeader.numInvItemTypes = NUM_INVENTORYITEM_TYPES;
#endif
#if __JHEXEN__ || __JDOOM64__
        playerHeader.numInvItemTypes = SV_ReadLong();
#endif
#if __JHEXEN__
        playerHeader.numArmorTypes  = SV_ReadLong();
#endif
    }
    else // The old format didn't save the counts.
    {
#if __JHEXEN__
        playerHeader.numPowers      = 9;
        playerHeader.numKeys        = 11;
        playerHeader.numFrags       = 8;
        playerHeader.numWeapons     = 4;
        playerHeader.numAmmoTypes   = 2;
        playerHeader.numPSprites    = 2;
        playerHeader.numInvItemTypes = 33;
        playerHeader.numArmorTypes  = 4;
#elif __JDOOM__ || __JDOOM64__
        playerHeader.numPowers      = 6;
        playerHeader.numKeys        = 6;
        playerHeader.numFrags       = 4; // Why was this only 4?
        playerHeader.numWeapons     = 9;
        playerHeader.numAmmoTypes   = 4;
        playerHeader.numPSprites    = 2;
# if __JDOOM64__
        playerHeader.numInvItemTypes = 3;
# endif
#elif __JHERETIC__
        playerHeader.numPowers      = 9;
        playerHeader.numKeys        = 3;
        playerHeader.numFrags       = 4; // ?
        playerHeader.numWeapons     = 8;
        playerHeader.numInvItemTypes = 14;
        playerHeader.numAmmoTypes   = 6;
        playerHeader.numPSprites    = 2;
#endif
    }
    playerHeaderOK = true;
}

static void writePlayers()
{
    SV_BeginSegment(ASEG_PLAYERS);
    {
#if __JHEXEN__
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            SV_WriteByte(players[i].plr->inGame);
        }
#endif

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame)
                continue;

            SV_WriteLong(Net_GetPlayerID(i));
            SV_WritePlayer(i);
        }
    }
    SV_EndSegment();
}

static void readPlayers(dd_bool *infile, dd_bool *loaded)
{
    DENG_ASSERT(infile && loaded);

    // Setup the dummy.
    ddplayer_t dummyDDPlayer;
    player_t dummyPlayer;
    dummyPlayer.plr = &dummyDDPlayer;

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        loaded[i] = 0;
#if !__JHEXEN__
        infile[i] = hdr->players[i];
#endif
    }

    SV_AssertSegment(ASEG_PLAYERS);
    {
#if __JHEXEN__
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            infile[i] = SV_ReadByte();
        }
#endif

        // Load the players.
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            // By default a saved player translates to nothing.
            saveToRealPlayerNum[i] = -1;

            if(!infile[i]) continue;

            // The ID number will determine which player this actually is.
            int pid = SV_ReadLong();
            player_t *player = 0;
            for(int k = 0; k < MAXPLAYERS; ++k)
            {
                if((IS_NETGAME && int(Net_GetPlayerID(k)) == pid) ||
                   (!IS_NETGAME && k == 0))
                {
                    // This is our guy.
                    player = players + k;
                    loaded[k] = true;
                    // Later references to the player number 'i' must be translated!
                    saveToRealPlayerNum[i] = k;
                    App_Log(DE2_DEV_MAP_MSG, "readPlayers: saved %i is now %i\n", i, k);
                    break;
                }
            }

            if(!player)
            {
                // We have a missing player. Use a dummy to load the data.
                player = &dummyPlayer;
            }

            // Read the data.
            SV_ReadPlayer(player);
        }
    }
    SV_AssertSegment(ASEG_END);
}

static void SV_WriteSector(Sector *sec)
{
    int i, type;
    float flooroffx           = P_GetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_X);
    float flooroffy           = P_GetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_Y);
    float ceiloffx            = P_GetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_X);
    float ceiloffy            = P_GetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_Y);
    byte lightlevel           = (byte) (255.f * P_GetFloatp(sec, DMU_LIGHT_LEVEL));
    short floorheight         = (short) P_GetIntp(sec, DMU_FLOOR_HEIGHT);
    short ceilingheight       = (short) P_GetIntp(sec, DMU_CEILING_HEIGHT);
    short floorFlags          = (short) P_GetIntp(sec, DMU_FLOOR_FLAGS);
    short ceilingFlags        = (short) P_GetIntp(sec, DMU_CEILING_FLAGS);
    Material *floorMaterial   = (Material *)P_GetPtrp(sec, DMU_FLOOR_MATERIAL);
    Material *ceilingMaterial = (Material *)P_GetPtrp(sec, DMU_CEILING_MATERIAL);

    xsector_t *xsec = P_ToXSector(sec);

#if !__JHEXEN__
    // Determine type.
    if(xsec->xg)
        type = sc_xg1;
    else
#endif
        if(!FEQUAL(flooroffx, 0) || !FEQUAL(flooroffy, 0) || !FEQUAL(ceiloffx, 0) || !FEQUAL(ceiloffy, 0))
        type = sc_ploff;
    else
        type = sc_normal;

    // Type byte.
    SV_WriteByte(type);

    // Version.
    // 2: Surface colors.
    // 3: Surface flags.
    SV_WriteByte(3); // write a version byte.

    SV_WriteShort(floorheight);
    SV_WriteShort(ceilingheight);
    SV_WriteShort(MaterialArchive_FindUniqueSerialId(materialArchive, floorMaterial));
    SV_WriteShort(MaterialArchive_FindUniqueSerialId(materialArchive, ceilingMaterial));
    SV_WriteShort(floorFlags);
    SV_WriteShort(ceilingFlags);
#if __JHEXEN__
    SV_WriteShort((short) lightlevel);
#else
    SV_WriteByte(lightlevel);
#endif

    float rgb[3];
    P_GetFloatpv(sec, DMU_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        SV_WriteByte((byte)(255.f * rgb[i]));

    P_GetFloatpv(sec, DMU_FLOOR_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        SV_WriteByte((byte)(255.f * rgb[i]));

    P_GetFloatpv(sec, DMU_CEILING_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        SV_WriteByte((byte)(255.f * rgb[i]));

    SV_WriteShort(xsec->special);
    SV_WriteShort(xsec->tag);

#if __JHEXEN__
    SV_WriteShort(xsec->seqType);
#endif

    if(type == sc_ploff
#if !__JHEXEN__
       || type == sc_xg1
#endif
       )
    {
        SV_WriteFloat(flooroffx);
        SV_WriteFloat(flooroffy);
        SV_WriteFloat(ceiloffx);
        SV_WriteFloat(ceiloffy);
    }

#if !__JHEXEN__
    if(xsec->xg)                 // Extended General?
    {
        SV_WriteXGSector(sec);
    }

    // Count the number of sound targets
    if(xsec->soundTarget)
        numSoundTargets++;
#endif
}

/**
 * Reads all versions of archived sectors.
 * Including the old Ver1.
 */
static void SV_ReadSector(Sector* sec)
{
    int i, ver = 1;
    int type = 0;
    Material* floorMaterial = NULL, *ceilingMaterial = NULL;
    byte rgb[3], lightlevel;
    xsector_t* xsec = P_ToXSector(sec);
    int fh, ch;

    // A type byte?
#if __JHEXEN__
    if(mapVersion < 4)
        type = sc_ploff;
    else
#else
    if(hdr->version <= 1)
        type = sc_normal;
    else
#endif
        type = SV_ReadByte();

    // A version byte?
#if __JHEXEN__
    if(mapVersion > 2)
#else
    if(hdr->version > 4)
#endif
        ver = SV_ReadByte();

    fh = SV_ReadShort();
    ch = SV_ReadShort();

    P_SetIntp(sec, DMU_FLOOR_HEIGHT, fh);
    P_SetIntp(sec, DMU_CEILING_HEIGHT, ch);
#if __JHEXEN__
    // Update the "target heights" of the planes.
    P_SetIntp(sec, DMU_FLOOR_TARGET_HEIGHT, fh);
    P_SetIntp(sec, DMU_CEILING_TARGET_HEIGHT, ch);
    // The move speed is not saved; can cause minor problems.
    P_SetIntp(sec, DMU_FLOOR_SPEED, 0);
    P_SetIntp(sec, DMU_CEILING_SPEED, 0);
#endif

#if !__JHEXEN__
    if(hdr->version == 1)
    {
        // The flat numbers are absolute lump indices.
        Uri* uri = Uri_NewWithPath2("Flats:", RC_NULL);
        Uri_SetPath(uri, Str_Text(W_LumpName(SV_ReadShort())));
        floorMaterial = (Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));

        Uri_SetPath(uri, Str_Text(W_LumpName(SV_ReadShort())));
        ceilingMaterial = (Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
        Uri_Delete(uri);
    }
    else if(hdr->version >= 4)
#endif
    {
        // The flat numbers are actually archive numbers.
        floorMaterial   = SV_GetArchiveMaterial(SV_ReadShort(), 0);
        ceilingMaterial = SV_GetArchiveMaterial(SV_ReadShort(), 0);
    }

    P_SetPtrp(sec, DMU_FLOOR_MATERIAL,   floorMaterial);
    P_SetPtrp(sec, DMU_CEILING_MATERIAL, ceilingMaterial);

    if(ver >= 3)
    {
        P_SetIntp(sec, DMU_FLOOR_FLAGS,   SV_ReadShort());
        P_SetIntp(sec, DMU_CEILING_FLAGS, SV_ReadShort());
    }

#if __JHEXEN__
    lightlevel = (byte) SV_ReadShort();
#else
    // In Ver1 the light level is a short
    if(hdr->version == 1)
        lightlevel = (byte) SV_ReadShort();
    else
        lightlevel = SV_ReadByte();
#endif
    P_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) lightlevel / 255.f);

#if !__JHEXEN__
    if(hdr->version > 1)
#endif
    {
        SV_Read(rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_COLOR_RED + i, rgb[i] / 255.f);
    }

    // Ver 2 includes surface colours
    if(ver >= 2)
    {
        SV_Read(rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_FLOOR_COLOR_RED + i, rgb[i] / 255.f);

        SV_Read(rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_CEILING_COLOR_RED + i, rgb[i] / 255.f);
    }

    xsec->special = SV_ReadShort();
    /*xsec->tag =*/ SV_ReadShort();

#if __JHEXEN__
    xsec->seqType = seqtype_t(SV_ReadShort());
#endif

    if(type == sc_ploff
#if !__JHEXEN__
       || type == sc_xg1
#endif
       )
    {
        P_SetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_X, SV_ReadFloat());
        P_SetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_Y, SV_ReadFloat());
        P_SetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_X, SV_ReadFloat());
        P_SetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_Y, SV_ReadFloat());
    }

#if !__JHEXEN__
    if(type == sc_xg1)
        SV_ReadXGSector(sec);
#endif

#if !__JHEXEN__
    if(hdr->version <= 1)
#endif
    {
        xsec->specialData = 0;
    }

    // We'll restore the sound targets latter on
    xsec->soundTarget = 0;
}

static void SV_WriteLine(Line *li)
{
    xline_t *xli = P_ToXLine(li);

    lineclass_t type;
#if !__JHEXEN__
    if(xli->xg)
        type =  lc_xg1;
    else
#endif
        type = lc_normal;
    SV_WriteByte(type);

    // Version.
    // 2: Per surface texture offsets.
    // 2: Surface colors.
    // 3: "Mapped by player" values.
    // 3: Surface flags.
    // 4: Engine-side line flags.
    SV_WriteByte(4); // Write a version byte

    SV_WriteShort(P_GetIntp(li, DMU_FLAGS));
    SV_WriteShort(xli->flags);

    for(int i = 0; i < MAXPLAYERS; ++i)
        SV_WriteByte(xli->mapped[i]);

#if __JHEXEN__
    SV_WriteByte(xli->special);
    SV_WriteByte(xli->arg1);
    SV_WriteByte(xli->arg2);
    SV_WriteByte(xli->arg3);
    SV_WriteByte(xli->arg4);
    SV_WriteByte(xli->arg5);
#else
    SV_WriteShort(xli->special);
    SV_WriteShort(xli->tag);
#endif

    // For each side
    float rgba[4];
    for(int i = 0; i < 2; ++i)
    {
        Side *si = (Side *)P_GetPtrp(li, (i? DMU_BACK:DMU_FRONT));
        if(!si) continue;

        SV_WriteShort(P_GetIntp(si, DMU_TOP_MATERIAL_OFFSET_X));
        SV_WriteShort(P_GetIntp(si, DMU_TOP_MATERIAL_OFFSET_Y));
        SV_WriteShort(P_GetIntp(si, DMU_MIDDLE_MATERIAL_OFFSET_X));
        SV_WriteShort(P_GetIntp(si, DMU_MIDDLE_MATERIAL_OFFSET_Y));
        SV_WriteShort(P_GetIntp(si, DMU_BOTTOM_MATERIAL_OFFSET_X));
        SV_WriteShort(P_GetIntp(si, DMU_BOTTOM_MATERIAL_OFFSET_Y));

        SV_WriteShort(P_GetIntp(si, DMU_TOP_FLAGS));
        SV_WriteShort(P_GetIntp(si, DMU_MIDDLE_FLAGS));
        SV_WriteShort(P_GetIntp(si, DMU_BOTTOM_FLAGS));

        SV_WriteShort(MaterialArchive_FindUniqueSerialId(materialArchive, (Material *)P_GetPtrp(si, DMU_TOP_MATERIAL)));
        SV_WriteShort(MaterialArchive_FindUniqueSerialId(materialArchive, (Material *)P_GetPtrp(si, DMU_BOTTOM_MATERIAL)));
        SV_WriteShort(MaterialArchive_FindUniqueSerialId(materialArchive, (Material *)P_GetPtrp(si, DMU_MIDDLE_MATERIAL)));

        P_GetFloatpv(si, DMU_TOP_COLOR, rgba);
        for(int k = 0; k < 3; ++k)
            SV_WriteByte((byte)(255 * rgba[k]));

        P_GetFloatpv(si, DMU_BOTTOM_COLOR, rgba);
        for(int k = 0; k < 3; ++k)
            SV_WriteByte((byte)(255 * rgba[k]));

        P_GetFloatpv(si, DMU_MIDDLE_COLOR, rgba);
        for(int k = 0; k < 4; ++k)
            SV_WriteByte((byte)(255 * rgba[k]));

        SV_WriteLong(P_GetIntp(si, DMU_MIDDLE_BLENDMODE));
        SV_WriteShort(P_GetIntp(si, DMU_FLAGS));
    }

#if !__JHEXEN__
    // Extended General?
    if(xli->xg)
    {
        SV_WriteXGLine(li);
    }
#endif
}

/**
 * Reads all versions of archived lines.
 * Including the old Ver1.
 */
static void SV_ReadLine(Line *li)
{
    Material *topMaterial = 0, *bottomMaterial = 0, *middleMaterial = 0;
    xline_t *xli = P_ToXLine(li);

    // A type byte?
    lineclass_t type;

#if __JHEXEN__
    if(mapVersion < 4)
#else
    if(hdr->version < 2)
#endif
        type = lc_normal;
    else
        type = lineclass_t(SV_ReadByte());

#ifdef __JHEXEN__
    DENG_UNUSED(type);
#endif

    // A version byte?
    int ver;
#if __JHEXEN__
    if(mapVersion < 3)
#else
    if(hdr->version < 5)
#endif
        ver = 1;
    else
        ver = (int) SV_ReadByte();

    if(ver >= 4)
        P_SetIntp(li, DMU_FLAGS, SV_ReadShort());

    int flags = SV_ReadShort();

    if(xli->flags & ML_TWOSIDED)
        flags |= ML_TWOSIDED;

    if(ver < 4)
    {
        // Translate old line flags.
        int ddLineFlags = 0;

        if(flags & 0x0001) // old ML_BLOCKING flag
        {
            ddLineFlags |= DDLF_BLOCKING;
            flags &= ~0x0001;
        }

        if(flags & 0x0008) // old ML_DONTPEGTOP flag
        {
            ddLineFlags |= DDLF_DONTPEGTOP;
            flags &= ~0x0008;
        }

        if(flags & 0x0010) // old ML_DONTPEGBOTTOM flag
        {
            ddLineFlags |= DDLF_DONTPEGBOTTOM;
            flags &= ~0x0010;
        }

        P_SetIntp(li, DMU_FLAGS, ddLineFlags);
    }

    if(ver < 3)
    {
        if(flags & ML_MAPPED)
        {
            int lineIDX = P_ToIndex(li);

            // Set line as having been seen by all players..
            memset(xli->mapped, 0, sizeof(xli->mapped));
            for(int i = 0; i < MAXPLAYERS; ++i)
                P_SetLineAutomapVisibility(i, lineIDX, true);
        }
    }

    xli->flags = flags;

    if(ver >= 3)
    {
        for(int i = 0; i < MAXPLAYERS; ++i)
            xli->mapped[i] = SV_ReadByte();
    }

#if __JHEXEN__
    xli->special = SV_ReadByte();
    xli->arg1 = SV_ReadByte();
    xli->arg2 = SV_ReadByte();
    xli->arg3 = SV_ReadByte();
    xli->arg4 = SV_ReadByte();
    xli->arg5 = SV_ReadByte();
#else
    xli->special = SV_ReadShort();
    /*xli->tag =*/ SV_ReadShort();
#endif

    // For each side
    for(int i = 0; i < 2; ++i)
    {
        Side *si = (Side *)P_GetPtrp(li, (i? DMU_BACK:DMU_FRONT));
        if(!si) continue;

        // Versions latter than 2 store per surface texture offsets.
        if(ver >= 2)
        {
            float offset[2];

            offset[VX] = (float) SV_ReadShort();
            offset[VY] = (float) SV_ReadShort();
            P_SetFloatpv(si, DMU_TOP_MATERIAL_OFFSET_XY, offset);

            offset[VX] = (float) SV_ReadShort();
            offset[VY] = (float) SV_ReadShort();
            P_SetFloatpv(si, DMU_MIDDLE_MATERIAL_OFFSET_XY, offset);

            offset[VX] = (float) SV_ReadShort();
            offset[VY] = (float) SV_ReadShort();
            P_SetFloatpv(si, DMU_BOTTOM_MATERIAL_OFFSET_XY, offset);
        }
        else
        {
            float offset[2];

            offset[VX] = (float) SV_ReadShort();
            offset[VY] = (float) SV_ReadShort();

            P_SetFloatpv(si, DMU_TOP_MATERIAL_OFFSET_XY,    offset);
            P_SetFloatpv(si, DMU_MIDDLE_MATERIAL_OFFSET_XY, offset);
            P_SetFloatpv(si, DMU_BOTTOM_MATERIAL_OFFSET_XY, offset);
        }

        if(ver >= 3)
        {
            P_SetIntp(si, DMU_TOP_FLAGS,    SV_ReadShort());
            P_SetIntp(si, DMU_MIDDLE_FLAGS, SV_ReadShort());
            P_SetIntp(si, DMU_BOTTOM_FLAGS, SV_ReadShort());
        }

#if !__JHEXEN__
        if(hdr->version >= 4)
#endif
        {
            topMaterial    = SV_GetArchiveMaterial(SV_ReadShort(), 1);
            bottomMaterial = SV_GetArchiveMaterial(SV_ReadShort(), 1);
            middleMaterial = SV_GetArchiveMaterial(SV_ReadShort(), 1);
        }

        P_SetPtrp(si, DMU_TOP_MATERIAL,    topMaterial);
        P_SetPtrp(si, DMU_BOTTOM_MATERIAL, bottomMaterial);
        P_SetPtrp(si, DMU_MIDDLE_MATERIAL, middleMaterial);

        // Ver2 includes surface colours
        if(ver >= 2)
        {
            float rgba[4];
            int flags;

            for(int k = 0; k < 3; ++k)
                rgba[k] = (float) SV_ReadByte() / 255.f;
            rgba[3] = 1;
            P_SetFloatpv(si, DMU_TOP_COLOR, rgba);

            for(int k = 0; k < 3; ++k)
                rgba[k] = (float) SV_ReadByte() / 255.f;
            rgba[3] = 1;
            P_SetFloatpv(si, DMU_BOTTOM_COLOR, rgba);

            for(int k = 0; k < 4; ++k)
                rgba[k] = (float) SV_ReadByte() / 255.f;
            P_SetFloatpv(si, DMU_MIDDLE_COLOR, rgba);

            P_SetIntp(si, DMU_MIDDLE_BLENDMODE, SV_ReadLong());

            flags = SV_ReadShort();
#if __JHEXEN__
            if(mapVersion < 12)
#else
            if(hdr->version < 12)
#endif
            {
                if(P_GetIntp(si, DMU_FLAGS) & SDF_SUPPRESS_BACK_SECTOR)
                    flags |= SDF_SUPPRESS_BACK_SECTOR;
            }
            P_SetIntp(si, DMU_FLAGS, flags);
        }
    }

#if !__JHEXEN__
    if(type == lc_xg1)
        SV_ReadXGLine(li);
#endif
}

#if __JHEXEN__
static void SV_WritePolyObj(Polyobj *po)
{
    DENG_ASSERT(po != 0);

    SV_WriteByte(1); // write a version byte.

    SV_WriteLong(po->tag);
    SV_WriteLong(po->angle);
    SV_WriteLong(FLT2FIX(po->origin[VX]));
    SV_WriteLong(FLT2FIX(po->origin[VY]));
}

static int SV_ReadPolyObj()
{
    int ver = (mapVersion >= 3)? SV_ReadByte() : 0;
    DENG_UNUSED(ver);

    Polyobj *po = Polyobj_ByTag(SV_ReadLong());
    DENG_ASSERT(po != 0);

    angle_t angle = angle_t(SV_ReadLong());
    Polyobj_Rotate(po, angle);
    po->destAngle = angle;

    float const origX = FIX2FLT(SV_ReadLong());
    float const origY = FIX2FLT(SV_ReadLong());
    Polyobj_MoveXY(po, origX - po->origin[VX], origY - po->origin[VY]);

    /// @todo What about speed? It isn't saved at all?

    return true;
}
#endif

static void writeMapElements(Writer *writer)
{
    SV_BeginSegment(ASEG_MAP_ELEMENTS);

    for(int i = 0; i < numsectors; ++i)
    {
        SV_WriteSector((Sector *)P_ToPtr(DMU_SECTOR, i));
    }

    for(int i = 0; i < numlines; ++i)
    {
        SV_WriteLine((Line *)P_ToPtr(DMU_LINE, i));
    }

#if __JHEXEN__
    SV_BeginSegment(ASEG_POLYOBJS);
    SV_WriteLong(numpolyobjs);
    for(int i = 0; i < numpolyobjs; ++i)
    {
        SV_WritePolyObj(Polyobj_ById(i));
    }
#endif
}

static void readMapElements(Reader *reader)
{
    SV_AssertSegment(ASEG_MAP_ELEMENTS);

    // Load sectors.
    for(int i = 0; i < numsectors; ++i)
    {
        SV_ReadSector((Sector *)P_ToPtr(DMU_SECTOR, i));
    }

    // Load lines.
    for(int i = 0; i < numlines; ++i)
    {
        SV_ReadLine((Line *)P_ToPtr(DMU_LINE, i));
    }

#if __JHEXEN__
    // Load polyobjects.
    SV_AssertSegment(ASEG_POLYOBJS);

    long const writtenPolyobjCount = SV_ReadLong();
    DENG_ASSERT(writtenPolyobjCount == numpolyobjs);
    for(int i = 0; i < writtenPolyobjCount; ++i)
    {
        SV_ReadPolyObj();
    }
#endif
}

#if __JHEXEN__
static void SV_WriteMovePoly(polyevent_t const *th)
{
    DENG_ASSERT(th != 0);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(th->polyobj);
    SV_WriteLong(th->intSpeed);
    SV_WriteLong(th->dist);
    SV_WriteLong(th->fangle);
    SV_WriteLong(FLT2FIX(th->speed[VX]));
    SV_WriteLong(FLT2FIX(th->speed[VY]));
}

static int SV_ReadMovePoly(polyevent_t *th, int /*mapVersion*/)
{
    DENG_ASSERT(th != 0);

    if(mapVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        th->polyobj     = SV_ReadLong();
        th->intSpeed    = SV_ReadLong();
        th->dist        = SV_ReadLong();
        th->fangle      = SV_ReadLong();
        th->speed[VX]   = FIX2FLT(SV_ReadLong());
        th->speed[VY]   = FIX2FLT(SV_ReadLong());
    }
    else
    {
        // Its in the old pre V4 format which serialized polyevent_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->polyobj     = SV_ReadLong();
        th->intSpeed    = SV_ReadLong();
        th->dist        = SV_ReadLong();
        th->fangle      = SV_ReadLong();
        th->speed[VX]   = FIX2FLT(SV_ReadLong());
        th->speed[VY]   = FIX2FLT(SV_ReadLong());
    }

    th->thinker.function = T_MovePoly;

    return true; // Add this thinker.
}
#endif // __JHEXEN__

/**
 * Serializes the specified thinker and writes it to save state.
 *
 * @param th  The thinker to be serialized.
 */
static int writeThinker(thinker_t *th, void *context)
{
    DENG_ASSERT(th != 0 && context != 0);
    Writer *writer = (Writer *) context;

    // We are only concerned with thinkers we have save info for.
    ThinkerClassInfo *thInfo = infoForThinker(*th);
    if(!thInfo) return false;

    // Are we excluding players?
    if(thingArchiveExcludePlayers)
    {
        if(th->function == (thinkfunc_t) P_MobjThinker && ((mobj_t *) th)->player)
            return false; // Continue iteration.
    }

    // Only the server saves this class of thinker?
    if((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT)
        return false;

    // Write the header block for this thinker.
    SV_WriteByte(thInfo->thinkclass); // Thinker type byte.
    SV_WriteByte(th->inStasis? 1 : 0); // In stasis?

    // Write the thinker data.
    thInfo->writeFunc(th, writer);

    return false; // Continue iteration.
}

/**
 * Serializes thinkers for both client and server.
 *
 * @note Clients do not save data for all thinkers. In some cases the server
 * will send it anyway (so saving it would just bloat client save states).
 *
 * @note Some thinker classes are NEVER saved by clients.
 */
static void writeThinkers(Writer *writer)
{
    SV_BeginSegment(ASEG_THINKERS);
    {
#if __JHEXEN__
        SV_WriteLong(thingArchiveSize); // number of mobjs.
#endif

        // Serialize qualifying thinkers.
        Thinker_Iterate(0/*all thinkers*/, writeThinker, writer);
    }
    SV_WriteByte(TC_END);
}

static int restoreMobjLinks(thinker_t *th, void *parameters)
{
    DENG_UNUSED(parameters);

    if(th->function != (thinkfunc_t) P_MobjThinker)
        return false; // Continue iteration.

    mobj_t *mo = (mobj_t *) th;
    mo->target = SV_GetArchiveThing(PTR2INT(mo->target), &mo->target);
    mo->onMobj = SV_GetArchiveThing(PTR2INT(mo->onMobj), &mo->onMobj);

#if __JHEXEN__
    switch(mo->type)
    {
    // Just tracer
    case MT_BISH_FX:
    case MT_HOLY_FX:
    case MT_DRAGON:
    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
    case MT_MINOTAUR:
    case MT_SORCFX1:
        if(mapVersion >= 3)
        {
            mo->tracer = SV_GetArchiveThing(PTR2INT(mo->tracer), &mo->tracer);
        }
        else
        {
            mo->tracer = SV_GetArchiveThing(mo->special1, &mo->tracer);
            mo->special1 = 0;
        }
        break;

    // Just special2
    case MT_LIGHTNING_FLOOR:
    case MT_LIGHTNING_ZAP:
        mo->special2 = PTR2INT(SV_GetArchiveThing(mo->special2, &mo->special2));
        break;

    // Both tracer and special2
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(mapVersion >= 3)
        {
            mo->tracer = SV_GetArchiveThing(PTR2INT(mo->tracer), &mo->tracer);
        }
        else
        {
            mo->tracer = SV_GetArchiveThing(mo->special1, &mo->tracer);
            mo->special1 = 0;
        }
        mo->special2 = PTR2INT(SV_GetArchiveThing(mo->special2, &mo->special2));
        break;

    default:
        break;
    }
#else
# if __JDOOM__ || __JDOOM64__
    mo->tracer = SV_GetArchiveThing(PTR2INT(mo->tracer), &mo->tracer);
# endif
# if __JHERETIC__
    mo->generator = SV_GetArchiveThing(PTR2INT(mo->generator), &mo->generator);
# endif
#endif

    return false; // Continue iteration.
}

static int removeThinkerWorker(thinker_t *th, void *context)
{
    DENG_UNUSED(context);

    if(th->function == (thinkfunc_t) P_MobjThinker)
        P_MobjRemove((mobj_t *) th, true);
    else
        Z_Free(th);

    return false; // Continue iteration.
}

static void removeLoadSpawnedThinkers()
{
#if !__JHEXEN__
    if(!IS_SERVER) return; // Not for us.
#endif

    Thinker_Iterate(0 /*all thinkers*/, removeThinkerWorker, 0/*no parameters*/);
    Thinker_Init();
}

#if __JHEXEN__
static bool mobjtypeHasCorpse(mobjtype_t type)
{
    // Only corpses that call A_QueueCorpse from death routine.
    /// @todo fixme: What about mods? Look for this action in the death
    /// state sequence?
    switch(type)
    {
    case MT_CENTAUR:
    case MT_CENTAURLEADER:
    case MT_DEMON:
    case MT_DEMON2:
    case MT_WRAITH:
    case MT_WRAITHB:
    case MT_BISHOP:
    case MT_ETTIN:
    case MT_PIG:
    case MT_CENTAUR_SHIELD:
    case MT_CENTAUR_SWORD:
    case MT_DEMONCHUNK1:
    case MT_DEMONCHUNK2:
    case MT_DEMONCHUNK3:
    case MT_DEMONCHUNK4:
    case MT_DEMONCHUNK5:
    case MT_DEMON2CHUNK1:
    case MT_DEMON2CHUNK2:
    case MT_DEMON2CHUNK3:
    case MT_DEMON2CHUNK4:
    case MT_DEMON2CHUNK5:
    case MT_FIREDEMON_SPLOTCH1:
    case MT_FIREDEMON_SPLOTCH2:
        return true;

    default: return false;
    }
}

static int rebuildCorpseQueueWorker(thinker_t *th, void *parameters)
{
    DENG_UNUSED(parameters);

    mobj_t *mo = (mobj_t *) th;

    // Must be a non-iced corpse.
    if((mo->flags & MF_CORPSE) && !(mo->flags & MF_ICECORPSE) &&
       mobjtypeHasCorpse(mobjtype_t(mo->type)))
    {
        P_AddCorpseToQueue(mo);
    }

    return false; // Continue iteration.
}

/**
 * @todo fixme: the corpse queue should be serialized (original order unknown).
 */
static void rebuildCorpseQueue()
{
    P_InitCorpseQueue();
    // Search the thinker list for corpses and place them in the queue.
    Thinker_Iterate((thinkfunc_t) P_MobjThinker, rebuildCorpseQueueWorker, NULL/*no params*/);
}
#endif

/**
 * Update the references between thinkers. To be called during the load
 * process to finalize the loaded thinkers.
 */
static void relinkThinkers()
{
#if __JHEXEN__
    Thinker_Iterate((thinkfunc_t) P_MobjThinker, restoreMobjLinks, 0/*no params*/);

    P_CreateTIDList();
    rebuildCorpseQueue();

#else
    if(IS_SERVER)
    {
        Thinker_Iterate((thinkfunc_t) P_MobjThinker, restoreMobjLinks, 0/*no params*/);

        for(int i = 0; i < numlines; ++i)
        {
            xline_t *xline = P_ToXLine((Line *)P_ToPtr(DMU_LINE, i));
            if(!xline->xg) continue;

            xline->xg->activator = SV_GetArchiveThing(PTR2INT(xline->xg->activator),
                                                      &xline->xg->activator);
        }
    }
#endif
}

/**
 * Deserializes and then spawns thinkers for both client and server.
 */
static void readThinkers(Reader *reader)
{
#if __JHEXEN__
    int const arcMapVersion = mapVersion;
#else
    int const arcMapVersion = hdr->version;
#endif

#if __JHEXEN__
    bool const formatHasStasisInfo = (mapVersion >= 6);
#else
    bool const formatHasStasisInfo = (hdr->version >= 6);
#endif

    removeLoadSpawnedThinkers();

#if __JHEXEN__
    if(mapVersion < 4)
        SV_AssertSegment(ASEG_MOBJS);
    else
#endif
        SV_AssertSegment(ASEG_THINKERS);

#if __JHEXEN__
    initTargetPlayers();
    initThingArchiveForLoad(SV_ReadLong() /* num elements */);
#endif

    // Read in saved thinkers.
#if __JHEXEN__
    int i = 0;
    bool reachedSpecialsBlock = (mapVersion >= 4);
#else
    bool reachedSpecialsBlock = (hdr->version >= 5);
#endif
    byte tClass = 0;
    for(;;)
    {
#if __JHEXEN__
        if(reachedSpecialsBlock)
#endif
            tClass = SV_ReadByte();

#if __JHEXEN__
        if(mapVersion < 4)
        {
            if(reachedSpecialsBlock) // Have we started on the specials yet?
            {
                // Versions prior to 4 used a different value to mark
                // the end of the specials data and the thinker class ids
                // are differrent, so we need to manipulate the thinker
                // class identifier value.
                if(tClass != TC_END)
                    tClass += 2;
            }
            else
            {
                tClass = TC_MOBJ;
            }

            if(tClass == TC_MOBJ && (uint)i == thingArchiveSize)
            {
                SV_AssertSegment(ASEG_THINKERS);
                // We have reached the begining of the "specials" block.
                reachedSpecialsBlock = true;
                continue;
            }
        }
#else
        if(hdr->version < 5)
        {
            if(reachedSpecialsBlock)
            {
                // Versions prior to 5 used a different value to mark
                // the end of the specials data so we need to manipulate
                // the thinker class identifier value.
                if(tClass == PRE_VER5_END_SPECIALS)
                    tClass = TC_END;
                else
                    tClass += 3;
            }
            else if(tClass == TC_END)
            {
                // We have reached the begining of the "specials" block.
                reachedSpecialsBlock = true;
                continue;
            }
        }
#endif
        if(tClass == TC_END)
            break; // End of the list.

        ThinkerClassInfo *thInfo = infoForThinkerClass(thinkerclass_t(tClass));
        DENG_ASSERT(thInfo != 0);
        // Not for us? (it shouldn't be here anyway!).
        DENG_ASSERT(!((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT));

        // Mobjs use a special engine-side allocator.
        thinker_t *th = 0;
        if(thInfo->thinkclass == TC_MOBJ)
        {
            th = reinterpret_cast<thinker_t *>(Mobj_CreateXYZ((thinkfunc_t) P_MobjThinker, 0, 0, 0, 0, 64, 64, 0));
        }
        else
        {
            th = reinterpret_cast<thinker_t *>(Z_Calloc(thInfo->size, PU_MAP, 0));
        }

        bool putThinkerInStasis = (formatHasStasisInfo? CPP_BOOL(SV_ReadByte()) : false);

        if(thInfo->readFunc(th, reader, arcMapVersion))
        {
            Thinker_Add(th);
        }

        if(putThinkerInStasis)
        {
            Thinker_SetStasis(th, true);
        }

#if __JHEXEN__
        if(tClass == TC_MOBJ)
            i++;
#endif
    }

    // Update references between thinkers.
    relinkThinkers();
}

static void writeBrain(Writer *writer)
{
#if __JDOOM__
    // Not for us?
    if(!IS_SERVER) return;

    SV_WriteByte(1); // Write a version byte.

    SV_WriteShort(brain.numTargets);
    SV_WriteShort(brain.targetOn);
    SV_WriteByte(brain.easy!=0? 1:0);

    // Write the mobj references using the mobj archive.
    for(int i = 0; i < brain.numTargets; ++i)
    {
        SV_WriteShort(SV_ThingArchiveId(brain.targets[i]));
    }
#endif
}

static void readBrain(Reader *reader)
{
#if __JDOOM__
    // Not for us?
    if(!IS_SERVER) return;

    // No brain data before version 3.
    if(hdr->version < 3) return;

    P_BrainClearTargets();

    int ver = (hdr->version >= 8? SV_ReadByte() : 0);
    int numTargets;
    if(ver >= 1)
    {
        numTargets      = SV_ReadShort();
        brain.targetOn  = SV_ReadShort();
        brain.easy      = (dd_bool)SV_ReadByte();
    }
    else
    {
        numTargets      = SV_ReadByte();
        brain.targetOn  = SV_ReadByte();
        brain.easy      = false;
    }

    for(int i = 0; i < numTargets; ++i)
    {
        P_BrainAddTarget(SV_GetArchiveThing((int) SV_ReadShort(), 0));
    }
#endif
}

static void writeSoundTargets(Writer *writer)
{
#if !__JHEXEN__
    // Not for us?
    if(!IS_SERVER) return;

    // Write the total number.
    SV_WriteLong(numSoundTargets);

    // Write the mobj references using the mobj archive.
    for(int i = 0; i < numsectors; ++i)
    {
        xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, i));

        if(xsec->soundTarget)
        {
            SV_WriteLong(i);
            SV_WriteShort(SV_ThingArchiveId(xsec->soundTarget));
        }
    }
#endif
}

static void readSoundTargets(Reader *reader)
{
#if !__JHEXEN__
    // Not for us?
    if(!IS_SERVER) return;

    // Sound Target data was introduced in ver 5
    if(hdr->version < 5) return;

    // Read the number of targets
    int numsoundtargets = SV_ReadLong();

    // Read in the sound targets.
    for(int i = 0; i < numsoundtargets; ++i)
    {
        xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, SV_ReadLong()));
        DENG_ASSERT(xsec != 0);
        if(!xsec)
        {
            DENG_UNUSED(SV_ReadShort());
            continue;
        }

        xsec->soundTarget = INT2PTR(mobj_t, SV_ReadShort());
        xsec->soundTarget =
            SV_GetArchiveThing(PTR2INT(xsec->soundTarget), &xsec->soundTarget);
    }
#endif
}

static void writeMisc(Writer *writer)
{
#if __JHEXEN__
    SV_BeginSegment(ASEG_MISC);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        SV_WriteLong(localQuakeHappening[i]);
    }
#endif
}

static void readMisc(Reader *reader)
{
#if __JHEXEN__
    SV_AssertSegment(ASEG_MISC);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        localQuakeHappening[i] = SV_ReadLong();
    }
#endif
}

static void writeMap(Writer *writer)
{
#if !__JHEXEN__
    // Clear the sound target count (determined while saving sectors).
    numSoundTargets = 0;
#endif

    SV_BeginSegment(ASEG_MAP_HEADER2);
    {
#if __JHEXEN__
        SV_WriteByte(MY_SAVE_VERSION); // Map version also.

        // Write the map timer
        SV_WriteLong(mapTime);
#endif

        MaterialArchive_Write(materialArchive, writer);
        writeMapElements(writer);
        writeThinkers(writer);
#if __JHEXEN__
        P_WriteMapACScriptData(writer);
        SN_WriteSequences(writer);
#endif
        writeMisc(writer);
        writeBrain(writer);
        writeSoundTargets(writer);
    }
    SV_EndSegment();
}

static void readMap(Reader *reader)
{
    sideArchive = new SideArchive;

    savestatesegment_t mapSegmentId;
    SV_AssertMapSegment(&mapSegmentId);
    {
#if __JHEXEN__
        mapVersion = (mapSegmentId == ASEG_MAP_HEADER2? SV_ReadByte() : 2);

        // Read the map timer.
        mapTime = SV_ReadLong();
#endif

        // Read the material archive for the map.
#if !__JHEXEN__
        if(hdr->version >= 4)
#endif

        MaterialArchive_Read(materialArchive, reader, materialArchiveVersion());
        readMapElements(reader);
        readThinkers(reader);
#if __JHEXEN__
        P_ReadMapACScriptData(reader);
        SN_ReadSequences(reader, mapVersion);
#endif
        readMisc(reader);
        readBrain(reader);
        readSoundTargets(reader);
    }
    SV_AssertSegment(ASEG_END);

    delete sideArchive; sideArchive = 0;
}

void SV_Initialize()
{
    static bool firstInit = true;

    SV_InitIO();
    saveInfo = 0;

    inited = true;
    if(firstInit)
    {
        firstInit         = false;
        playerHeaderOK    = false;
        thingArchive      = 0;
        thingArchiveSize  = 0;
        materialArchive   = 0;
#if __JHEXEN__
        targetPlayerAddrs = 0;
        saveBuffer        = 0;
#else
        numSoundTargets   = 0;
#endif
        // -1 = Not yet chosen/determined.
        cvarLastSlot      = -1;
        cvarQuickSlot     = -1;
    }

    // (Re)Initialize the saved game paths, possibly creating them if they do not exist.
    SV_ConfigureSavePaths();
}

void SV_Shutdown()
{
    if(!inited) return;

    SV_ShutdownIO();
    clearSaveInfo();

    cvarLastSlot  = -1;
    cvarQuickSlot = -1;

    inited = false;
}

MaterialArchive *SV_MaterialArchive()
{
    DENG_ASSERT(inited);
    return materialArchive;
}

SideArchive &SV_SideArchive()
{
    DENG_ASSERT(inited);
    DENG_ASSERT(sideArchive != 0);
    return *sideArchive;
}

static bool openGameSaveFile(Str const *fileName, bool write)
{
#if __JHEXEN__
    if(!write)
    {
        bool result = M_ReadFile(Str_Text(fileName), (char **)&saveBuffer) > 0;
        // Set the save pointer.
        SV_HxSavePtr()->b = saveBuffer;
        return result;
    }
    else
#endif
    {
        SV_OpenFile(fileName, write? "wp" : "rp");
    }

    return SV_File() != 0;
}

static int SV_LoadState(Str const *path, SaveInfo *saveInfo)
{
    DENG_ASSERT(path != 0 && saveInfo != 0);

    playerHeaderOK = false; // Uninitialized.

    if(!openGameSaveFile(path, false))
        return 1; // Failed?

    Reader *reader = SV_NewReader();

    // Read the header again.
    /// @todo Seek past the header straight to the game state.
    {
        SaveInfo *tmp = SaveInfo_New();
        SV_SaveInfo_Read(tmp, reader);
        SaveInfo_Delete(tmp);
    }

    /*
     * Configure global game state:
     */
    hdr = SaveInfo_Header(saveInfo);

    gameEpisode     = hdr->episode - 1;
    gameMap         = hdr->map - 1;

    // Apply the game rules:
#if __JHEXEN__
    gameSkill       = hdr->skill;
#else
    gameSkill       = hdr->skill;
    fastParm        = hdr->fast;
#endif
    deathmatch      = hdr->deathmatch;
    noMonstersParm  = hdr->noMonsters;
#if __JHEXEN__
    randomClassParm = hdr->randomClasses;
#else
    respawnMonsters = hdr->respawnMonsters;
#endif

#if __JHEXEN__
    P_ReadGlobalACScriptData(reader, hdr->version);
#endif

    /*
     * Load the map and configure some game settings.
     */
    briefDisabled = true;
    G_NewGame(gameSkill, gameEpisode, gameMap, 0/*gameMapEntryPoint*/);

    G_SetGameAction(GA_NONE); /// @todo Necessary?

#if !__JHEXEN__
    // Set the time.
    mapTime = hdr->mapTime;
#endif

#if !__JHEXEN__
    initThingArchiveForLoad(hdr->version >= 5? SV_ReadLong() : 1024 /* num elements */);
#endif

    readPlayerHeader();

    // Read the player structures
    // We don't have the right to say which players are in the game. The
    // players that already are will continue to be. If the data for a given
    // player is not in the savegame file, he will be notified. The data for
    // players who were saved but are not currently in the game will be
    // discarded.
    dd_bool loaded[MAXPLAYERS], infile[MAXPLAYERS];
    readPlayers(infile, loaded);

#if __JHEXEN__
    Z_Free(saveBuffer);
#endif

    // Create and populate the MaterialArchive.
#ifdef __JHEXEN__
    materialArchive = MaterialArchive_NewEmpty(true /* segment checks */);
#else
    materialArchive = MaterialArchive_NewEmpty(false);
#endif

    // Load the current map state.
#if __JHEXEN__
    readMapState(reader, composeGameSavePathForSlot2(BASE_SLOT, gameMap+1));
#else
    readMapState(reader);
#endif

#if !__JHEXEN__
    SV_ReadConsistencyBytes();
    SV_CloseFile();
#endif

    /*
     * Cleanup:
     */
    clearMaterialArchive();
#if !__JHEXEN__
    clearThingArchive();
#endif
#if __JHEXEN__
    clearTargetPlayers();
#endif

    // Notify the players that weren't in the savegame.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        dd_bool notLoaded = false;

#if __JHEXEN__
        if(players[i].plr->inGame)
        {
            // Try to find a saved player that corresponds this one.
            uint k;
            for(k = 0; k < MAXPLAYERS; ++k)
            {
                if(saveToRealPlayerNum[k] == i)
                    break;
            }
            if(k < MAXPLAYERS)
                continue; // Found; don't bother this player.

            players[i].playerState = PST_REBORN;

            if(!i)
            {
                // If the CONSOLEPLAYER isn't in the save, it must be some
                // other player's file?
                P_SetMessage(players, LMF_NO_HIDE, GET_TXT(TXT_LOADMISSING));
            }
            else
            {
                NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
                notLoaded = true;
            }
        }
#else
        if(!loaded[i] && players[i].plr->inGame)
        {
            if(!i)
            {
                P_SetMessage(players, LMF_NO_HIDE, GET_TXT(TXT_LOADMISSING));
            }
            else
            {
                NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
            }
            notLoaded = true;
        }
#endif

        if(notLoaded)
        {
            // Kick this player out, he doesn't belong here.
            DD_Executef(false, "kick %i", i);
        }
    }

#if !__JHEXEN__
    // In netgames, the server tells the clients about this.
    NetSv_LoadGame(SaveInfo_GameId(saveInfo));
#endif

    Reader_Delete(reader);

    return 0;
}

static int loadStateWorker(Str const *path, SaveInfo &saveInfo)
{
    DENG_ASSERT(path != 0);

    int loadError = true; // Failed.

    if(recogniseNativeState(path, &saveInfo))
    {
        loadError = SV_LoadState(path, &saveInfo);
    }
    // Perhaps an original game state?
#if __JDOOM__
    else if(SV_RecogniseState_Dm_v19(path, &saveInfo))
    {
        loadError = SV_LoadState_Dm_v19(path, &saveInfo);
    }
#endif
#if __JHERETIC__
    else if(SV_RecogniseState_Hr_v13(path, &saveInfo))
    {
        loadError = SV_LoadState_Hr_v13(path, &saveInfo);
    }
#endif

    if(loadError) return loadError;

    /*
     * Game state was loaded successfully.
     */

    // Material origin scrollers must be re-spawned for older save state versions.
    saveheader_t const *hdr = SaveInfo_Header(&saveInfo);

    /// @todo Implement SaveInfo format type identifiers.
    if((hdr->magic != (IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC)) ||
       hdr->version <= 10)
    {
        P_SpawnAllMaterialOriginScrollers();
    }

    // Let the engine know where the local players are now.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        R_UpdateConsoleView(i);
    }

    // Inform the engine to perform map setup once more.
    R_SetupMap(0, 0);

    return 0; // Success.
}

dd_bool SV_LoadGame(int slot)
{
    DENG_ASSERT(inited);

#if __JHEXEN__
    int const logicalSlot = BASE_SLOT;
#else
    int const logicalSlot = slot;
#endif

    if(!SV_IsValidSlot(slot))
        return false;

    AutoStr *path = composeGameSavePathForSlot(slot);
    if(Str_IsEmpty(path))
    {
        App_Log(DE2_RES_ERROR, "Game not loaded: path \"%s\" is unreachable", SV_SavePath());
        return false;
    }

    App_Log(DE2_RES_VERBOSE, "Attempting load of save slot #%i...", slot);

#if __JHEXEN__
    // Copy all needed save files to the base slot.
    /// @todo Why do this BEFORE loading?? (G_NewGame() does not load the serialized map state)
    /// @todo Does any caller ever attempt to load the base slot?? (Doesn't seem logical)
    if(slot != BASE_SLOT)
    {
        SV_CopySlot(slot, BASE_SLOT);
    }
#endif

    SaveInfo *saveInfo = SV_SaveInfoForSlot(logicalSlot);
    DENG_ASSERT(saveInfo != 0);

    int loadError = loadStateWorker(path, *saveInfo);
    if(!loadError)
    {
        Con_SetInteger2("game-save-last-slot", slot, SVF_WRITE_OVERRIDE);
    }
    else
    {
        App_Log(DE2_RES_WARNING, "Failed loading save slot #%i", slot);
    }

    return !loadError;
}

void SV_SaveGameClient(uint gameId)
{
#if !__JHEXEN__ // unsupported in libhexen
    DENG_ASSERT(inited);

    player_t *pl = &players[CONSOLEPLAYER];
    mobj_t *mo = pl->plr->mo;
    AutoStr *gameSavePath;
    SaveInfo *saveInfo;

    if(!IS_CLIENT || !mo)
        return;

    playerHeaderOK = false; // Uninitialized.

    gameSavePath = composeGameSavePathForClientGameId(gameId);
    if(!SV_OpenFile(gameSavePath, "wp"))
    {
        App_Log(DE2_RES_WARNING, "SV_SaveGameClient: Failed opening \"%s\" for writing", Str_Text(gameSavePath));
        return;
    }

    // Prepare the header.
    saveInfo = SaveInfo_New();
    SaveInfo_SetGameId(saveInfo, gameId);
    SaveInfo_Configure(saveInfo);

    Writer *writer = SV_NewWriter();
    SaveInfo_Write(saveInfo, writer);

    // Some important information.
    // Our position and look angles.
    SV_WriteLong(FLT2FIX(mo->origin[VX]));
    SV_WriteLong(FLT2FIX(mo->origin[VY]));
    SV_WriteLong(FLT2FIX(mo->origin[VZ]));
    SV_WriteLong(FLT2FIX(mo->floorZ));
    SV_WriteLong(FLT2FIX(mo->ceilingZ));
    SV_WriteLong(mo->angle); /* $unifiedangles */
    SV_WriteFloat(pl->plr->lookDir); /* $unifiedangles */
    writePlayerHeader();
    SV_WritePlayer(CONSOLEPLAYER);

    // Create and populate the MaterialArchive.
    materialArchive = MaterialArchive_New(false);

    writeMap(writer);
    /// @todo No consistency bytes in client saves?

    clearMaterialArchive();

    SV_CloseFile();
    Writer_Delete(writer);
    SaveInfo_Delete(saveInfo);
#else
    DENG_UNUSED(gameId);
#endif
}

void SV_LoadGameClient(uint gameId)
{
#if !__JHEXEN__ // unsupported in libhexen
    DENG_ASSERT(inited);

    player_t *cpl = players + CONSOLEPLAYER;
    mobj_t *mo = cpl->plr->mo;
    AutoStr *gameSavePath;
    SaveInfo *saveInfo;

    if(!IS_CLIENT || !mo)
        return;

    playerHeaderOK = false; // Uninitialized.

    gameSavePath = composeGameSavePathForClientGameId(gameId);
    if(!SV_OpenFile(gameSavePath, "rp"))
    {
        App_Log(DE2_RES_WARNING, "SV_LoadGameClient: Failed opening \"%s\" for reading", Str_Text(gameSavePath));
        return;
    }

    saveInfo = SaveInfo_New();
    Reader *reader = SV_NewReader();
    SV_SaveInfo_Read(saveInfo, reader);

    hdr = SaveInfo_Header(saveInfo);
    if(hdr->magic != MY_CLIENT_SAVE_MAGIC)
    {
        Reader_Delete(reader);
        SaveInfo_Delete(saveInfo);
        SV_CloseFile();
        App_Log(DE2_RES_ERROR, "Client save file format not recognized");
        return;
    }

    gameSkill = skillmode_t( hdr->skill );
    deathmatch = hdr->deathmatch;
    noMonstersParm = hdr->noMonsters;
    respawnMonsters = hdr->respawnMonsters;
    // Do we need to change the map?
    if(gameMap != (unsigned) (hdr->map - 1) || gameEpisode != (unsigned) (hdr->episode - 1))
    {
        gameEpisode = hdr->episode - 1;
        gameMap = hdr->map - 1;
        gameMapEntryPoint = 0;
        G_NewGame(gameSkill, gameEpisode, gameMap, gameMapEntryPoint);
        /// @todo Necessary?
        G_SetGameAction(GA_NONE);
    }
    mapTime = hdr->mapTime;

    P_MobjUnlink(mo);
    mo->origin[VX] = FIX2FLT(SV_ReadLong());
    mo->origin[VY] = FIX2FLT(SV_ReadLong());
    mo->origin[VZ] = FIX2FLT(SV_ReadLong());
    P_MobjLink(mo);
    mo->floorZ = FIX2FLT(SV_ReadLong());
    mo->ceilingZ = FIX2FLT(SV_ReadLong());
    mo->angle = SV_ReadLong(); /* $unifiedangles */
    cpl->plr->lookDir = SV_ReadFloat(); /* $unifiedangles */
    readPlayerHeader();
    SV_ReadPlayer(cpl);

    /**
     * Create and populate the MaterialArchive.
     *
     * @todo Does this really need to be done at all as a client?
     * When the client connects to the server it should send a copy
     * of the map upon joining, so why are we reading it here?
     */
    materialArchive = MaterialArchive_New(false);

    readMap(reader);

    clearMaterialArchive();

    SV_CloseFile();
    Reader_Delete(reader);
    SaveInfo_Delete(saveInfo);
#else
    DENG_UNUSED(gameId);
#endif
}

#if __JHEXEN__
static void readMapState(Reader *reader, Str const *path)
#else
static void readMapState(Reader *reader)
#endif
{
#if __JHEXEN__
    DENG_ASSERT(path != 0);

    App_Log(DE2_DEV_MAP_MSG, "readMapState: Opening file \"%s\"\n", Str_Text(path));

    // Load the file
    size_t bufferSize = M_ReadFile(Str_Text(path), (char**)&saveBuffer);
    if(!bufferSize)
    {
        App_Log(DE2_RES_ERROR, "readMapState: Failed opening \"%s\" for reading", Str_Text(path));
        return;
    }

    SV_HxSavePtr()->b = saveBuffer;
    SV_HxSetSaveEndPtr(saveBuffer + bufferSize);
#endif

    readMap(reader);

#if __JHEXEN__
    clearThingArchive();
    Z_Free(saveBuffer);
#endif
}

static int saveStateWorker(Str const *path, SaveInfo *saveInfo)
{
    App_Log(DE2_LOG_VERBOSE, "saveStateWorker: Attempting save game to \"%s\"", Str_Text(path));

    // In networked games the server tells the clients to save their games.
#if !__JHEXEN__
    NetSv_SaveGame(SaveInfo_GameId(saveInfo));
#endif

    if(!openGameSaveFile(path, true))
    {
        return SV_INVALIDFILENAME; // No success.
    }

    playerHeaderOK = false; // Uninitialized.

    /*
     * Write the game session header.
     */
    Writer *writer = SV_NewWriter();
    SaveInfo_Write(saveInfo, writer);

#if __JHEXEN__
    P_WriteGlobalACScriptData(writer);
#endif

    // Set the mobj archive numbers.
    initThingArchiveForSave();

#if !__JHEXEN__
    SV_WriteLong(thingArchiveSize);
#endif

    // Create and populate the MaterialArchive.
#ifdef __JHEXEN__
    materialArchive = MaterialArchive_New(true /* segment check */);
#else
    materialArchive = MaterialArchive_New(false);
#endif

    writePlayerHeader();
    writePlayers();

#if __JHEXEN__
    // Close the game session file (maps are saved into a seperate file).
    SV_CloseFile();
#endif

    /*
     * Save the map.
     */
#if __JHEXEN__
    // ...map state is actually written to a separate file.
    SV_OpenFile(composeGameSavePathForSlot2(BASE_SLOT, gameMap+1), "wp");
#endif

    writeMap(writer);

    SV_WriteConsistencyBytes(); // To be absolutely sure...
    SV_CloseFile();

    clearMaterialArchive();
#if !__JHEXEN___
    clearThingArchive();
#endif

    Writer_Delete(writer);

    return SV_OK;
}

/**
 * Create a new SaveInfo for the current game session.
 */
static SaveInfo *createSaveInfo(char const *name)
{
    ddstring_t nameStr;
    SaveInfo *info = SaveInfo_New();
    SaveInfo_SetName(info, Str_InitStatic(&nameStr, name));
    SaveInfo_SetGameId(info, SV_GenerateGameId());
    SaveInfo_Configure(info);
    return info;
}

dd_bool SV_SaveGame(int slot, char const *name)
{
    DENG_ASSERT(inited);
    DENG_ASSERT(name != 0);

#if __JHEXEN__
    int const logicalSlot = BASE_SLOT;
#else
    int const logicalSlot = slot;
#endif

    if(!SV_IsValidSlot(slot))
    {
        DENG_ASSERT(!"Invalid slot '%i' specified");
        return false;
    }
    if(!name[0])
    {
        DENG_ASSERT(!"Empty name specified for slot");
        return false;
    }

    AutoStr *path = composeGameSavePathForSlot(logicalSlot);
    if(Str_IsEmpty(path))
    {
        App_Log(DE2_RES_WARNING, "Cannot save game: path \"%s\" is unreachable", SV_SavePath());
        return false;
    }

    SaveInfo *info = createSaveInfo(name);

    int saveError = saveStateWorker(path, info);
    if(!saveError)
    {
        // Swap the save info.
        replaceSaveInfo(logicalSlot, info);

#if __JHEXEN__
        // Copy base slot to destination slot.
        SV_CopySlot(logicalSlot, slot);
#endif

        // The "last" save slot is now this.
        Con_SetInteger2("game-save-last-slot", slot, SVF_WRITE_OVERRIDE);
    }
    else
    {
        // We no longer need the info.
        SaveInfo_Delete(info);

        if(saveError == SV_INVALIDFILENAME)
        {
            App_Log(DE2_RES_ERROR, "Failed opening \"%s\" for writing", Str_Text(path));
        }
    }

    return !saveError;
}

#if __JHEXEN__
void SV_HxSaveClusterMap()
{
    playerHeaderOK = false; // Uninitialized.

    SV_OpenFile(composeGameSavePathForSlot2(BASE_SLOT, gameMap+1), "wp");

    // Set the mobj archive numbers
    initThingArchiveForSave(true /*exclude players*/);

    Writer *writer = SV_NewWriter();

    // Create and populate the MaterialArchive.
    materialArchive = MaterialArchive_New(true);

    writeMap(writer);

    clearMaterialArchive();

    // Close the output file
    SV_CloseFile();

    Writer_Delete(writer);
}

void SV_HxLoadClusterMap()
{
    // Only readMap() uses targetPlayerAddrs, so it's NULLed here for the
    // following check (player mobj redirection).
    targetPlayerAddrs = 0;

    playerHeaderOK = false; // Uninitialized.

    // Create the MaterialArchive.
    materialArchive = MaterialArchive_NewEmpty(true);

    Reader *reader = SV_NewReader();

    // Been here before, load the previous map state.
    readMapState(reader, composeGameSavePathForSlot2(BASE_SLOT, gameMap+1));

    clearMaterialArchive();

    Reader_Delete(reader);
}

void SV_HxBackupPlayersInCluster(playerbackup_t playerBackup[MAXPLAYERS])
{
    DENG_ASSERT(playerBackup);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        playerbackup_t *pb = playerBackup + i;
        player_t *plr = players + i;

        std::memcpy(&pb->player, plr, sizeof(player_t));

        // Make a copy of the inventory states also.
        for(int k = 0; k < NUM_INVENTORYITEM_TYPES; ++k)
        {
            pb->numInventoryItems[k] = P_InventoryCount(i, inventoryitemtype_t(k));
        }
        pb->readyItem = P_InventoryReadyItem(i);
    }
}

void SV_HxRestorePlayersInCluster(playerbackup_t playerBackup[MAXPLAYERS],
    uint entryPoint)
{
    mobj_t *targetPlayerMobj;

    DENG_ASSERT(playerBackup);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        playerbackup_t *pb = playerBackup + i;
        player_t *plr = players + i;
        ddplayer_t *ddplr = plr->plr;
        int oldKeys = 0, oldPieces = 0;
        dd_bool oldWeaponOwned[NUM_WEAPON_TYPES];
        dd_bool wasReborn;

        if(!ddplr->inGame) continue;

        std::memcpy(plr, &pb->player, sizeof(player_t));
        for(int k = 0; k < NUM_INVENTORYITEM_TYPES; ++k)
        {
            // Don't give back the wings of wrath if reborn.
            if(k == IIT_FLY && plr->playerState == PST_REBORN)
                continue;

            for(uint l = 0; l < pb->numInventoryItems[k]; ++l)
            {
                P_InventoryGive(i, inventoryitemtype_t(k), true);
            }
        }
        P_InventorySetReadyItem(i, pb->readyItem);

        ST_LogEmpty(i);
        plr->attacker = NULL;
        plr->poisoner = NULL;

        if(IS_NETGAME || deathmatch)
        {
            // In a network game, force all players to be alive
            if(plr->playerState == PST_DEAD)
            {
                plr->playerState = PST_REBORN;
            }

            if(!deathmatch)
            {
                // Cooperative net-play; retain keys and weapons.
                oldKeys = plr->keys;
                oldPieces = plr->pieces;
                for(int j = 0; j < NUM_WEAPON_TYPES; ++j)
                {
                    oldWeaponOwned[j] = plr->weapons[j].owned;
                }
            }
        }

        wasReborn = (plr->playerState == PST_REBORN);

        if(deathmatch)
        {
            memset(plr->frags, 0, sizeof(plr->frags));
            ddplr->mo = NULL;
            G_DeathMatchSpawnPlayer(i);
        }
        else
        {
            playerstart_t const *start;

            if((start = P_GetPlayerStart(entryPoint, i, false)))
            {
                mapspot_t const *spot = &mapSpots[start->spot];
                P_SpawnPlayer(i, cfg.playerClass[i], spot->origin[VX],
                              spot->origin[VY], spot->origin[VZ], spot->angle,
                              spot->flags, false, true);
            }
            else
            {
                P_SpawnPlayer(i, cfg.playerClass[i], 0, 0, 0, 0,
                              MSF_Z_FLOOR, true, true);
            }
        }

        if(wasReborn && IS_NETGAME && !deathmatch)
        {
            // Restore keys and weapons when reborn in co-op.
            plr->keys = oldKeys;
            plr->pieces = oldPieces;

            int bestWeapon = 0;
            for(int k = 0; k < NUM_WEAPON_TYPES; ++k)
            {
                if(oldWeaponOwned[k])
                {
                    bestWeapon = k;
                    plr->weapons[k].owned = true;
                }
            }

            plr->ammo[AT_BLUEMANA].owned = 25; /// @todo values.ded
            plr->ammo[AT_GREENMANA].owned = 25; /// @todo values.ded

            // Bring up the best weapon.
            if(bestWeapon)
            {
                plr->pendingWeapon = weapontype_t(bestWeapon);
            }
        }
    }

    targetPlayerMobj = 0;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr = players + i;
        ddplayer_t *ddplr = plr->plr;

        if(!ddplr->inGame) continue;

        if(!targetPlayerMobj)
        {
            targetPlayerMobj = ddplr->mo;
        }
    }

    /// @todo Redirect anything targeting a player mobj
    /// FIXME! This only supports single player games!!
    if(targetPlayerAddrs)
    {
        for(targetplraddress_t *p = targetPlayerAddrs; p; p = p->next)
        {
            *(p->address) = targetPlayerMobj;
        }

        clearTargetPlayers();

        /*
         * When XG is available in Hexen, call this after updating target player
         * references (after a load) - ds
        // The activator mobjs must be set.
        XL_UpdateActivators();
        */
    }

    // Destroy all things touching players.
    P_TelefragMobjsTouchingPlayers();
}
#endif
