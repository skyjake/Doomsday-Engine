/** @file p_saveg.cpp  Common game-save state management.
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

#include "common.h"
#include "p_saveg.h"

#include "d_net.h"
#include "dmu_lib.h"
#if __JHERETIC__ || __JHEXEN__
#  include "hu_inventory.h"
#endif
#include "mobj.h"
#include "p_actor.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_inventory.h"
#include "p_tick.h"
#include "r_common.h"
#include "p_savedef.h"
#include "p_saveio.h"
#include "polyobjs.h"
#include "mapstatereader.h"
#include "mapstatewriter.h"
#include <de/String>
#include <de/memory.h>
#include <lzss.h>
#include <cstdio>
#include <cstring>

using namespace dmu_lib;

struct playerheader_t
{
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
};

static bool inited = false;

SaveSlots saveSlots(NUMSAVESLOTS);

static SaveInfo const *curInfo;

static playerheader_t playerHeader;
static dd_bool playerHeaderOK;

#if __JHEXEN__
/// Symbolic identifier used to mark references to players in map states.
static ThingSerialId const TargetPlayerId = -2;
#endif

static mobj_t **thingArchive;
int thingArchiveVersion;
uint thingArchiveSize;
static bool thingArchiveExcludePlayers;

static int saveToRealPlayerNum[MAXPLAYERS];
#if __JHEXEN__
static targetplraddress_t *targetPlayerAddrs;
#endif

#if __JHEXEN__
static byte *saveBuffer;
#endif

#if !__JHEXEN__
/**
 * Compose the (possibly relative) path to the game-save associated
 * with @a sessionId.
 *
 * @param sessionId  Unique game-session identifier.
 *
 * @return  File path to the reachable save directory. If the game-save path
 *          is unreachable then a zero-length string is returned instead.
 */
static AutoStr *composeGameSavePathForClientSessionId(uint sessionId)
{
    AutoStr *path = AutoStr_NewStd();
    // Do we have a valid path?
    if(!F_MakePath(SV_ClientSavePath())) return path; // return zero-length string.
    // Compose the full game-save path and filename.
    Str_Appendf(path, "%s" CLIENTSAVEGAMENAME "%08X." SAVEGAMEEXTENSION, SV_ClientSavePath(), sessionId);
    F_TranslatePath(path, path);
    return path;
}
#endif

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
        info->read_Hx_v9(reader);
    }
    else
#endif
    {
        info->read(reader);
    }
}

static bool recognizeNativeState(Str const *path, SaveInfo *info)
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
    if(info->magic() != MY_SAVE_MAGIC && info->magic() != MY_CLIENT_SAVE_MAGIC)
    {
        return false;
    }

    /*
     * Check for unsupported versions.
     */
    if(info->version() > MY_SAVE_VERSION) // Future version?
    {
        return false;
    }

#if __JHEXEN__
    // We are incompatible with v3 saves due to an invalid test used to determine
    // present sides (ver3 format's sides contain chunks of junk data).
    if(info->version() == 3)
    {
        return false;
    }
#endif

    return true;
}

dd_bool SV_RecogniseGameState(Str const *path, SaveInfo *info)
{
    if(path && info)
    {
        if(recognizeNativeState(path, info))
            return true;

        // Perhaps an original game state?
#if __JDOOM__
        if(SV_RecognizeState_Dm_v19(path, info))
            return true;
#endif
#if __JHERETIC__
        if(SV_RecognizeState_Hr_v13(path, info))
            return true;
#endif
    }
    return false;
}

#if __JHEXEN__
dd_bool SV_HxHaveMapStateForSlot(int slot, uint map)
{
    DENG_ASSERT(inited);
    AutoStr *path = saveSlots.composeSavePathForSlot(slot, (int)map+1);
    if(!path || Str_IsEmpty(path)) return false;
    return SV_ExistingFile(path);
}
#endif

#if __JHEXEN__
void SV_HxInitBaseSlot()
{
    saveSlots.clearSlot(BASE_SLOT);
}
#endif

void SV_InitThingArchiveForLoad(uint size)
{
    thingArchiveSize = size;
    thingArchive = reinterpret_cast<mobj_t **>(M_Calloc(thingArchiveSize * sizeof(*thingArchive)));
}

struct countmobjthinkerstoarchive_params_t
{
    uint count;
    bool excludePlayers;
};

static int countMobjThinkersToArchive(thinker_t *th, void *context)
{
    countmobjthinkerstoarchive_params_t *parms = (countmobjthinkerstoarchive_params_t *) context;
    if(!(Mobj_IsPlayer((mobj_t *) th) && parms->excludePlayers))
    {
        parms->count++;
    }
    return false; // Continue iteration.
}

static void initThingArchiveForSave(bool excludePlayers = false)
{
    // Count the number of things we'll be writing.
    countmobjthinkerstoarchive_params_t parms;
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
    if(thingArchiveVersion >= 1)
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

#if __JHEXEN__
void SV_InitTargetPlayers()
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
    if(thingArchiveVersion < 1)
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
 * Writes the given player's data (not including the ID number).
 */
static void SV_WritePlayer(int playernum, Writer *writer)
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
    Writer_WriteByte(writer, 6);

#if __JHEXEN__
    // Class.
    Writer_WriteByte(writer, cfg.playerClass[playernum]);
#endif

    Writer_WriteInt32(writer, p->playerState);
#if __JHEXEN__
    Writer_WriteInt32(writer, p->class_);    // 2nd class...?
#endif
    Writer_WriteInt32(writer, FLT2FIX(p->viewZ));
    Writer_WriteInt32(writer, FLT2FIX(p->viewHeight));
    Writer_WriteInt32(writer, FLT2FIX(p->viewHeightDelta));
#if !__JHEXEN__
    Writer_WriteFloat(writer, dp->lookDir);
#endif
    Writer_WriteInt32(writer, FLT2FIX(p->bob));
#if __JHEXEN__
    Writer_WriteInt32(writer, p->flyHeight);
    Writer_WriteFloat(writer, dp->lookDir);
    Writer_WriteInt32(writer, p->centering);
#endif
    Writer_WriteInt32(writer, p->health);

#if __JHEXEN__
    for(i = 0; i < getPlayerHeader()->numArmorTypes; ++i)
    {
        Writer_WriteInt32(writer, p->armorPoints[i]);
    }
#else
    Writer_WriteInt32(writer, p->armorPoints);
    Writer_WriteInt32(writer, p->armorType);
#endif

#if __JDOOM64__ || __JHEXEN__
    for(i = 0; i < getPlayerHeader()->numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRST + i);

        Writer_WriteInt32(writer, type);
        Writer_WriteInt32(writer, P_InventoryCount(playernum, type));
    }
    Writer_WriteInt32(writer, P_InventoryReadyItem(playernum));
#endif

    for(i = 0; i < getPlayerHeader()->numPowers; ++i)
    {
        Writer_WriteInt32(writer, p->powers[i]);
    }

#if __JHEXEN__
    Writer_WriteInt32(writer, p->keys);
#else
    for(i = 0; i < getPlayerHeader()->numKeys; ++i)
    {
        Writer_WriteInt32(writer, p->keys[i]);
    }
#endif

#if __JHEXEN__
    Writer_WriteInt32(writer, p->pieces);
#else
    Writer_WriteInt32(writer, p->backpack);
#endif

    for(i = 0; i < getPlayerHeader()->numFrags; ++i)
    {
        Writer_WriteInt32(writer, p->frags[i]);
    }

    Writer_WriteInt32(writer, p->readyWeapon);
    Writer_WriteInt32(writer, p->pendingWeapon);

    for(i = 0; i < getPlayerHeader()->numWeapons; ++i)
    {
        Writer_WriteInt32(writer, p->weapons[i].owned);
    }

    for(i = 0; i < getPlayerHeader()->numAmmoTypes; ++i)
    {
        Writer_WriteInt32(writer, p->ammo[i].owned);
#if !__JHEXEN__
        Writer_WriteInt32(writer, p->ammo[i].max);
#endif
    }

    Writer_WriteInt32(writer, p->attackDown);
    Writer_WriteInt32(writer, p->useDown);

    Writer_WriteInt32(writer, p->cheats);

    Writer_WriteInt32(writer, p->refire);

    Writer_WriteInt32(writer, p->killCount);
    Writer_WriteInt32(writer, p->itemCount);
    Writer_WriteInt32(writer, p->secretCount);

    Writer_WriteInt32(writer, p->damageCount);
    Writer_WriteInt32(writer, p->bonusCount);
#if __JHEXEN__
    Writer_WriteInt32(writer, p->poisonCount);
#endif

    Writer_WriteInt32(writer, dp->extraLight);
    Writer_WriteInt32(writer, dp->fixedColorMap);
    Writer_WriteInt32(writer, p->colorMap);

    for(i = 0; i < numPSprites; ++i)
    {
        pspdef_t *psp = &p->pSprites[i];

        Writer_WriteInt32(writer, PTR2INT(psp->state));
        Writer_WriteInt32(writer, psp->tics);
        Writer_WriteInt32(writer, FLT2FIX(psp->pos[VX]));
        Writer_WriteInt32(writer, FLT2FIX(psp->pos[VY]));
    }

