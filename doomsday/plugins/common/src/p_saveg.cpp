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
#include "g_common.h"
#include "p_actor.h"
#include "p_map.h"          // P_TelefragMobjsTouchingPlayers
#include "p_mapsetup.h"     // P_SpawnAllMaterialOriginScrollers
#include "p_inventory.h"
#include "p_tick.h"         // mapTime
#include "p_saveio.h"
#include "polyobjs.h"
#include "mapstatereader.h"
#include "mapstatewriter.h"
#include "r_common.h"       // R_UpdateConsoleView
#include <de/String>
#include <de/memory.h>
#include <lzss.h>
#include <cstdio>
#include <cstring>

static bool inited = false;

static SaveSlots sslots(NUMSAVESLOTS);
SaveSlots *saveSlots = &sslots;

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

int saveToRealPlayerNum[MAXPLAYERS];
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
    if(!F_MakePath(SV_ClientSavePath()))
    {
        return path; // return zero-length string.
    }

    // Compose the full game-save path and filename.
    Str_Appendf(path, "%s" CLIENTSAVEGAMENAME "%08X." SAVEGAMEEXTENSION, SV_ClientSavePath(), sessionId);
    F_TranslatePath(path, path);
    return path;
}
#endif

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
static void openMapSaveFile(Str const *path)
{
    DENG_ASSERT(path != 0);

    App_Log(DE2_DEV_RES_MSG, "openMapSaveFile: Opening \"%s\"", Str_Text(path));

    // Load the file
    size_t bufferSize = M_ReadFile(Str_Text(path), (char **)&saveBuffer);
    if(!bufferSize)
    {
        throw de::Error("openMapSaveFile", "Failed opening \"" + de::String(Str_Text(path)) + "\" for read");
    }

    SV_HxSavePtr()->b = saveBuffer;
    SV_HxSetSaveEndPtr(saveBuffer + bufferSize);
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
    if(!SV_OpenFile(path, "rp")) return false;
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

dd_bool SV_RecognizeGameState(Str const *path, SaveInfo *info)
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
    AutoStr *path = saveSlots->composeSavePathForSlot(slot, (int)map + 1);
    if(path && !Str_IsEmpty(path))
    {
        return SV_ExistingFile(path);
    }
    return false;
}
#endif

void SV_InitThingArchiveForLoad(uint size)
{
    thingArchiveSize = size;
    thingArchive     = reinterpret_cast<mobj_t **>(M_Calloc(thingArchiveSize * sizeof(*thingArchive)));
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
    countmobjthinkerstoarchive_params_t parm; de::zap(parm);
    parm.count          = 0;
    parm.excludePlayers = excludePlayers;
    Thinker_Iterate((thinkfunc_t) P_MobjThinker, countMobjThinkersToArchive, &parm);

    thingArchiveSize           = parm.count;
    thingArchive               = reinterpret_cast<mobj_t **>(M_Calloc(thingArchiveSize * sizeof(*thingArchive)));
    thingArchiveExcludePlayers = excludePlayers;
}

void SV_InsertThingInArchive(mobj_t const *mo, ThingSerialId thingId)
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
    M_Free(thingArchive); thingArchive = 0;
    thingArchiveSize = 0;
}

ThingSerialId SV_ThingArchiveId(mobj_t const *mo)
{
    DENG_ASSERT(inited);
    DENG_ASSERT(thingArchive != 0);

    if(!mo) return 0;

    // We only archive mobj thinkers.
    if(((thinker_t *) mo)->function != (thinkfunc_t) P_MobjThinker)
    {
        return 0;
    }

#if __JHEXEN__
    if(mo->player && thingArchiveExcludePlayers)
    {
        return TargetPlayerId;
    }
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
        {
            return i + 1;
        }
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

playerheader_t *SV_GetPlayerHeader()
{
    DENG_ASSERT(playerHeaderOK);
    return &playerHeader;
}

#if !__JDOOM64__
void SV_TranslateLegacyMobjFlags(mobj_t *mo, int ver)
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
        playerHeader.numPowers       = 9;
        playerHeader.numKeys         = 11;
        playerHeader.numFrags        = 8;
        playerHeader.numWeapons      = 4;
        playerHeader.numAmmoTypes    = 2;
        playerHeader.numPSprites     = 2;
        playerHeader.numInvItemTypes = 33;
        playerHeader.numArmorTypes   = 4;
#elif __JDOOM__ || __JDOOM64__
        playerHeader.numPowers       = 6;
        playerHeader.numKeys         = 6;
        playerHeader.numFrags        = 4; // Why was this only 4?
        playerHeader.numWeapons      = 9;
        playerHeader.numAmmoTypes    = 4;
        playerHeader.numPSprites     = 2;
# if __JDOOM64__
        playerHeader.numInvItemTypes = 3;
# endif
#elif __JHERETIC__
        playerHeader.numPowers       = 9;
        playerHeader.numKeys         = 3;
        playerHeader.numFrags        = 4; // ?
        playerHeader.numWeapons      = 8;
        playerHeader.numInvItemTypes = 14;
        playerHeader.numAmmoTypes    = 6;
        playerHeader.numPSprites     = 2;
#endif
    }
    playerHeaderOK = true;
}

