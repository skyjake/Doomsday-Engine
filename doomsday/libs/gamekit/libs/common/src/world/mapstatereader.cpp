/** @file mapstatereader.cpp  Saved map state reader.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "mapstatereader.h"

#include <de/arrayvalue.h>
#include <de/nativepath.h>
#include <de/string.h>
#include <doomsday/world/materialarchive.h>

#include "d_netsv.h"           /// @todo remove me
#include "dmu_lib.h"
#include "dmu_archiveindex.h"
#include "g_game.h"
#include "mapstatewriter.h"    // ChunkId
#include "p_actor.h"
#include "p_mapsetup.h"
#include "p_savedef.h"
#include "p_saveg.h"           // SV_ReadLine, SV_ReadSector
#include "p_saveio.h"
#include "player.h"
#include "polyobjs.h"
#include "r_common.h"
#include "thinkerinfo.h"
#include "gamesession.h"
#include "acs/system.h"

namespace internal
{
    static bool useMaterialArchiveSegments() {
#if __JHEXEN__
        return true;
#else
        return false;
#endif
    }

    static int thingArchiveVersionFor(int mapVersion) {
#if __JHEXEN__
        return mapVersion >= 4? 1 : 0;
#else
        return 0;
        DE_UNUSED(mapVersion);
#endif
    }
}

using namespace de;

DE_PIMPL(MapStateReader)
{
    reader_s *reader;
    int saveVersion;
    int mapVersion;
    bool formatHasMapVersionNumber;

    dd_bool loaded[MAXPLAYERS];
    dd_bool infile[MAXPLAYERS];

    int thingArchiveSize;

    ThingArchive *thingArchive;
    world::MaterialArchive *materialArchive;
    dmu_lib::SideArchive *sideArchive;
    Hash<Id::Type, thinker_t *> archivedThinkerIds;

    Impl(Public *i)
        : Base(i)
        , reader(0)
        , saveVersion(0)
        , mapVersion(0)
        , formatHasMapVersionNumber(false)
        , thingArchiveSize(0)
        , thingArchive(0)
        , materialArchive(0)
        , sideArchive(0)
    {
        de::zap(loaded);
        de::zap(infile);
    }

    ~Impl()
    {
        delete thingArchive;
        delete sideArchive;
        delete materialArchive;
        Reader_Delete(reader);
    }

    void beginSegment(int segId)
    {
#if __JHEXEN__
        if (segId == ASEG_END && SV_RawReader().source()->size() - SV_RawReader().offset() < 4)
        {
            App_Log(DE2_LOG_WARNING, "Savegame lacks ASEG_END marker (unexpected end-of-file)");
            return;
        }
        if (Reader_ReadInt32(reader) != segId)
        {
            /// @throw ReadError Failed alignment check.
            throw ReadError("MapStateReader", "Corrupt save game, segment #" + String::asText(segId) + " failed alignment check");
        }
#else
        DE_UNUSED(segId);
#endif
    }

    /// Special case check for the top-level map state segment.
    void beginMapSegment()
    {
#if __JHEXEN__
        int segId = Reader_ReadInt32(reader);
        if (segId != ASEG_MAP_HEADER2 && segId != ASEG_MAP_HEADER)
        {
            /// @throw ReadError Failed alignment check.
            throw ReadError("MapStateReader", "Corrupt save game, segment #" + String::asText(segId) + " failed alignment check");
        }
        formatHasMapVersionNumber = (segId == ASEG_MAP_HEADER2);
#else
        beginSegment(ASEG_MAP_HEADER2);
#endif
    }

    void endSegment()
    {
        beginSegment(ASEG_END);
    }

    void readMapHeader()
    {
#if __JHEXEN__
        // Maps have their own version number, in Hexen.
        mapVersion = (formatHasMapVersionNumber? Reader_ReadByte(reader) : 2);

        // Read the map timer.
        mapTime = Reader_ReadInt32(reader);
#endif
    }

    void readConsistencyBytes()
    {
#if !__JHEXEN__
        if (Reader_ReadByte(reader) != CONSISTENCY)
        {
            /// @throw ReadError Failed alignment check.
            throw ReadError("MapStateReader", "Corrupt save game, failed consistency check");
        }
#endif
    }

    void readMaterialArchive()
    {
        materialArchive = new world::MaterialArchive(::internal::useMaterialArchiveSegments(), false /*empty*/);