#if !__JHEXEN__
    Writer_WriteInt32(writer, p->didSecret);

    // Added in ver 2 with __JDOOM__
    Writer_WriteInt32(writer, p->flyHeight);
#endif

#if __JHERETIC__
    for(i = 0; i < getPlayerHeader()->numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRST + i);

        Writer_WriteInt32(writer, type);
        Writer_WriteInt32(writer, P_InventoryCount(playernum, type));
    }
    Writer_WriteInt32(writer, P_InventoryReadyItem(playernum));
    Writer_WriteInt32(writer, p->chickenPeck);
#endif

#if __JHERETIC__ || __JHEXEN__
    Writer_WriteInt32(writer, p->morphTics);
#endif

    Writer_WriteInt32(writer, p->airCounter);

#if __JHEXEN__
    Writer_WriteInt32(writer, p->jumpTics);
    Writer_WriteInt32(writer, p->worldTimer);
#elif __JHERETIC__
    Writer_WriteInt32(writer, p->flameCount);

    // Added in ver 2
    Writer_WriteByte(writer, p->class_);
#endif
}

/**
 * Reads a player's data (not including the ID number).
 */
static void SV_ReadPlayer(player_t *p, Reader *reader)
{
    int plrnum = p - players;
    int numPSprites = getPlayerHeader()->numPSprites;
    ddplayer_t *dp = p->plr;

    byte ver = Reader_ReadByte(reader);

#if __JHEXEN__
    cfg.playerClass[plrnum] = playerclass_t(Reader_ReadByte(reader));

    memset(p, 0, sizeof(*p)); // Force everything NULL,
    p->plr = dp;              // but restore the ddplayer pointer.
#endif

    p->playerState     = playerstate_t(Reader_ReadInt32(reader));
#if __JHEXEN__
    p->class_          = playerclass_t(Reader_ReadInt32(reader)); // 2nd class?? (ask Raven...)
#endif

    p->viewZ           = FIX2FLT(Reader_ReadInt32(reader));
    p->viewHeight      = FIX2FLT(Reader_ReadInt32(reader));
    p->viewHeightDelta = FIX2FLT(Reader_ReadInt32(reader));
#if !__JHEXEN__
    dp->lookDir        = Reader_ReadFloat(reader);
#endif
    p->bob             = FIX2FLT(Reader_ReadInt32(reader));
#if __JHEXEN__
    p->flyHeight       = Reader_ReadInt32(reader);
    dp->lookDir        = Reader_ReadFloat(reader);
    p->centering       = Reader_ReadInt32(reader);
#endif

    p->health          = Reader_ReadInt32(reader);

#if __JHEXEN__
    for(int i = 0; i < getPlayerHeader()->numArmorTypes; ++i)
    {
        p->armorPoints[i] = Reader_ReadInt32(reader);
    }
#else
    p->armorPoints = Reader_ReadInt32(reader);
    p->armorType = Reader_ReadInt32(reader);
#endif

#if __JDOOM64__ || __JHEXEN__
    P_InventoryEmpty(plrnum);
    for(int i = 0; i < getPlayerHeader()->numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(Reader_ReadInt32(reader));
        int count = Reader_ReadInt32(reader);

        for(int j = 0; j < count; ++j)
            P_InventoryGive(plrnum, type, true);
    }

    P_InventorySetReadyItem(plrnum, inventoryitemtype_t(Reader_ReadInt32(reader)));
# if __JHEXEN__
    Hu_InventorySelect(plrnum, P_InventoryReadyItem(plrnum));
    if(ver < 5)
    {
        Reader_ReadInt32(reader); // Current inventory item count?
    }
    if(ver < 6)
    /*p->inventorySlotNum =*/ Reader_ReadInt32(reader);
# endif
#endif

    for(int i = 0; i < getPlayerHeader()->numPowers; ++i)
    {
        p->powers[i] = Reader_ReadInt32(reader);
    }
    if(p->powers[PT_ALLMAP])
        ST_RevealAutomap(plrnum, true);

#if __JHEXEN__
    p->keys = Reader_ReadInt32(reader);
#else
    for(int i = 0; i < getPlayerHeader()->numKeys; ++i)
    {
        p->keys[i] = Reader_ReadInt32(reader);
    }
#endif

#if __JHEXEN__
    p->pieces = Reader_ReadInt32(reader);
#else
    p->backpack = Reader_ReadInt32(reader);
#endif

    for(int i = 0; i < getPlayerHeader()->numFrags; ++i)
    {
        p->frags[i] = Reader_ReadInt32(reader);
    }

    p->readyWeapon = weapontype_t(Reader_ReadInt32(reader));
#if __JHEXEN__
    if(ver < 5)
        p->pendingWeapon = WT_NOCHANGE;
    else
#endif
        p->pendingWeapon = weapontype_t(Reader_ReadInt32(reader));

    for(int i = 0; i < getPlayerHeader()->numWeapons; ++i)
    {
        p->weapons[i].owned = (Reader_ReadInt32(reader)? true : false);
    }

    for(int i = 0; i < getPlayerHeader()->numAmmoTypes; ++i)
    {
        p->ammo[i].owned = Reader_ReadInt32(reader);

#if !__JHEXEN__
        p->ammo[i].max = Reader_ReadInt32(reader);
#endif
    }

    p->attackDown  = Reader_ReadInt32(reader);
    p->useDown     = Reader_ReadInt32(reader);
    p->cheats      = Reader_ReadInt32(reader);
    p->refire      = Reader_ReadInt32(reader);
    p->killCount   = Reader_ReadInt32(reader);
    p->itemCount   = Reader_ReadInt32(reader);
    p->secretCount = Reader_ReadInt32(reader);

#if __JHEXEN__
    if(ver <= 1)
    {
        /*p->messageTics     =*/ Reader_ReadInt32(reader);
        /*p->ultimateMessage =*/ Reader_ReadInt32(reader);
        /*p->yellowMessage   =*/ Reader_ReadInt32(reader);
    }
#endif

    p->damageCount = Reader_ReadInt32(reader);
    p->bonusCount  = Reader_ReadInt32(reader);
#if __JHEXEN__
    p->poisonCount = Reader_ReadInt32(reader);
#endif

    dp->extraLight = Reader_ReadInt32(reader);
    dp->fixedColorMap = Reader_ReadInt32(reader);
    p->colorMap    = Reader_ReadInt32(reader);

    for(int i = 0; i < numPSprites; ++i)
    {
        pspdef_t *psp = &p->pSprites[i];

        psp->state = INT2PTR(state_t, Reader_ReadInt32(reader));
        psp->tics = Reader_ReadInt32(reader);
        psp->pos[VX] = FIX2FLT(Reader_ReadInt32(reader));
        psp->pos[VY] = FIX2FLT(Reader_ReadInt32(reader));
    }

#if !__JHEXEN__
    p->didSecret = Reader_ReadInt32(reader);

# if __JDOOM__ || __JDOOM64__
    if(ver == 2) // nolonger used in >= ver 3
        /*p->messageTics =*/ Reader_ReadInt32(reader);

    if(ver >= 2)
        p->flyHeight = Reader_ReadInt32(reader);

# elif __JHERETIC__
    if(ver < 3) // nolonger used in >= ver 3
        /*p->messageTics =*/ Reader_ReadInt32(reader);

    p->flyHeight = Reader_ReadInt32(reader);

    P_InventoryEmpty(plrnum);
    for(int i = 0; i < getPlayerHeader()->numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(Reader_ReadInt32(reader));
        int count = Reader_ReadInt32(reader);

        for(int j = 0; j < count; ++j)
            P_InventoryGive(plrnum, type, true);
    }

    P_InventorySetReadyItem(plrnum, (inventoryitemtype_t) Reader_ReadInt32(reader));
    Hu_InventorySelect(plrnum, P_InventoryReadyItem(plrnum));
    if(ver < 5)
    {
        Reader_ReadInt32(reader); // Current inventory item count?
    }
    if(ver < 6)
    /*p->inventorySlotNum =*/ Reader_ReadInt32(reader);

    p->chickenPeck = Reader_ReadInt32(reader);
# endif
#endif

#if __JHERETIC__ || __JHEXEN__
    p->morphTics = Reader_ReadInt32(reader);
#endif

    if(ver >= 2)
        p->airCounter = Reader_ReadInt32(reader);

#if __JHEXEN__
    p->jumpTics = Reader_ReadInt32(reader);
    p->worldTimer = Reader_ReadInt32(reader);
#elif __JHERETIC__
    p->flameCount = Reader_ReadInt32(reader);

    if(ver >= 2)
        p->class_ = playerclass_t(Reader_ReadByte(reader));
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

void SV_WriteMobj(thinker_t *th, MapStateWriter *msw)
{
    Writer *writer = msw->writer();

    mobj_t const *original = (mobj_t*) th;
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
    Writer_WriteByte(writer, MOBJ_SAVEVERSION);

#if !__JHEXEN__
    // A version 2 features: archive number and target.
    Writer_WriteInt16(writer, SV_ThingArchiveId((mobj_t*) original));
    Writer_WriteInt16(writer, SV_ThingArchiveId(mo->target));

# if __JDOOM__ || __JDOOM64__
    // Ver 5 features: Save tracer (fixes Archvile, Revenant bug)
    Writer_WriteInt16(writer, SV_ThingArchiveId(mo->tracer));
# endif
#endif

    Writer_WriteInt16(writer, SV_ThingArchiveId(mo->onMobj));

    // Info for drawing: position.
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VX]));
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VY]));
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VZ]));

    //More drawing info: to determine current sprite.
    Writer_WriteInt32(writer, mo->angle); // Orientation.
    Writer_WriteInt32(writer, mo->sprite); // Used to find patch_t and flip value.
    Writer_WriteInt32(writer, mo->frame);