class GameStateWriter
{
public:
    /// An error occurred attempting to open the output file. @ingroup errors
    DENG2_ERROR(FileAccessError);

public:
    GameStateWriter();

    void write(SaveInfo *saveInfo, Str const *path);

private:
    DENG2_PRIVATE(d)
};

DENG2_PIMPL_NOREF(GameStateWriter)
{
    SaveInfo *saveInfo; ///< Info for the save state to be written.
    Writer *writer;

    Instance(/*Public *i*/)
        : /*Base(i)
        ,*/ saveInfo(0)
        , writer(0)
    {}

    void writeSessionHeader()
    {
        saveInfo->write(writer);
    }

    void writeWorldACScriptData()
    {
#if __JHEXEN__
        SV_BeginSegment(ASEG_WORLDSCRIPTDATA);
        Game_ACScriptInterpreter().writeWorldScriptData(writer);
#endif
    }

    void writeMap()
    {
#if __JHEXEN__ // The map state is actually written to a separate file.
        // Close the game state file.
        SV_CloseFile();

        // Open the map state file.
        SV_OpenFile(saveSlots->composeSavePathForSlot(BASE_SLOT, gameMap + 1), "wp");
#endif

        MapStateWriter(thingArchiveExcludePlayers).write(writer);

        SV_WriteConsistencyBytes(); // To be absolutely sure...
        SV_CloseFile();

#if !__JHEXEN___
        clearThingArchive();
#endif
    }

    void writePlayers()
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
                player_t *plr = players + i;
                if(!plr->plr->inGame)
                    continue;

                Writer_WriteInt32(writer, Net_GetPlayerID(i));
                plr->write(writer);
            }
        }
        SV_EndSegment();
    }
};

GameStateWriter::GameStateWriter() : d(new Instance/*(this)*/)
{}

void GameStateWriter::write(SaveInfo *saveInfo, Str const *path)
{
    DENG_ASSERT(saveInfo != 0 && path != 0);
    d->saveInfo = saveInfo;

    playerHeaderOK = false; // Uninitialized.

    if(!openGameSaveFile(path, true))
    {
        throw FileAccessError("GameStateWriter", "Failed opening \"" + de::String(Str_Text(path)) + "\"");
    }

    d->writer = SV_NewWriter();

    d->writeSessionHeader();
    d->writeWorldACScriptData();

    // Set the mobj archive numbers.
    initThingArchiveForSave();
#if !__JHEXEN__
    Writer_WriteInt32(d->writer, thingArchiveSize);
#endif

    writePlayerHeader(d->writer);

    d->writePlayers();
    d->writeMap();

    Writer_Delete(d->writer); d->writer = 0;
}

class GameStateReader
{
public:
    /// An error occurred attempting to open the input file. @ingroup errors
    DENG2_ERROR(FileAccessError);

public:
    GameStateReader();

    void read(SaveInfo *saveInfo, Str const *path);

private:
    DENG2_PRIVATE(d)
};