#if !__JHEXEN__
        if (mapVersion >= 4)
#endif
        {
            materialArchive->read(*reader, mapVersion < 6? 0 : -1);
        }
    }

    // We don't have the right to say which players are in the game. The players that already are
    // will continue to be. If the data for a given player is not in the savegame file, he will
    // be notified. The data for players who were saved but are not currently in the game will be
    // discarded.
    void readPlayers()
    {
#if __JHEXEN__
        if (saveVersion >= 4)
#else
        if (saveVersion >= 5)
#endif
        {
            beginSegment(ASEG_PLAYER_HEADER);
        }
        playerheader_t plrHdr;
        plrHdr.read(reader, saveVersion);

        // Setup the dummy.
        ddplayer_t dummyDDPlayer;
        player_t dummyPlayer;
        dummyPlayer.plr = &dummyDDPlayer;

#if !__JHEXEN__
        const ArrayValue &presentPlayers = self().metadata().geta("players");
#endif
        for (int i = 0; i < MAXPLAYERS; ++i)
        {
            loaded[i] = 0;
#if !__JHEXEN__
            infile[i] = presentPlayers.at(i).isTrue();;
#endif
        }

        beginSegment(ASEG_PLAYERS);
        {
#if __JHEXEN__
            for (int i = 0; i < MAXPLAYERS; ++i)
            {
                infile[i] = Reader_ReadByte(reader);
            }
#endif

            // Load the players.
            for (int i = 0; i < MAXPLAYERS; ++i)
            {
                // By default a saved player translates to nothing.
                saveToRealPlayerNum[i] = -1;

                if (!infile[i]) continue;

                // The ID number will determine which player this actually is.
                int pid = Reader_ReadInt32(reader);
                player_t *player = 0;
                for (int k = 0; k < MAXPLAYERS; ++k)
                {
                    if ((IS_NETGAME && int(Net_GetPlayerID(k)) == pid) ||
                       (!IS_NETGAME && k == 0))
                    {
                        // This is our guy.
                        player = players + k;
                        loaded[k] = true;
                        // Later references to the player number 'i' must be translated!
                        saveToRealPlayerNum[i] = k;
                        App_Log(DE2_DEV_MAP_MSG, "readPlayers: saved %i is now %i", i, k);
                        break;
                    }
                }

                if (!player)
                {
                    // We have a missing player. Use a dummy to load the data.
                    player = &dummyPlayer;
                }

                // Read the data.
                player->read(reader, plrHdr);
            }
        }
        endSegment();
    }

    void kickMissingPlayers()
    {
        for (int i = 0; i < MAXPLAYERS; ++i)
        {
            bool notLoaded = false;

#if __JHEXEN__
            if (players[i].plr->inGame)
            {
                // Try to find a saved player that corresponds this one.
                int k;
                for (k = 0; k < MAXPLAYERS; ++k)
                {
                    if (saveToRealPlayerNum[k] == i)
                    {
                        break;
                    }
                }
                if (k < MAXPLAYERS)
                    continue; // Found; don't bother this player.

                players[i].playerState = PST_REBORN;

                if (!i)
                {
                    // If the CONSOLEPLAYER isn't in the save, it must be some
                    // other player's file?
                    P_SetMessageWithFlags(players, GET_TXT(TXT_LOADMISSING), LMF_NO_HIDE);
                }
                else
                {
                    NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
                    notLoaded = true;
                }
            }
#else
            if (!loaded[i] && players[i].plr->inGame)
            {
                if (!i)
                {
                    P_SetMessageWithFlags(players, GET_TXT(TXT_LOADMISSING), LMF_NO_HIDE);
                }
                else
                {
                    NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
                }
                notLoaded = true;
            }
#endif

            if (notLoaded)
            {
                // Kick this player out, he doesn't belong here.
                DD_Executef(false, "kick %i", i);
            }
        }
    }

    void readElements()
    {
        beginSegment(ASEG_MAP_ELEMENTS);

        // Sectors.
        for (int i = 0; i < numsectors; ++i)
        {
            SV_ReadSector((Sector *)P_ToPtr(DMU_SECTOR, i), thisPublic);
        }

        // Lines.
        for (int i = 0; i < numlines; ++i)
        {
            SV_ReadLine((Line *)P_ToPtr(DMU_LINE, i), thisPublic);
        }

        // endSegment();
    }

    void readPolyobjs()
    {
#if __JHEXEN__
        beginSegment(ASEG_POLYOBJS);

        const int writtenPolyobjCount = Reader_ReadInt32(reader);
        DE_ASSERT(writtenPolyobjCount == numpolyobjs);
        for (int i = 0; i < writtenPolyobjCount; ++i)
        {
            /*Skip unused version byte*/ if (mapVersion >= 3) Reader_ReadByte(reader);

            Polyobj *po = Polyobj_ByTag(Reader_ReadInt32(reader));
            DE_ASSERT(po != 0);
            po->read(thisPublic);
        }

        // endSegment();
#endif
    }

    static int removeLoadSpawnedThinkerWorker(thinker_t *th, void * /*context*/)
    {
        if (th->function == (thinkfunc_t) P_MobjThinker)
        {
            P_MobjRemove((mobj_t *) th, true);
        }
        else
        {
            Z_Free(th);
        }

        return false; // Continue iteration.
    }

    void removeLoadSpawnedThinkers()
    {
#if !__JHEXEN__
        if (!IS_SERVER) return; // Not for us.
#endif

        Thinker_Iterate(0 /*all thinkers*/, removeLoadSpawnedThinkerWorker, 0/*no params*/);
        Thinker_Init();
    }