#if !__JHEXEN__
    // The closest interval over all contacted Sectors.
    Writer_WriteInt32(writer, FLT2FIX(mo->floorZ));
    Writer_WriteInt32(writer, FLT2FIX(mo->ceilingZ));
#endif

    // For movement checking.
    Writer_WriteInt32(writer, FLT2FIX(mo->radius));
    Writer_WriteInt32(writer, FLT2FIX(mo->height));

    // Momentums, used to update position.
    Writer_WriteInt32(writer, FLT2FIX(mo->mom[MX]));
    Writer_WriteInt32(writer, FLT2FIX(mo->mom[MY]));
    Writer_WriteInt32(writer, FLT2FIX(mo->mom[MZ]));

    // If == VALIDCOUNT, already checked.
    Writer_WriteInt32(writer, mo->valid);

    Writer_WriteInt32(writer, mo->type);
    Writer_WriteInt32(writer, mo->tics); // State tic counter.
    Writer_WriteInt32(writer, PTR2INT(mo->state));

#if __JHEXEN__
    Writer_WriteInt32(writer, mo->damage);
#endif

    Writer_WriteInt32(writer, mo->flags);
#if __JHEXEN__
    Writer_WriteInt32(writer, mo->flags2);
    Writer_WriteInt32(writer, mo->flags3);

    if(mo->type == MT_KORAX)
        Writer_WriteInt32(writer, 0); // Searching index.
    else
        Writer_WriteInt32(writer, mo->special1);

    switch(mo->type)
    {
    case MT_LIGHTNING_FLOOR:
    case MT_LIGHTNING_ZAP:
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(mo->flags & MF_CORPSE)
            Writer_WriteInt32(writer, 0);
        else
            Writer_WriteInt32(writer, SV_ThingArchiveId(INT2PTR(mobj_t, mo->special2)));
        break;

    default:
        Writer_WriteInt32(writer, mo->special2);
        break;
    }
#endif
    Writer_WriteInt32(writer, mo->health);

    // Movement direction, movement generation (zig-zagging).
    Writer_WriteInt32(writer, mo->moveDir); // 0-7
    Writer_WriteInt32(writer, mo->moveCount); // When 0, select a new dir.

#if __JHEXEN__
    if(mo->flags & MF_CORPSE)
        Writer_WriteInt32(writer, 0);
    else
        Writer_WriteInt32(writer, (int) SV_ThingArchiveId(mo->target));
#endif

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    Writer_WriteInt32(writer, mo->reactionTime);

    // If >0, the target will be chased no matter what (even if shot).
    Writer_WriteInt32(writer, mo->threshold);

    // Additional info record for player avatars only (only valid if type
    // == MT_PLAYER).
    Writer_WriteInt32(writer, PTR2INT(mo->player));

    // Player number last looked for.
    Writer_WriteInt32(writer, mo->lastLook);

#if !__JHEXEN__
    // For nightmare/multiplayer respawn.
    Writer_WriteInt32(writer, FLT2FIX(mo->spawnSpot.origin[VX]));
    Writer_WriteInt32(writer, FLT2FIX(mo->spawnSpot.origin[VY]));
    Writer_WriteInt32(writer, FLT2FIX(mo->spawnSpot.origin[VZ]));
    Writer_WriteInt32(writer, mo->spawnSpot.angle);
    Writer_WriteInt32(writer, mo->spawnSpot.flags);

    Writer_WriteInt32(writer, mo->intFlags); // $dropoff_fix: internal flags.
    Writer_WriteInt32(writer, FLT2FIX(mo->dropOffZ)); // $dropoff_fix
    Writer_WriteInt32(writer, mo->gear); // Used in torque simulation.

    Writer_WriteInt32(writer, mo->damage);
    Writer_WriteInt32(writer, mo->flags2);
    Writer_WriteInt32(writer, mo->flags3);
# ifdef __JHERETIC__
    Writer_WriteInt32(writer, mo->special1);
    Writer_WriteInt32(writer, mo->special2);
    Writer_WriteInt32(writer, mo->special3);
# endif

    Writer_WriteByte(writer, mo->translucency);
    Writer_WriteByte(writer, (byte)(mo->visTarget +1));
#endif

    Writer_WriteInt32(writer, FLT2FIX(mo->floorClip));
#if __JHEXEN__
    Writer_WriteInt32(writer, SV_ThingArchiveId((mobj_t*) original));
    Writer_WriteInt32(writer, mo->tid);
    Writer_WriteInt32(writer, mo->special);
    Writer_Write(writer, mo->args, sizeof(mo->args));
    Writer_WriteByte(writer, mo->translucency);
    Writer_WriteByte(writer, (byte)(mo->visTarget +1));

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
            Writer_WriteInt32(writer, 0);
        else
            Writer_WriteInt32(writer, SV_ThingArchiveId(mo->tracer));
        break;

    default:
        DENG_ASSERT(mo->tracer == NULL); /// @todo Tracer won't be saved correctly?
        Writer_WriteInt32(writer, PTR2INT(mo->tracer));
        break;
    }

    Writer_WriteInt32(writer, PTR2INT(mo->lastEnemy));