DENG2_PIMPL_NOREF(GameStateReader)
{
    SaveInfo *saveInfo; ///< Info for the save state to be read.
    Reader *reader;
    dd_bool loaded[MAXPLAYERS];
    dd_bool infile[MAXPLAYERS];

    Instance(/*Public *i*/)
        : /*Base(i)
        ,*/saveInfo(0)
        , reader(0)
    {
        de::zap(loaded);
        de::zap(infile);
    }

    /// Assumes the reader is currently positioned at the start of the stream.
    void seekToGameState()
    {
        // Read the header again.
        SaveInfo *tmp = new SaveInfo;
        SV_SaveInfo_Read(tmp, reader);
        delete tmp;
    }

    void readWorldACScriptData()
    {
#if __JHEXEN__
        if(saveInfo->version() >= 7)
        {
            SV_AssertSegment(ASEG_WORLDSCRIPTDATA);
        }
        Game_ACScriptInterpreter().readWorldScriptData(reader, saveInfo->version());
#endif
    }

    void readMap()
    {
#if __JHEXEN__ // The map state is actually read from a separate file.
        // Close the game state file.
        Z_Free(saveBuffer);
        SV_CloseFile();

        // Open the map state file.
        openMapSaveFile(saveSlots->composeSavePathForSlot(BASE_SLOT, gameMap + 1));
#endif

        MapStateReader(saveInfo->version()).read(reader);

#if __JHEXEN__
        Z_Free(saveBuffer);
#endif

#if !__JHEXEN__
        SV_ReadConsistencyBytes();
        SV_CloseFile();
#endif

        clearThingArchive();
#if __JHEXEN__
        clearTargetPlayers();
#endif
    }

    // We don't have the right to say which players are in the game. The players that already are
    // will continue to be. If the data for a given player is not in the savegame file, he will
    // be notified. The data for players who were saved but are not currently in the game will be
    // discarded.
    void readPlayers()
    {
        // Setup the dummy.
        ddplayer_t dummyDDPlayer;
        player_t dummyPlayer;
        dummyPlayer.plr = &dummyDDPlayer;

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            loaded[i] = 0;
#if !__JHEXEN__
            infile[i] = saveInfo->_players[i];
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
                player->read(reader);
            }
        }
        SV_AssertSegment(ASEG_END);
    }

    void kickMissingPlayers()
    {
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
    }
};

GameStateReader::GameStateReader() : d(new Instance/*(this)*/)
{}

void GameStateReader::read(SaveInfo *saveInfo, Str const *path)
{
    DENG_ASSERT(saveInfo != 0 && path != 0);
    d->saveInfo = saveInfo;

    playerHeaderOK = false; // Uninitialized.

    if(!openGameSaveFile(path, false))
    {
        throw FileAccessError("GameStateReader", "Failed opening \"" + de::String(Str_Text(path)) + "\"");
    }

    d->reader = SV_NewReader();

    d->seekToGameState();

    curInfo = d->saveInfo;

    d->readWorldACScriptData();

    /*
     * Load the map and configure some game settings.
     */
    briefDisabled = true;
    G_NewGame(d->saveInfo->mapUri(), 0/*not saved??*/, &d->saveInfo->gameRules());
    G_SetGameAction(GA_NONE); /// @todo Necessary?

#if !__JHEXEN__
    mapTime = d->saveInfo->mapTime();
#endif

#if !__JHEXEN__
    SV_InitThingArchiveForLoad(d->saveInfo->version() >= 5? Reader_ReadInt32(d->reader) : 1024 /* num elements */);
#endif

    readPlayerHeader(d->reader, d->saveInfo->version());

    d->readPlayers();
    d->readMap();

    // Notify the players that weren't in the savegame.
    d->kickMissingPlayers();

#if !__JHEXEN__
    // In netgames, the server tells the clients about this.
    NetSv_LoadGame(d->saveInfo->sessionId());
#endif

    Reader_Delete(d->reader); d->reader = 0;
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

void SV_ReadSector(Sector *sec, MapStateReader *msr)
{
    xsector_t *xsec = P_ToXSector(sec);
    Reader *reader  = msr->reader();
    int mapVersion  = msr->mapVersion();

    // A type byte?
    int type = 0;
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
    int ver = 1;
#if __JHEXEN__
    if(mapVersion > 2)
#else
    if(mapVersion > 4)
#endif
    {
        ver = Reader_ReadByte(reader);
    }

    int fh = Reader_ReadInt16(reader);
    int ch = Reader_ReadInt16(reader);

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

    Material *floorMaterial = 0, *ceilingMaterial = 0;
#if !__JHEXEN__
    if(mapVersion == 1)
    {
        // The flat numbers are absolute lump indices.
        Uri *uri = Uri_NewWithPath2("Flats:", RC_NULL);
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

    byte lightlevel;
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
        byte rgb[3];
        Reader_Read(reader, rgb, 3);
        for(int i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_COLOR_RED + i, rgb[i] / 255.f);
    }

    // Ver 2 includes surface colours
    if(ver >= 2)
    {
        byte rgb[3];
        Reader_Read(reader, rgb, 3);
        for(int i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_FLOOR_COLOR_RED + i, rgb[i] / 255.f);

        Reader_Read(reader, rgb, 3);
        for(int i = 0; i < 3; ++i)
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
    {
        SV_ReadXGSector(sec, reader, mapVersion);
    }
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

void SV_WriteLine(Line *li, MapStateWriter *msw)
{
    xline_t *xli   = P_ToXLine(li);
    Writer *writer = msw->writer();

#if !__JHEXEN__
    Writer_WriteByte(writer, xli->xg? 1 : 0); /// @c 1= XG data will follow.
#else
    Writer_WriteByte(writer, 0);
#endif

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
    xline_t *xli   = P_ToXLine(li);
    Reader *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    dd_bool xgDataFollows = false;
#if __JHEXEN__
    if(mapVersion >= 4)
#else
    if(mapVersion >= 2)
#endif
    {
        xgDataFollows = Reader_ReadByte(reader) == 1;
    }
#ifdef __JHEXEN__
    DENG_UNUSED(xgDataFollows);
#endif

    // A version byte?
    int ver = 1;
#if __JHEXEN__
    if(mapVersion >= 3)
#else
    if(mapVersion >= 5)
#endif
    {
        ver = (int) Reader_ReadByte(reader);
    }

    if(ver >= 4)
    {
        P_SetIntp(li, DMU_FLAGS, Reader_ReadInt16(reader));
    }

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
            de::zap(xli->mapped);
            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                P_SetLineAutomapVisibility(i, lineIDX, true);
            }
        }
    }

    xli->flags = flags;

    if(ver >= 3)
    {
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            xli->mapped[i] = Reader_ReadByte(reader);
        }
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

        Material *topMaterial = 0, *bottomMaterial = 0, *middleMaterial = 0;
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

            if(mapVersion < 12)
            {
                if(P_GetIntp(si, DMU_FLAGS) & SDF_SUPPRESS_BACK_SECTOR)
                    flags |= SDF_SUPPRESS_BACK_SECTOR;
            }
            P_SetIntp(si, DMU_FLAGS, flags);
        }
    }