#if __JHEXEN__
    static bool mobjtypeHasCorpse(mobjtype_t type)
    {
        // Only corpses that call A_QueueCorpse from death routine.
        /// @todo fixme: What about mods? Look for this action in the death
        /// state sequence?
        switch (type)
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

    static int rebuildCorpseQueueWorker(thinker_t *th, void * /*context*/)
    {
        mobj_t *mo = (mobj_t *) th;

        // Must be a non-iced corpse.
        if ((mo->flags & MF_CORPSE) && !(mo->flags & MF_ICECORPSE) &&
           mobjtypeHasCorpse(mobjtype_t(mo->type)))
        {
            P_AddCorpseToQueue(mo);
        }

        return false; // Continue iteration.
    }

    /**
     * @todo fixme: the corpse queue should be serialized (original order unknown).
     */
    void rebuildCorpseQueue()
    {
        P_InitCorpseQueue();
        // Search the thinker list for corpses and place them in the queue.
        Thinker_Iterate((thinkfunc_t) P_MobjThinker, rebuildCorpseQueueWorker, NULL/*no params*/);
    }
#endif

    static int restoreMobjLinksWorker(thinker_t *th, void *context)
    {
        MapStateReader *inst = static_cast<MapStateReader *>(context);

        if (th->function != (thinkfunc_t) P_MobjThinker)
            return false; // Continue iteration.

        mobj_t *mo = (mobj_t *) th;
        mo->target = inst->mobj(PTR2INT(mo->target), &mo->target);
        mo->onMobj = inst->mobj(PTR2INT(mo->onMobj), &mo->onMobj);

#if __JHEXEN__
        switch (mo->type)
        {
        // Just tracer
        case MT_BISH_FX:
        case MT_HOLY_FX:
        case MT_DRAGON:
        case MT_THRUSTFLOOR_UP:
        case MT_THRUSTFLOOR_DOWN:
        case MT_MINOTAUR:
        case MT_SORCFX1:
            if (inst->mapVersion() >= 3)
            {
                mo->tracer = inst->mobj(PTR2INT(mo->tracer), &mo->tracer);
            }
            else
            {
                mo->tracer = inst->mobj(mo->special1, &mo->tracer);
                mo->special1 = 0;
            }
            break;

        // Just special2
        case MT_LIGHTNING_FLOOR:
        case MT_LIGHTNING_ZAP:
            mo->special2 = PTR2INT(inst->mobj(mo->special2, &mo->special2));
            break;

        // Both tracer and special2
        case MT_HOLY_TAIL:
        case MT_LIGHTNING_CEILING:
            if (inst->mapVersion() >= 3)
            {
                mo->tracer = inst->mobj(PTR2INT(mo->tracer), &mo->tracer);
            }
            else
            {
                mo->tracer = inst->mobj(mo->special1, &mo->tracer);
                mo->special1 = 0;
            }
            mo->special2 = PTR2INT(inst->mobj(mo->special2, &mo->special2));
            break;

        default:
            break;
        }
#else
# if __JDOOM__ || __JDOOM64__
        mo->tracer = inst->mobj(PTR2INT(mo->tracer), &mo->tracer);
# endif
# if __JHERETIC__
        mo->generator = inst->mobj(PTR2INT(mo->generator), &mo->generator);
# endif
#endif

        return false; // Continue iteration.
    }

    void readThinkers()
    {
#if !__JHEXEN__
#define PRE_VER5_END_SPECIALS  7
#endif

        const bool formatHasStasisInfo = (mapVersion >= 6);

#if __JHEXEN__
        if (mapVersion < 4)
            beginSegment(ASEG_MOBJS);
        else
#endif
            beginSegment(ASEG_THINKERS);

#if __JHEXEN__
        SV_InitTargetPlayers();
        thingArchive->initForLoad(Reader_ReadInt32(reader) /* num elements */);
#endif

        // Read in saved thinkers.
#if __JHEXEN__
        int i = 0;
        bool reachedSpecialsBlock = (mapVersion >= 4);
#else
        bool reachedSpecialsBlock = (mapVersion >= 5);
#endif

        byte tClass = 0;
        for (;;)
        {
#if __JHEXEN__
            if (reachedSpecialsBlock)
#endif
            {
                tClass = Reader_ReadByte(reader);
            }

#if __JHEXEN__
            if (mapVersion < 4)
            {
                if (reachedSpecialsBlock) // Have we started on the specials yet?
                {
                    // Versions prior to 4 used a different value to mark
                    // the end of the specials data and the thinker class ids
                    // are differrent, so we need to manipulate the thinker
                    // class identifier value.
                    if (tClass != TC_END)
                    {
                        tClass += 2;
                    }
                }
                else
                {
                    tClass = TC_MOBJ;
                }

                if (tClass == TC_MOBJ && (uint)i == thingArchive->size())
                {
                    beginSegment(ASEG_THINKERS);
                    // We have reached the begining of the "specials" block.
                    reachedSpecialsBlock = true;
                    continue;
                }
            }
#else
            if (mapVersion < 5)
            {
                if (reachedSpecialsBlock)
                {
                    // Versions prior to 5 used a different value to mark
                    // the end of the specials data so we need to manipulate
                    // the thinker class identifier value.
                    if (tClass == PRE_VER5_END_SPECIALS)
                    {
                        tClass = TC_END;
                    }
                    else
                    {
                        tClass += 3;
                    }
                }
                else if (tClass == TC_END)
                {
                    // We have reached the begining of the "specials" block.
                    reachedSpecialsBlock = true;
                    continue;
                }
            }
#endif

            if (tClass == TC_END)
                break; // End of the list.

            ThinkerClassInfo *thInfo = SV_ThinkerInfoForClass(thinkerclass_t(tClass));
            DE_ASSERT(thInfo != 0);
            // Not for us? (it shouldn't be here anyway!).
            DE_ASSERT(!((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT));

            // Mobjs use a special engine-side allocator.
            thinker_t *th = 0;
            if (thInfo->thinkclass == TC_MOBJ)
            {
                th = reinterpret_cast<thinker_t *>(Mobj_CreateXYZ((thinkfunc_t) P_MobjThinker, 0, 0, 0, 0, 64, 64, 0));
            }
            else
            {
                th = Thinker(Thinker::AllocateMemoryZone, thInfo->size).take();
            }

            bool putThinkerInStasis = (formatHasStasisInfo? CPP_BOOL(Reader_ReadByte(reader)) : false);

            // Private identifier of the thinker.
            if (saveVersion >= 15)
            {
                const Id::Type privateId = Reader_ReadUInt32(reader);
                archivedThinkerIds.insert(privateId, th);
            }

            if (thInfo->readFunc(th, thisPublic))
            {
                Thinker_Add(th);
            }

            if (putThinkerInStasis)
            {
                Thinker_SetStasis(th, true);
            }

#if __JHEXEN__
            if (tClass == TC_MOBJ)
            {
                i++;
            }
#endif
        }

        // Update references between thinkers.
#if __JHEXEN__
        Thinker_Iterate((thinkfunc_t) P_MobjThinker, restoreMobjLinksWorker, thisPublic);

        P_CreateTIDList();
        rebuildCorpseQueue();

#else
        if (IS_SERVER)
        {
            Thinker_Iterate((thinkfunc_t) P_MobjThinker, restoreMobjLinksWorker, thisPublic);

            for (int i = 0; i < numlines; ++i)
            {
                xline_t *xline = P_ToXLine((Line *)P_ToPtr(DMU_LINE, i));
                if (!xline->xg) continue;

                xline->xg->activator = thingArchive->mobj(PTR2INT(xline->xg->activator),
                                                          &xline->xg->activator);
            }
        }
#endif

#undef PRE_VER5_END_SPECIALS
    }

    void readACScriptData()
    {
#if __JHEXEN__
        beginSegment(ASEG_SCRIPTS);
        gfw_Session()->acsSystem().readMapState(thisPublic);
        // endSegment();
#endif
    }

    void readSoundSequences()
    {
#if __JHEXEN__
        beginSegment(ASEG_SOUNDS);
        SN_ReadSequences(reader, mapVersion);
        // endSegment();
#endif
    }

    void readMisc()
    {
#if __JHEXEN__
        beginSegment(ASEG_MISC);

        for (int i = 0; i < MAXPLAYERS; ++i)
        {
            localQuakeHappening[i] = Reader_ReadInt32(reader);
        }
#endif
#if __JDOOM__
        DE_ASSERT(theBossBrain != 0);
        theBossBrain->read(thisPublic);
#endif
    }

    void readSoundTargets()
    {
#if !__JHEXEN__
        if (!IS_SERVER) return; // Not for us.

        // Sound target data was introduced in ver 5
        if (mapVersion < 5) return;

        int numTargets = Reader_ReadInt32(reader);
        for (int i = 0; i < numTargets; ++i)
        {
            xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader)));
            DE_ASSERT(xsec != 0);

            if (!xsec)
            {
                DE_UNUSED(Reader_ReadInt16(reader));
                continue;
            }

            xsec->soundTarget = INT2PTR(mobj_t, Reader_ReadInt16(reader));
            xsec->soundTarget = thingArchive->mobj(PTR2INT(xsec->soundTarget), &xsec->soundTarget);
        }