#elif __JHERETIC__
    // Ver 7 features: generator
    Writer_WriteInt16(writer, SV_ThingArchiveId(mo->generator));
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
int SV_ReadMobj(thinker_t *th, MapStateReader *msr)
{
#define FF_FULLBRIGHT 0x8000 ///< Used to be a flag in thing->frame.
#define FF_FRAMEMASK  0x7fff

    Reader *reader = msr->reader();
    mobj_t *mo = (mobj_t *) th;

    int ver = Reader_ReadByte(reader);
#if !__JHEXEN__
    if(ver >= 2) // Version 2 has mobj archive numbers.
    {
        insertThingInArchive(mo, Reader_ReadInt16(reader));
    }
#endif

#if !__JHEXEN__
    mo->target = 0;
    if(ver >= 2)
    {
        mo->target = INT2PTR(mobj_t, Reader_ReadInt16(reader));
    }
#endif

#if __JDOOM__ || __JDOOM64__
    // Tracer for enemy attacks (updated after all mobjs are loaded).
    mo->tracer = 0;
    if(ver >= 5)
    {
        mo->tracer = INT2PTR(mobj_t, Reader_ReadInt16(reader));
    }
#endif

    // mobj this one is on top of (updated after all mobjs are loaded).
    mo->onMobj = 0;
#if __JHEXEN__
    if(ver >= 8)
#else
    if(ver >= 5)
#endif
    {
        mo->onMobj = INT2PTR(mobj_t, Reader_ReadInt16(reader));
    }

    // Info for drawing: position.
    mo->origin[VX] = FIX2FLT(Reader_ReadInt32(reader));
    mo->origin[VY] = FIX2FLT(Reader_ReadInt32(reader));
    mo->origin[VZ] = FIX2FLT(Reader_ReadInt32(reader));

    //More drawing info: to determine current sprite.
    mo->angle  = Reader_ReadInt32(reader); // orientation
    mo->sprite = Reader_ReadInt32(reader); // used to find patch_t and flip value

    mo->frame  = Reader_ReadInt32(reader); // might be ORed with FF_FULLBRIGHT
    if(mo->frame & FF_FULLBRIGHT)
        mo->frame &= FF_FRAMEMASK; // not used anymore.

#if __JHEXEN__
    if(ver < 6)
        Reader_ReadInt32(reader); // Used to be floorflat.
#else
    // The closest interval over all contacted Sectors.
    mo->floorZ   = FIX2FLT(Reader_ReadInt32(reader));
    mo->ceilingZ = FIX2FLT(Reader_ReadInt32(reader));
#endif

    // For movement checking.
    mo->radius  = FIX2FLT(Reader_ReadInt32(reader));
    mo->height  = FIX2FLT(Reader_ReadInt32(reader));

    // Momentums, used to update position.
    mo->mom[MX] = FIX2FLT(Reader_ReadInt32(reader));
    mo->mom[MY] = FIX2FLT(Reader_ReadInt32(reader));
    mo->mom[MZ] = FIX2FLT(Reader_ReadInt32(reader));

    // If == VALIDCOUNT, already checked.
    mo->valid = Reader_ReadInt32(reader);
    mo->type  = Reader_ReadInt32(reader);
#if __JHEXEN__
    if(ver < 7)
        /*mo->info = (mobjinfo_t *)*/ Reader_ReadInt32(reader);
#endif
    mo->info = &MOBJINFO[mo->type];

    if(mo->info->flags2 & MF2_FLOATBOB)
        mo->mom[MZ] = 0;

    if(mo->info->flags & MF_SOLID)
        mo->ddFlags |= DDMF_SOLID;
    if(mo->info->flags2 & MF2_DONTDRAW)
        mo->ddFlags |= DDMF_DONTDRAW;

    mo->tics   = Reader_ReadInt32(reader);   // state tic counter
    mo->state  = INT2PTR(state_t, Reader_ReadInt32(reader));

#if __JHEXEN__
    mo->damage = Reader_ReadInt32(reader);
#endif

    mo->flags  = Reader_ReadInt32(reader);

#if __JHEXEN__
    mo->flags2 = Reader_ReadInt32(reader);
    if(ver >= 5)
        mo->flags3 = Reader_ReadInt32(reader);
    mo->special1 = Reader_ReadInt32(reader);
    mo->special2 = Reader_ReadInt32(reader);
#endif

    mo->health = Reader_ReadInt32(reader);
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
    mo->moveDir = Reader_ReadInt32(reader);    // 0-7
    mo->moveCount = Reader_ReadInt32(reader);  // when 0, select a new dir

#if __JHEXEN__
    mo->target = INT2PTR(mobj_t, Reader_ReadInt32(reader));
#endif

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactionTime = Reader_ReadInt32(reader);

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold = Reader_ReadInt32(reader);

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player = INT2PTR(player_t, Reader_ReadInt32(reader));

    // Player number last looked for.
    mo->lastLook = Reader_ReadInt32(reader);

#if __JHEXEN__
    mo->floorClip = FIX2FLT(Reader_ReadInt32(reader));
    insertThingInArchive(mo, Reader_ReadInt32(reader));
    mo->tid = Reader_ReadInt32(reader);
#else
    // For nightmare respawn.
    if(ver >= 6)
    {
        mo->spawnSpot.origin[VX] = FIX2FLT(Reader_ReadInt32(reader));
        mo->spawnSpot.origin[VY] = FIX2FLT(Reader_ReadInt32(reader));
        mo->spawnSpot.origin[VZ] = FIX2FLT(Reader_ReadInt32(reader));
        mo->spawnSpot.angle = Reader_ReadInt32(reader);
        if(ver < 10)
        /* mo->spawnSpot.type = */ Reader_ReadInt32(reader);
        mo->spawnSpot.flags = Reader_ReadInt32(reader);
    }
    else
    {
        mo->spawnSpot.origin[VX] = (float) Reader_ReadInt16(reader);
        mo->spawnSpot.origin[VY] = (float) Reader_ReadInt16(reader);
        mo->spawnSpot.origin[VZ] = 0; // Initialize with "something".
        mo->spawnSpot.angle = (angle_t) (ANG45 * (Reader_ReadInt16(reader) / 45));
        /*mo->spawnSpot.type = (int)*/ Reader_ReadInt16(reader);
        mo->spawnSpot.flags = (int) Reader_ReadInt16(reader);
    }

# if __JDOOM__ || __JDOOM64__
    if(ver >= 3)
# elif __JHERETIC__
    if(ver >= 5)
# endif
    {
        mo->intFlags = Reader_ReadInt32(reader);   // killough $dropoff_fix: internal flags
        mo->dropOffZ = FIX2FLT(Reader_ReadInt32(reader));   // killough $dropoff_fix
        mo->gear = Reader_ReadInt32(reader);   // killough used in torque simulation
    }

# if __JDOOM__ || __JDOOM64__
    if(ver >= 6)
    {
        mo->damage = Reader_ReadInt32(reader);
        mo->flags2 = Reader_ReadInt32(reader);
    }// Else flags2 will be applied from the defs.
    else
        mo->damage = DDMAXINT; // Use the value set in mo->info->damage

# elif __JHERETIC__
    mo->damage = Reader_ReadInt32(reader);
    mo->flags2 = Reader_ReadInt32(reader);
# endif

    if(ver >= 7)
        mo->flags3 = Reader_ReadInt32(reader);
    // Else flags3 will be applied from the defs.
#endif

#if __JHEXEN__
    mo->special = Reader_ReadInt32(reader);
    Reader_Read(reader, mo->args, 1 * 5);
#elif __JHERETIC__
    mo->special1 = Reader_ReadInt32(reader);
    mo->special2 = Reader_ReadInt32(reader);
    if(ver >= 8)
        mo->special3 = Reader_ReadInt32(reader);
#endif

#if __JHEXEN__
    if(ver >= 2)
#else
    if(ver >= 4)
#endif
        mo->translucency = Reader_ReadByte(reader);

#if __JHEXEN__
    if(ver >= 3)
#else
    if(ver >= 5)
#endif
        mo->visTarget = (short) (Reader_ReadByte(reader)) -1;

#if __JHEXEN__
    if(ver >= 4)
        mo->tracer = INT2PTR(mobj_t, Reader_ReadInt32(reader));

    if(ver >= 4)
        mo->lastEnemy = INT2PTR(mobj_t, Reader_ReadInt32(reader));
#else
    if(ver >= 5)
        mo->floorClip = FIX2FLT(Reader_ReadInt32(reader));
#endif

#if __JHERETIC__
    if(ver >= 7)
        mo->generator = INT2PTR(mobj_t, Reader_ReadInt16(reader));
    else
        mo->generator = NULL;
#endif

    // Restore! (unmangle)
    RestoreMobj(mo, ver);

    return false;

#undef FF_FRAMEMASK
#undef FF_FULLBRIGHT
}

/**
 * Prepare and write the player header info.
 */
static void writePlayerHeader(Writer *writer)
{
    playerheader_t *ph = &playerHeader;

    SV_BeginSegment(ASEG_PLAYER_HEADER);
    Writer_WriteByte(writer, 2); // version byte

    ph->numPowers       = NUM_POWER_TYPES;
    ph->numKeys         = NUM_KEY_TYPES;
    ph->numFrags        = MAXPLAYERS;
    ph->numWeapons      = NUM_WEAPON_TYPES;
    ph->numAmmoTypes    = NUM_AMMO_TYPES;
    ph->numPSprites     = NUMPSPRITES;
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    ph->numInvItemTypes = NUM_INVENTORYITEM_TYPES;
#endif
#if __JHEXEN__
    ph->numArmorTypes   = NUMARMOR;
#endif

    Writer_WriteInt32(writer, ph->numPowers);
    Writer_WriteInt32(writer, ph->numKeys);
    Writer_WriteInt32(writer, ph->numFrags);
    Writer_WriteInt32(writer, ph->numWeapons);
    Writer_WriteInt32(writer, ph->numAmmoTypes);
    Writer_WriteInt32(writer, ph->numPSprites);
#if __JDOOM64__ || __JHERETIC__ || __JHEXEN__
    Writer_WriteInt32(writer, ph->numInvItemTypes);
#endif
#if __JHEXEN__
    Writer_WriteInt32(writer, ph->numArmorTypes);
#endif

    playerHeaderOK = true;
}

/**
 * Read player header info from the game state.
 */
static void readPlayerHeader(Reader *reader, int saveVersion)
{
#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(saveVersion >= 5)
#endif
    {
        SV_AssertSegment(ASEG_PLAYER_HEADER);
        int ver = Reader_ReadByte(reader);
#if !__JHERETIC__
        DENG_UNUSED(ver);
#endif

        playerHeader.numPowers      = Reader_ReadInt32(reader);
        playerHeader.numKeys        = Reader_ReadInt32(reader);
        playerHeader.numFrags       = Reader_ReadInt32(reader);
        playerHeader.numWeapons     = Reader_ReadInt32(reader);
        playerHeader.numAmmoTypes   = Reader_ReadInt32(reader);
        playerHeader.numPSprites    = Reader_ReadInt32(reader);
#if __JHERETIC__
        if(ver >= 2)
            playerHeader.numInvItemTypes = Reader_ReadInt32(reader);
        else
            playerHeader.numInvItemTypes = NUM_INVENTORYITEM_TYPES;
#endif
#if __JHEXEN__ || __JDOOM64__
        playerHeader.numInvItemTypes = Reader_ReadInt32(reader);
#endif
#if __JHEXEN__
        playerHeader.numArmorTypes  = Reader_ReadInt32(reader);
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

static void writePlayers(Writer *writer)
{
    SV_BeginSegment(ASEG_PLAYERS);
    {
#if __JHEXEN__
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            Writer_WriteByte(writer, players[i].plr->inGame);
        }
#endif

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame)
                continue;

            Writer_WriteInt32(writer, Net_GetPlayerID(i));
            SV_WritePlayer(i, writer);
        }
    }
    SV_EndSegment();
}