#if !__JHEXEN__
    if(xgDataFollows)
    {
        SV_ReadXGLine(li, reader, mapVersion);
    }
#endif
}

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
    saveSlots->clearAllSaveInfo();

    inited = false;
}

static void loadGameState(Str const *path, SaveInfo &saveInfo)
{
    if(recognizeNativeState(path, &saveInfo))
    {
        GameStateReader().read(&saveInfo, path);
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

    if(!saveSlots->isValidSlot(slot))
    {
        return false;
    }

    App_Log(DE2_RES_VERBOSE, "Attempting load of save slot #%i...", slot);

    AutoStr *path = saveSlots->composeSavePathForSlot(slot);
    if(Str_IsEmpty(path))
    {
        App_Log(DE2_RES_ERROR, "Game not loaded: path \"%s\" is unreachable", SV_SavePath());
        return false;
    }

#if __JHEXEN__
    // Copy all needed save files to the base slot.
    /// @todo Why do this BEFORE loading?? (G_NewGame() does not load the serialized map state)
    /// @todo Does any caller ever attempt to load the base slot?? (Doesn't seem logical)
    if(slot != BASE_SLOT)
    {
        saveSlots->copySlot(slot, BASE_SLOT);
    }
#endif

    try
    {
        SaveInfo &info = saveSlots->saveInfo(logicalSlot);

        loadGameState(path, info);

        // Material scrollers must be re-spawned for older savegame versions.
        /// @todo Implement SaveInfo format type identifiers.
        if((info.magic() != (IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC)) ||
           info.version() <= 10)
        {
            P_SpawnAllMaterialOriginScrollers();
        }

        // Let the engine know where the local players are now.
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            R_UpdateConsoleView(i);
        }

        // Inform the engine that map setup must be performed once more.
        R_SetupMap(0, 0);

        // Make note of the last used save slot.
        Con_SetInteger2("game-save-last-slot", slot, SVF_WRITE_OVERRIDE);

        return true;
    }
    catch(de::Error const &er)
    {
        App_Log(DE2_RES_WARNING, "Error loading save slot #%i:\n%s", slot, er.asText().toLatin1().constData());
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
    players[CONSOLEPLAYER].write(writer);

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

    SaveInfo *info = new SaveInfo;
    Reader *reader = SV_NewReader();
    SV_SaveInfo_Read(info, reader);

    curInfo = info;

    if(info->magic() != MY_CLIENT_SAVE_MAGIC)
    {
        Reader_Delete(reader);
        delete info;
        SV_CloseFile();
        App_Log(DE2_RES_ERROR, "Client save file format not recognized");
        return;
    }

    // Do we need to change the map?
    if(!Uri_Equality(gameMapUri, info->mapUri()))
    {
        G_NewGame(info->mapUri(), 0/*default*/, &info->gameRules());
        /// @todo Necessary?
        G_SetGameAction(GA_NONE);
    }
    else
    {
        /// @todo Necessary?
        gameRules = info->gameRules();
    }
    mapTime = info->mapTime();

    P_MobjUnlink(mo);
    mo->origin[VX] = FIX2FLT(Reader_ReadInt32(reader));
    mo->origin[VY] = FIX2FLT(Reader_ReadInt32(reader));
    mo->origin[VZ] = FIX2FLT(Reader_ReadInt32(reader));
    P_MobjLink(mo);
    mo->floorZ     = FIX2FLT(Reader_ReadInt32(reader));
    mo->ceilingZ   = FIX2FLT(Reader_ReadInt32(reader));
    mo->angle      = Reader_ReadInt32(reader); /* $unifiedangles */

    cpl->plr->lookDir = Reader_ReadFloat(reader); /* $unifiedangles */

    readPlayerHeader(reader, info->version());
    cpl->read(reader);

    MapStateReader(info->version()).read(reader);

    SV_CloseFile();
    Reader_Delete(reader);
    delete info;
#else
    DENG_UNUSED(sessionId);
#endif
}

dd_bool SV_SaveGame(int slot, char const *description)
{
    DENG_ASSERT(inited);

#if __JHEXEN__
    int const logicalSlot = BASE_SLOT;
#else
    int const logicalSlot = slot;
#endif

    if(!saveSlots->isValidSlot(slot))
    {
        DENG_ASSERT(!"Invalid slot specified");
        return false;
    }
    if(!description || !description[0])
    {
        DENG_ASSERT(!"Empty description specified for slot");
        return false;
    }

    AutoStr *path = saveSlots->composeSavePathForSlot(logicalSlot);
    if(Str_IsEmpty(path))
    {
        App_Log(DE2_RES_WARNING, "Cannot save game: path \"%s\" is unreachable", SV_SavePath());
        return false;
    }

    App_Log(DE2_LOG_VERBOSE, "Attempting save game to \"%s\"", Str_Text(path));

    SaveInfo *info = SaveInfo::newWithCurrentSessionMetadata(AutoStr_FromTextStd(description));
    try
    {
        // In networked games the server tells the clients to save their games.
#if !__JHEXEN__
        NetSv_SaveGame(info->sessionId());
#endif

        GameStateWriter().write(info, path);

        // Swap the save info.
        saveSlots->replaceSaveInfo(logicalSlot, info);

#if __JHEXEN__
        // Copy base slot to destination slot.
        saveSlots->copySlot(logicalSlot, slot);
#endif

        // Make note of the last used save slot.
        Con_SetInteger2("game-save-last-slot", slot, SVF_WRITE_OVERRIDE);

        return true;
    }
    catch(de::Error const &er)
    {
        App_Log(DE2_RES_WARNING, "Error writing to save slot #%i:\n%s", slot, er.asText().toLatin1().constData());
    }

    // Discard the useless save info.
    delete info;

    return false;
}

#if __JHEXEN__
void SV_HxSaveHubMap()
{
    playerHeaderOK = false; // Uninitialized.

    SV_OpenFile(saveSlots->composeSavePathForSlot(BASE_SLOT, gameMap + 1), "wp");

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
    try
    {
        openMapSaveFile(saveSlots->composeSavePathForSlot(BASE_SLOT, gameMap + 1));

        MapStateReader(info->version()).read(reader);

#if __JHEXEN__
        clearThingArchive();
        Z_Free(saveBuffer);
#endif
    }
    catch(de::Error const &er)
    {
        App_Log(DE2_RES_WARNING, "Error loading hub map save state:\n%s", er.asText().toLatin1().constData());
    }

    Reader_Delete(reader);
}

void SV_HxBackupPlayersInHub(playerbackup_t playerBackup[MAXPLAYERS])
{
    DENG_ASSERT(playerBackup != 0);

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

void SV_HxRestorePlayersInHub(playerbackup_t playerBackup[MAXPLAYERS], uint mapEntrance)
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
            if(playerstart_t const *start = P_GetPlayerStart(mapEntrance, i, false))
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