#endif
    }
};

MapStateReader::MapStateReader(const GameStateFolder &session)
    : GameStateFolder::MapStateReader(session)
    , d(new Impl(this))
{}

MapStateReader::~MapStateReader()
{}

void MapStateReader::read(const String &mapUriStr)
{
    res::Uri const mapUri(mapUriStr, RC_NULL);
    const File &mapStateFile = folder().locate<File const>(String("maps") / mapUri.path() + "State");
    SV_OpenFileForRead(mapStateFile);
    d->reader = SV_NewReader();

    /*magic*/ Reader_ReadInt32(d->reader);

    d->saveVersion = Reader_ReadInt32(d->reader);
    d->mapVersion  = d->saveVersion; // Default: mapVersion == saveVersion

    d->thingArchiveSize = 0;
#if !__JHEXEN__
    d->thingArchiveSize = (d->saveVersion >= 5? Reader_ReadInt32(d->reader) : 1024);
#endif

    d->readPlayers();

    // Prepare and populate the side archive.
    d->sideArchive = new dmu_lib::SideArchive;

    // Deserialize the map.
    d->beginMapSegment();
    {
        d->readMapHeader();
        d->readMaterialArchive();

        d->thingArchive = new ThingArchive(::internal::thingArchiveVersionFor(d->mapVersion));
#if !__JHEXEN__
        d->thingArchive->initForLoad(d->thingArchiveSize);
#endif
        d->removeLoadSpawnedThinkers();

        d->readElements();
        d->readPolyobjs();
        d->readThinkers();
        d->readACScriptData();
        d->readSoundSequences();
        d->readMisc();
        d->readSoundTargets();
    }
    d->endSegment();

    // Cleanup.
    //delete d->thingArchive;     d->thingArchive = 0;
    delete d->sideArchive;      d->sideArchive = 0;
    delete d->materialArchive;  d->materialArchive = 0;

    d->readConsistencyBytes();
    Reader_Delete(d->reader); d->reader = 0;

#if __JHEXEN__
    SV_ClearTargetPlayers();
#endif

    SV_CloseFile();

    // Notify the players that weren't in the savegame.
    d->kickMissingPlayers();

    // In netgames, the server tells the clients about this.
    NetSv_LoadGame(metadata().geti("sessionId"));

    // Material scrollers must be spawned for older savegame versions.
    if (d->saveVersion <= 10)
    {
        P_SpawnAllMaterialOriginScrollers();
    }

    // Let the engine know where the local players are now.
    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        R_UpdateConsoleView(i);
    }

    // Inform the engine that map setup must be performed once more.
    R_SetupMap(0, 0);
}