static void readPlayers(SaveInfo &info, dd_bool *infile, dd_bool *loaded,
    Reader *reader)
{
    DENG_ASSERT(infile != 0 && loaded != 0);

    // Setup the dummy.
    ddplayer_t dummyDDPlayer;
    player_t dummyPlayer;
    dummyPlayer.plr = &dummyDDPlayer;

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        loaded[i] = 0;
#if !__JHEXEN__
        infile[i] = info._players[i];
#endif
    }

    SV_AssertSegment(ASEG_PLAYERS);
    {
#if __JHEXEN__
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            infile[i] = Reader_ReadByte(reader);
        }
#endif

        // Load the players.
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            // By default a saved player translates to nothing.
            saveToRealPlayerNum[i] = -1;

            if(!infile[i]) continue;

            // The ID number will determine which player this actually is.
            int pid = Reader_ReadInt32(reader);
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
            SV_ReadPlayer(player, reader);
        }
    }
    SV_AssertSegment(ASEG_END);

#if __JHEXEN__
    DENG_UNUSED(info);
#endif
}

enum sectorclass_t
{
    sc_normal,
    sc_ploff, ///< plane offset
#if !__JHEXEN__
    sc_xg1,
#endif
    NUM_SECTORCLASSES
};

void SV_WriteSector(Sector *sec, MapStateWriter *msw)
{
    Writer *writer = msw->writer();

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
    Writer_WriteByte(writer, type);

    // Version.
    // 2: Surface colors.
    // 3: Surface flags.
    Writer_WriteByte(writer, 3); // write a version byte.

    Writer_WriteInt16(writer, floorheight);
    Writer_WriteInt16(writer, ceilingheight);
    Writer_WriteInt16(writer, msw->serialIdFor(floorMaterial));
    Writer_WriteInt16(writer, msw->serialIdFor(ceilingMaterial));
    Writer_WriteInt16(writer, floorFlags);
    Writer_WriteInt16(writer, ceilingFlags);
#if __JHEXEN__
    Writer_WriteInt16(writer, (short) lightlevel);
#else
    Writer_WriteByte(writer, lightlevel);
#endif

    float rgb[3];
    P_GetFloatpv(sec, DMU_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        Writer_WriteByte(writer, (byte)(255.f * rgb[i]));

    P_GetFloatpv(sec, DMU_FLOOR_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        Writer_WriteByte(writer, (byte)(255.f * rgb[i]));

    P_GetFloatpv(sec, DMU_CEILING_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        Writer_WriteByte(writer, (byte)(255.f * rgb[i]));

    Writer_WriteInt16(writer, xsec->special);
    Writer_WriteInt16(writer, xsec->tag);

#if __JHEXEN__
    Writer_WriteInt16(writer, xsec->seqType);
#endif

    if(type == sc_ploff
#if !__JHEXEN__
       || type == sc_xg1
#endif
       )
    {
        Writer_WriteFloat(writer, flooroffx);
        Writer_WriteFloat(writer, flooroffy);
        Writer_WriteFloat(writer, ceiloffx);
        Writer_WriteFloat(writer, ceiloffy);
    }

#if !__JHEXEN__
    if(xsec->xg) // Extended General?
    {
        SV_WriteXGSector(sec, writer);
    }
#endif
}

/**
 * Reads all versions of archived sectors.
 * Including the old Ver1.
 */
void SV_ReadSector(Sector *sec, MapStateReader *msr)
{
    Reader *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    int i, ver = 1;
    int type = 0;
    Material *floorMaterial = 0, *ceilingMaterial = 0;
    byte rgb[3], lightlevel;
    xsector_t *xsec = P_ToXSector(sec);
    int fh, ch;

    // A type byte?
#if __JHEXEN__
    if(mapVersion < 4)
        type = sc_ploff;
    else
#else
    if(mapVersion <= 1)
        type = sc_normal;
    else
#endif
        type = Reader_ReadByte(reader);

    // A version byte?
#if __JHEXEN__
    if(mapVersion > 2)
#else
    if(mapVersion > 4)
#endif
        ver = Reader_ReadByte(reader);

    fh = Reader_ReadInt16(reader);
    ch = Reader_ReadInt16(reader);

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
    if(mapVersion == 1)
    {
        // The flat numbers are absolute lump indices.
        Uri* uri = Uri_NewWithPath2("Flats:", RC_NULL);
        Uri_SetPath(uri, Str_Text(W_LumpName(Reader_ReadInt16(reader))));
        floorMaterial = (Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));

        Uri_SetPath(uri, Str_Text(W_LumpName(Reader_ReadInt16(reader))));
        ceilingMaterial = (Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(uri));
        Uri_Delete(uri);
    }
    else if(mapVersion >= 4)
#endif
    {
        // The flat numbers are actually archive numbers.
        floorMaterial   = msr->material(Reader_ReadInt16(reader), 0);
        ceilingMaterial = msr->material(Reader_ReadInt16(reader), 0);
    }

    P_SetPtrp(sec, DMU_FLOOR_MATERIAL,   floorMaterial);
    P_SetPtrp(sec, DMU_CEILING_MATERIAL, ceilingMaterial);

    if(ver >= 3)
    {
        P_SetIntp(sec, DMU_FLOOR_FLAGS,   Reader_ReadInt16(reader));
        P_SetIntp(sec, DMU_CEILING_FLAGS, Reader_ReadInt16(reader));
    }

#if __JHEXEN__
    lightlevel = (byte) Reader_ReadInt16(reader);
#else
    // In Ver1 the light level is a short
    if(mapVersion == 1)
    {
        lightlevel = (byte) Reader_ReadInt16(reader);
    }
    else
    {
        lightlevel = Reader_ReadByte(reader);
    }
#endif
    P_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) lightlevel / 255.f);

#if !__JHEXEN__
    if(mapVersion > 1)
#endif
    {
        Reader_Read(reader, rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_COLOR_RED + i, rgb[i] / 255.f);
    }

    // Ver 2 includes surface colours
    if(ver >= 2)
    {
        Reader_Read(reader, rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_FLOOR_COLOR_RED + i, rgb[i] / 255.f);

        Reader_Read(reader, rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_CEILING_COLOR_RED + i, rgb[i] / 255.f);
    }

    xsec->special = Reader_ReadInt16(reader);
    /*xsec->tag =*/ Reader_ReadInt16(reader);

#if __JHEXEN__
    xsec->seqType = seqtype_t(Reader_ReadInt16(reader));
#endif

    if(type == sc_ploff
#if !__JHEXEN__
       || type == sc_xg1
#endif
       )
    {
        P_SetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_X, Reader_ReadFloat(reader));
        P_SetFloatp(sec, DMU_FLOOR_MATERIAL_OFFSET_Y, Reader_ReadFloat(reader));
        P_SetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_X, Reader_ReadFloat(reader));
        P_SetFloatp(sec, DMU_CEILING_MATERIAL_OFFSET_Y, Reader_ReadFloat(reader));
    }

#if !__JHEXEN__
    if(type == sc_xg1)
        SV_ReadXGSector(sec, reader, mapVersion);
#endif

#if !__JHEXEN__
    if(mapVersion <= 1)
#endif
    {
        xsec->specialData = 0;
    }

    // We'll restore the sound targets latter on
    xsec->soundTarget = 0;
}

enum lineclass_t
{
    lc_normal,
#if !__JHEXEN__
    lc_xg1,
#endif
    NUM_LINECLASSES
};

void SV_WriteLine(Line *li, MapStateWriter *msw)
{
    Writer *writer = msw->writer();

    xline_t *xli = P_ToXLine(li);

    lineclass_t type;
#if !__JHEXEN__
    if(xli->xg)
        type =  lc_xg1;
    else
#endif
        type = lc_normal;
    Writer_WriteByte(writer, type);

    // Version.
    // 2: Per surface texture offsets.
    // 2: Surface colors.
    // 3: "Mapped by player" values.
    // 3: Surface flags.
    // 4: Engine-side line flags.
    Writer_WriteByte(writer, 4); // Write a version byte

    Writer_WriteInt16(writer, P_GetIntp(li, DMU_FLAGS));
    Writer_WriteInt16(writer, xli->flags);

    for(int i = 0; i < MAXPLAYERS; ++i)
        Writer_WriteByte(writer, xli->mapped[i]);

#if __JHEXEN__
    Writer_WriteByte(writer, xli->special);
    Writer_WriteByte(writer, xli->arg1);
    Writer_WriteByte(writer, xli->arg2);
    Writer_WriteByte(writer, xli->arg3);
    Writer_WriteByte(writer, xli->arg4);
    Writer_WriteByte(writer, xli->arg5);
#else
    Writer_WriteInt16(writer, xli->special);
    Writer_WriteInt16(writer, xli->tag);
#endif

    // For each side
    float rgba[4];
    for(int i = 0; i < 2; ++i)
    {
        Side *si = (Side *)P_GetPtrp(li, (i? DMU_BACK:DMU_FRONT));
        if(!si) continue;

        Writer_WriteInt16(writer, P_GetIntp(si, DMU_TOP_MATERIAL_OFFSET_X));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_TOP_MATERIAL_OFFSET_Y));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_MIDDLE_MATERIAL_OFFSET_X));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_MIDDLE_MATERIAL_OFFSET_Y));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_BOTTOM_MATERIAL_OFFSET_X));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_BOTTOM_MATERIAL_OFFSET_Y));

        Writer_WriteInt16(writer, P_GetIntp(si, DMU_TOP_FLAGS));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_MIDDLE_FLAGS));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_BOTTOM_FLAGS));

        Writer_WriteInt16(writer, msw->serialIdFor((Material *)P_GetPtrp(si, DMU_TOP_MATERIAL)));
        Writer_WriteInt16(writer, msw->serialIdFor((Material *)P_GetPtrp(si, DMU_BOTTOM_MATERIAL)));
        Writer_WriteInt16(writer, msw->serialIdFor((Material *)P_GetPtrp(si, DMU_MIDDLE_MATERIAL)));

        P_GetFloatpv(si, DMU_TOP_COLOR, rgba);
        for(int k = 0; k < 3; ++k)
            Writer_WriteByte(writer, (byte)(255 * rgba[k]));

        P_GetFloatpv(si, DMU_BOTTOM_COLOR, rgba);
        for(int k = 0; k < 3; ++k)
            Writer_WriteByte(writer, (byte)(255 * rgba[k]));

        P_GetFloatpv(si, DMU_MIDDLE_COLOR, rgba);
        for(int k = 0; k < 4; ++k)
            Writer_WriteByte(writer, (byte)(255 * rgba[k]));

        Writer_WriteInt32(writer, P_GetIntp(si, DMU_MIDDLE_BLENDMODE));
        Writer_WriteInt16(writer, P_GetIntp(si, DMU_FLAGS));
    }

#if !__JHEXEN__
    // Extended General?
    if(xli->xg)
    {
        SV_WriteXGLine(li, writer);
    }
#endif
}

/**
 * Reads all versions of archived lines.
 * Including the old Ver1.
 */
void SV_ReadLine(Line *li, MapStateReader *msr)
{
    Reader *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    Material *topMaterial = 0, *bottomMaterial = 0, *middleMaterial = 0;
    xline_t *xli = P_ToXLine(li);

    // A type byte?
    lineclass_t type;

#if __JHEXEN__
    if(mapVersion < 4)
#else
    if(mapVersion < 2)
#endif
        type = lc_normal;
    else
        type = lineclass_t(Reader_ReadByte(reader));

#ifdef __JHEXEN__
    DENG_UNUSED(type);
#endif

    // A version byte?
    int ver;
#if __JHEXEN__
    if(mapVersion < 3)
#else
    if(mapVersion < 5)
#endif
        ver = 1;
    else
        ver = (int) Reader_ReadByte(reader);

    if(ver >= 4)
        P_SetIntp(li, DMU_FLAGS, Reader_ReadInt16(reader));

    int flags = Reader_ReadInt16(reader);

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
            xli->mapped[i] = Reader_ReadByte(reader);
    }

#if __JHEXEN__
    xli->special = Reader_ReadByte(reader);
    xli->arg1 = Reader_ReadByte(reader);
    xli->arg2 = Reader_ReadByte(reader);
    xli->arg3 = Reader_ReadByte(reader);
    xli->arg4 = Reader_ReadByte(reader);
    xli->arg5 = Reader_ReadByte(reader);
#else
    xli->special = Reader_ReadInt16(reader);
    /*xli->tag =*/ Reader_ReadInt16(reader);
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

            offset[VX] = (float) Reader_ReadInt16(reader);
            offset[VY] = (float) Reader_ReadInt16(reader);
            P_SetFloatpv(si, DMU_TOP_MATERIAL_OFFSET_XY, offset);

            offset[VX] = (float) Reader_ReadInt16(reader);
            offset[VY] = (float) Reader_ReadInt16(reader);
            P_SetFloatpv(si, DMU_MIDDLE_MATERIAL_OFFSET_XY, offset);

            offset[VX] = (float) Reader_ReadInt16(reader);
            offset[VY] = (float) Reader_ReadInt16(reader);
            P_SetFloatpv(si, DMU_BOTTOM_MATERIAL_OFFSET_XY, offset);
        }
        else
        {
            float offset[2];

            offset[VX] = (float) Reader_ReadInt16(reader);
            offset[VY] = (float) Reader_ReadInt16(reader);

            P_SetFloatpv(si, DMU_TOP_MATERIAL_OFFSET_XY,    offset);
            P_SetFloatpv(si, DMU_MIDDLE_MATERIAL_OFFSET_XY, offset);
            P_SetFloatpv(si, DMU_BOTTOM_MATERIAL_OFFSET_XY, offset);
        }

        if(ver >= 3)
        {
            P_SetIntp(si, DMU_TOP_FLAGS,    Reader_ReadInt16(reader));
            P_SetIntp(si, DMU_MIDDLE_FLAGS, Reader_ReadInt16(reader));
            P_SetIntp(si, DMU_BOTTOM_FLAGS, Reader_ReadInt16(reader));
        }

#if !__JHEXEN__
        if(mapVersion >= 4)
#endif
        {
            topMaterial    = msr->material(Reader_ReadInt16(reader), 1);
            bottomMaterial = msr->material(Reader_ReadInt16(reader), 1);
            middleMaterial = msr->material(Reader_ReadInt16(reader), 1);
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
                rgba[k] = (float) Reader_ReadByte(reader) / 255.f;
            rgba[3] = 1;
            P_SetFloatpv(si, DMU_TOP_COLOR, rgba);

            for(int k = 0; k < 3; ++k)
                rgba[k] = (float) Reader_ReadByte(reader) / 255.f;
            rgba[3] = 1;
            P_SetFloatpv(si, DMU_BOTTOM_COLOR, rgba);

            for(int k = 0; k < 4; ++k)
                rgba[k] = (float) Reader_ReadByte(reader) / 255.f;
            P_SetFloatpv(si, DMU_MIDDLE_COLOR, rgba);

            P_SetIntp(si, DMU_MIDDLE_BLENDMODE, Reader_ReadInt32(reader));

            flags = Reader_ReadInt16(reader);
#if __JHEXEN__
            if(mapVersion < 12)
#else
            if(mapVersion < 12)
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
        SV_ReadXGLine(li, reader, mapVersion);
#endif
}

#if __JHEXEN__
void SV_WritePolyObj(Polyobj *po, MapStateWriter *msw)
{
    Writer *writer = msw->writer();

    Writer_WriteByte(writer, 1); // write a version byte.

    Writer_WriteInt32(writer, po->tag);
    Writer_WriteInt32(writer, po->angle);
    Writer_WriteInt32(writer, FLT2FIX(po->origin[VX]));
    Writer_WriteInt32(writer, FLT2FIX(po->origin[VY]));
}

int SV_ReadPolyObj(MapStateReader *msr)
{
    Reader *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    int ver = (mapVersion >= 3)? Reader_ReadByte(reader) : 0;
    DENG_UNUSED(ver);

    Polyobj *po = Polyobj_ByTag(Reader_ReadInt32(reader));
    DENG_ASSERT(po != 0);

    angle_t angle = angle_t(Reader_ReadInt32(reader));
    Polyobj_Rotate(po, angle);
    po->destAngle = angle;

    float const origX = FIX2FLT(Reader_ReadInt32(reader));
    float const origY = FIX2FLT(Reader_ReadInt32(reader));
    Polyobj_MoveXY(po, origX - po->origin[VX], origY - po->origin[VY]);

    /// @todo What about speed? It isn't saved at all?

    return true;
}
#endif

#if __JHEXEN__
void SV_WriteMovePoly(polyevent_t const *th, MapStateWriter *msw)
{
    Writer *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteInt32(writer, th->polyobj);
    Writer_WriteInt32(writer, th->intSpeed);
    Writer_WriteInt32(writer, th->dist);
    Writer_WriteInt32(writer, th->fangle);
    Writer_WriteInt32(writer, FLT2FIX(th->speed[VX]));
    Writer_WriteInt32(writer, FLT2FIX(th->speed[VY]));
}