mobj_t *MapStateReader::mobj(ThingArchive::SerialId serialId, void *address) const
{
    DE_ASSERT(d->thingArchive != 0);
    return d->thingArchive->mobj(serialId, address);
}

world_Material *MapStateReader::material(materialarchive_serialid_t serialId, int group) const
{
    DE_ASSERT(d->materialArchive != 0);
    return reinterpret_cast<world_Material *>(d->materialArchive->find(serialId, group));
}

Side *MapStateReader::side(int sideIndex) const
{
    DE_ASSERT(d->sideArchive != 0);
    return reinterpret_cast<Side *>(d->sideArchive->at(sideIndex));
}

thinker_t *MapStateReader::thinkerForPrivateId(Id::Type id) const
{
    auto found = d->archivedThinkerIds.find(id);
    if (found != d->archivedThinkerIds.end())
    {
        return found->second;
    }
    return nullptr;
}

player_t *MapStateReader::player(int serialId) const
{
    DE_ASSERT(serialId > 0 && serialId <= MAXPLAYERS);
    return players + saveToRealPlayerNum[serialId - 1];
}

int MapStateReader::mapVersion()
{
    return d->mapVersion;
}

reader_s *MapStateReader::reader()
{
    DE_ASSERT(d->reader != 0);
    return d->reader;
}

void MapStateReader::addMobjToThingArchive(mobj_t *mobj, ThingArchive::SerialId serialId)
{
    DE_ASSERT(d->thingArchive != 0);
    d->thingArchive->insert(mobj, serialId);
}