int SV_ReadMovePoly(polyevent_t *th, MapStateReader *msr)
{
    Reader *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    if(mapVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        // Start of used data members.
        th->polyobj     = Reader_ReadInt32(reader);
        th->intSpeed    = Reader_ReadInt32(reader);
        th->dist        = Reader_ReadInt32(reader);
        th->fangle      = Reader_ReadInt32(reader);
        th->speed[VX]   = FIX2FLT(Reader_ReadInt32(reader));
        th->speed[VY]   = FIX2FLT(Reader_ReadInt32(reader));
    }
    else
    {
        // Its in the old pre V4 format which serialized polyevent_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        th->polyobj     = Reader_ReadInt32(reader);
        th->intSpeed    = Reader_ReadInt32(reader);
        th->dist        = Reader_ReadInt32(reader);
        th->fangle      = Reader_ReadInt32(reader);
        th->speed[VX]   = FIX2FLT(Reader_ReadInt32(reader));
        th->speed[VY]   = FIX2FLT(Reader_ReadInt32(reader));
    }

    th->thinker.function = T_MovePoly;

    return true; // Add this thinker.
}
#endif // __JHEXEN__

void SV_Initialize()
{
    static bool firstInit = true;

    SV_InitIO();

    inited = true;
    if(firstInit)
    {
        firstInit         = false;
        playerHeaderOK    = false;
        thingArchive      = 0;
        thingArchiveSize  = 0;
#if __JHEXEN__
        targetPlayerAddrs = 0;
        saveBuffer        = 0;
#endif
    }

    // Reset last-used and quick-save slot tracking.
    Con_SetInteger2("game-save-last-slot", -1, SVF_WRITE_OVERRIDE);
    Con_SetInteger("game-save-quick-slot", -1);

    // (Re)Initialize the saved game paths, possibly creating them if they do not exist.
    SV_ConfigureSavePaths();
}

void SV_Shutdown()
{
    if(!inited) return;

    SV_ShutdownIO();
    saveSlots.clearSaveInfo();

    inited = false;
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

#if __JHEXEN__
static void readMapState(Reader *reader, int saveVersion, Str const *path)
#else
static void readMapState(Reader *reader, int saveVersion)
#endif
{
#if __JHEXEN__
    DENG_ASSERT(path != 0);

    App_Log(DE2_DEV_MAP_MSG, "readMapState: Opening file \"%s\"\n", Str_Text(path));

    // Load the file
    size_t bufferSize = M_ReadFile(Str_Text(path), (char **)&saveBuffer);
    if(!bufferSize)
    {
        App_Log(DE2_RES_ERROR, "readMapState: Failed opening \"%s\" for reading", Str_Text(path));
        return;
    }

    SV_HxSavePtr()->b = saveBuffer;
    SV_HxSetSaveEndPtr(saveBuffer + bufferSize);
#endif

    MapStateReader(saveVersion).read(reader);

#if __JHEXEN__
    clearThingArchive();
    Z_Free(saveBuffer);
#endif
}

static void loadNativeState(Str const *path, SaveInfo *info)
{
    DENG_ASSERT(path != 0 && info != 0);

    playerHeaderOK = false; // Uninitialized.

    if(!openGameSaveFile(path, false))
    {
        throw de::Error("SV_LoadState", "Failed opening \"" + de::String(Str_Text(path)) + "\" for read");
    }

    Reader *reader = SV_NewReader();

    // Read the header again.
    /// @todo Seek past the header straight to the game state.
    {
        SaveInfo *tmp = new SaveInfo;
        SV_SaveInfo_Read(tmp, reader);
        delete tmp;
    }

    curInfo = info;

#if __JHEXEN__
    if(info->version() >= 7)
    {
        SV_AssertSegment(ASEG_WORLDSCRIPTDATA);
    }
    Game_ACScriptInterpreter().readWorldScriptData(reader, info->version());
#endif

    /*
     * Load the map and configure some game settings.
     */
    briefDisabled = true;
    G_NewGame(info->mapUri(), 0/*not saved??*/, &info->gameRules());

#if !__JHEXEN__
    mapTime = info->mapTime();
#endif

    G_SetGameAction(GA_NONE); /// @todo Necessary?

#if !__JHEXEN__
    SV_InitThingArchiveForLoad(info->version() >= 5? Reader_ReadInt32(reader) : 1024 /* num elements */);
#endif

    readPlayerHeader(reader, info->version());

    // Read the player structures
    // We don't have the right to say which players are in the game. The
    // players that already are will continue to be. If the data for a given
    // player is not in the savegame file, he will be notified. The data for
    // players who were saved but are not currently in the game will be
    // discarded.
    dd_bool loaded[MAXPLAYERS], infile[MAXPLAYERS];
    readPlayers(*info, infile, loaded, reader);

#if __JHEXEN__
    Z_Free(saveBuffer);
#endif

    // Load the current map state.
#if __JHEXEN__
    readMapState(reader, info->version(), saveSlots.composeSavePathForSlot(BASE_SLOT, gameMap+1));
#else
    readMapState(reader, info->version());
#endif

#if !__JHEXEN__
    SV_ReadConsistencyBytes();
    SV_CloseFile();
#endif

    /*
     * Cleanup:
     */
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
    NetSv_LoadGame(info->sessionId());
#endif

    Reader_Delete(reader);
}

static void loadGameState(Str const *path, SaveInfo &saveInfo)
{
    if(recognizeNativeState(path, &saveInfo))
    {
        loadNativeState(path, &saveInfo);
        return;
    }

    // Perhaps an original game state?
#if __JDOOM__
    if(SV_RecognizeState_Dm_v19(path, &saveInfo))
    {
        int errorCode = SV_LoadState_Dm_v19(path, &saveInfo);
        if(errorCode != 0)
        {
            throw de::Error("SV_LoadState_Dm_v19", "Error " + de::String::number(errorCode));
        }
        return;
    }
#endif
#if __JHERETIC__
    if(SV_RecognizeState_Hr_v13(path, &saveInfo))
    {
        int errorCode = SV_LoadState_Hr_v13(path, &saveInfo);
        if(errorCode != 0)
        {
            throw de::Error("SV_LoadState_Hr_v13", "Error " + de::String::number(errorCode));
        }
        return;
    }
#endif

    /// @throw Error The savegame was not recognized.
    throw de::Error("loadGameState", "Unrecognized savegame format");
}

dd_bool SV_LoadGame(int slot)
{
    DENG_ASSERT(inited);

#if __JHEXEN__
    int const logicalSlot = BASE_SLOT;
#else
    int const logicalSlot = slot;
#endif

    if(!saveSlots.isValidSlot(slot))
        return false;

    AutoStr *path = saveSlots.composeSavePathForSlot(slot);
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
        saveSlots.copySlot(slot, BASE_SLOT);
    }
#endif

    try
    {
        SaveInfo &saveInfo = saveSlots.saveInfo(logicalSlot);

        loadGameState(path, saveInfo);

        // Material scrollers must be re-spawned for older savegame versions.
        /// @todo Implement SaveInfo format type identifiers.
        if((saveInfo.magic() != (IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC)) ||
           saveInfo.version() <= 10)
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

        // Make note of the last used save slot.
        Con_SetInteger2("game-save-last-slot", slot, SVF_WRITE_OVERRIDE);

        return true;
    }
    catch(de::Error const &er)
    {
        App_Log(DE2_RES_WARNING, "Error loading save slot #%i:\n%s", slot, er.asText());
    }

    return false;
}

void SV_SaveGameClient(uint sessionId)
{
#if !__JHEXEN__ // unsupported in libhexen
    DENG_ASSERT(inited);

    player_t *pl = &players[CONSOLEPLAYER];
    mobj_t *mo   = pl->plr->mo;

    if(!IS_CLIENT || !mo)
        return;

    playerHeaderOK = false; // Uninitialized.

    AutoStr *gameSavePath = composeGameSavePathForClientSessionId(sessionId);
    if(!SV_OpenFile(gameSavePath, "wp"))
    {
        App_Log(DE2_RES_WARNING, "SV_SaveGameClient: Failed opening \"%s\" for writing", Str_Text(gameSavePath));
        return;
    }

    // Prepare new save info.
    SaveInfo *info = SaveInfo::newWithCurrentSessionMetadata(AutoStr_New());
    info->setSessionId(sessionId);

    Writer *writer = SV_NewWriter();
    info->write(writer);

    // Some important information.
    // Our position and look angles.
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VX]));
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VY]));
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VZ]));
    Writer_WriteInt32(writer, FLT2FIX(mo->floorZ));
    Writer_WriteInt32(writer, FLT2FIX(mo->ceilingZ));
    Writer_WriteInt32(writer, mo->angle); /* $unifiedangles */
    Writer_WriteFloat(writer, pl->plr->lookDir); /* $unifiedangles */

    writePlayerHeader(writer);
    SV_WritePlayer(CONSOLEPLAYER, writer);

    MapStateWriter(thingArchiveExcludePlayers).write(writer);
    /// @todo No consistency bytes in client saves?

    SV_CloseFile();
    Writer_Delete(writer);
    delete info;
#else
    DENG_UNUSED(sessionId);
#endif
}

void SV_LoadGameClient(uint sessionId)
{
#if !__JHEXEN__ // unsupported in libhexen
    DENG_ASSERT(inited);

    player_t *cpl = players + CONSOLEPLAYER;
    mobj_t *mo    = cpl->plr->mo;

    if(!IS_CLIENT || !mo)
        return;

    playerHeaderOK = false; // Uninitialized.

    AutoStr *gameSavePath = composeGameSavePathForClientSessionId(sessionId);
    if(!SV_OpenFile(gameSavePath, "rp"))
    {
        App_Log(DE2_RES_WARNING, "SV_LoadGameClient: Failed opening \"%s\" for reading", Str_Text(gameSavePath));
        return;
    }

    SaveInfo *saveInfo = new SaveInfo;
    Reader *reader = SV_NewReader();
    SV_SaveInfo_Read(saveInfo, reader);

    curInfo = saveInfo;

    if(saveInfo->magic() != MY_CLIENT_SAVE_MAGIC)
    {
        Reader_Delete(reader);
        delete saveInfo;
        SV_CloseFile();
        App_Log(DE2_RES_ERROR, "Client save file format not recognized");
        return;
    }

    // Do we need to change the map?
    if(!Uri_Equality(gameMapUri, saveInfo->mapUri()))
    {
        G_NewGame(saveInfo->mapUri(), 0/*default*/, &saveInfo->gameRules());
        /// @todo Necessary?
        G_SetGameAction(GA_NONE);
    }
    else
    {
        /// @todo Necessary?
        gameRules = saveInfo->gameRules();
    }
    mapTime = saveInfo->mapTime();

    P_MobjUnlink(mo);
    mo->origin[VX] = FIX2FLT(Reader_ReadInt32(reader));
    mo->origin[VY] = FIX2FLT(Reader_ReadInt32(reader));
    mo->origin[VZ] = FIX2FLT(Reader_ReadInt32(reader));
    P_MobjLink(mo);
    mo->floorZ        = FIX2FLT(Reader_ReadInt32(reader));
    mo->ceilingZ      = FIX2FLT(Reader_ReadInt32(reader));
    mo->angle         = Reader_ReadInt32(reader); /* $unifiedangles */
    cpl->plr->lookDir = Reader_ReadFloat(reader); /* $unifiedangles */

    readPlayerHeader(reader, saveInfo->version());
    SV_ReadPlayer(cpl, reader);

    MapStateReader(saveInfo->version()).read(reader);

    SV_CloseFile();
    Reader_Delete(reader);
    delete saveInfo;
#else
    DENG_UNUSED(sessionId);
#endif
}

static int saveStateWorker(Str const *path, SaveInfo *saveInfo)
{
    App_Log(DE2_LOG_VERBOSE, "saveStateWorker: Attempting save game to \"%s\"", Str_Text(path));

    // In networked games the server tells the clients to save their games.
#if !__JHEXEN__
    NetSv_SaveGame(saveInfo->sessionId());
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
    saveInfo->write(writer);

#if __JHEXEN__
    SV_BeginSegment(ASEG_WORLDSCRIPTDATA);
    Game_ACScriptInterpreter().writeWorldScriptData(writer);
#endif

    // Set the mobj archive numbers.
    initThingArchiveForSave();

#if !__JHEXEN__
    Writer_WriteInt32(writer, thingArchiveSize);
#endif

    writePlayerHeader(writer);
    writePlayers(writer);

#if __JHEXEN__
    // Close the game session file (maps are saved into a seperate file).
    SV_CloseFile();
#endif

    /*
     * Save the map.
     */
#if __JHEXEN__
    // ...map state is actually written to a separate file.
    SV_OpenFile(saveSlots.composeSavePathForSlot(BASE_SLOT, gameMap + 1), "wp");
#endif

    MapStateWriter(thingArchiveExcludePlayers).write(writer);

    SV_WriteConsistencyBytes(); // To be absolutely sure...
    SV_CloseFile();

#if !__JHEXEN___
    clearThingArchive();
#endif

    Writer_Delete(writer);

    return SV_OK;
}

dd_bool SV_SaveGame(int slot, char const *description)
{
    DENG_ASSERT(inited);

#if __JHEXEN__
    int const logicalSlot = BASE_SLOT;
#else
    int const logicalSlot = slot;
#endif

    if(!saveSlots.isValidSlot(slot))
    {
        DENG_ASSERT(!"Invalid slot specified");
        return false;
    }
    if(!description || !description[0])
    {
        DENG_ASSERT(!"Empty description specified for slot");
        return false;
    }

    AutoStr *path = saveSlots.composeSavePathForSlot(logicalSlot);
    if(Str_IsEmpty(path))
    {
        App_Log(DE2_RES_WARNING, "Cannot save game: path \"%s\" is unreachable", SV_SavePath());
        return false;
    }

    SaveInfo *info = SaveInfo::newWithCurrentSessionMetadata(AutoStr_FromTextStd(description));

    int saveError = saveStateWorker(path, info);
    if(!saveError)
    {
        // Swap the save info.
        saveSlots.replaceSaveInfo(logicalSlot, info);

#if __JHEXEN__
        // Copy base slot to destination slot.
        saveSlots.copySlot(logicalSlot, slot);
#endif

        // The "last" save slot is now this.
        Con_SetInteger2("game-save-last-slot", slot, SVF_WRITE_OVERRIDE);
    }
    else
    {
        // We no longer need the info.
        delete info;

        if(saveError == SV_INVALIDFILENAME)
        {
            App_Log(DE2_RES_ERROR, "Failed opening \"%s\" for writing", Str_Text(path));
        }
    }

    return !saveError;
}

#if __JHEXEN__
void SV_HxSaveHubMap()
{
    playerHeaderOK = false; // Uninitialized.

    SV_OpenFile(saveSlots.composeSavePathForSlot(BASE_SLOT, gameMap+1), "wp");

    // Set the mobj archive numbers
    initThingArchiveForSave(true /*exclude players*/);

    Writer *writer = SV_NewWriter();

    MapStateWriter(thingArchiveExcludePlayers).write(writer);

    // Close the output file
    SV_CloseFile();

    Writer_Delete(writer);
}

void SV_HxLoadHubMap()
{
    /// @todo fixme: do not assume this pointer is still valid!
    DENG_ASSERT(curInfo != 0);
    SaveInfo const *info = curInfo;

    // Only readMap() uses targetPlayerAddrs, so it's NULLed here for the
    // following check (player mobj redirection).
    targetPlayerAddrs = 0;

    playerHeaderOK = false; // Uninitialized.

    Reader *reader = SV_NewReader();

    // Been here before, load the previous map state.
    readMapState(reader, info->version(), saveSlots.composeSavePathForSlot(BASE_SLOT, gameMap+1));

    Reader_Delete(reader);
}

void SV_HxBackupPlayersInHub(playerbackup_t playerBackup[MAXPLAYERS])
{
    DENG_ASSERT(playerBackup);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        playerbackup_t *pb = playerBackup + i;
        player_t *plr      = players + i;

        std::memcpy(&pb->player, plr, sizeof(player_t));

        // Make a copy of the inventory states also.
        for(int k = 0; k < NUM_INVENTORYITEM_TYPES; ++k)
        {
            pb->numInventoryItems[k] = P_InventoryCount(i, inventoryitemtype_t(k));
        }
        pb->readyItem = P_InventoryReadyItem(i);
    }
}

void SV_HxRestorePlayersInHub(playerbackup_t playerBackup[MAXPLAYERS],
    uint entryPoint)
{
    DENG_ASSERT(playerBackup != 0);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        playerbackup_t *pb = playerBackup + i;
        player_t *plr      = players + i;
        ddplayer_t *ddplr  = plr->plr;

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
        plr->attacker = 0;
        plr->poisoner = 0;

        int oldKeys = 0, oldPieces = 0;
        dd_bool oldWeaponOwned[NUM_WEAPON_TYPES];
        if(IS_NETGAME || gameRules.deathmatch)
        {
            // In a network game, force all players to be alive
            if(plr->playerState == PST_DEAD)
            {
                plr->playerState = PST_REBORN;
            }

            if(!gameRules.deathmatch)
            {
                // Cooperative net-play; retain keys and weapons.
                oldKeys   = plr->keys;
                oldPieces = plr->pieces;

                for(int k = 0; k < NUM_WEAPON_TYPES; ++k)
                {
                    oldWeaponOwned[k] = plr->weapons[k].owned;
                }
            }
        }

        dd_bool wasReborn = (plr->playerState == PST_REBORN);

        if(gameRules.deathmatch)
        {
            de::zap(plr->frags);
            ddplr->mo = 0;
            G_DeathMatchSpawnPlayer(i);
        }
        else
        {
            if(playerstart_t const *start = P_GetPlayerStart(entryPoint, i, false))
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

        if(wasReborn && IS_NETGAME && !gameRules.deathmatch)
        {
            // Restore keys and weapons when reborn in co-op.
            plr->keys   = oldKeys;
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

    mobj_t *targetPlayerMobj = 0;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr     = players + i;
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
