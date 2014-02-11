/** @file mapstatereader.cpp  Saved map state reader.
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
#include "mapstatereader.h"

#include "dmu_lib.h"
#include "p_actor.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "thinkerinfo.h"
#include <de/String>

DENG2_PIMPL(MapStateReader)
{
    Reader *reader;
    int saveVersion;
    int mapVersion;
    MaterialArchive *materialArchive;
    dmu_lib::SideArchive *sideArchive;

    Instance(Public *i)
        : Base(i)
        , reader(0)
        , saveVersion(0)
        , mapVersion(0)
        , materialArchive(0)
        , sideArchive(0)
    {}

    void checkSegment(int segId)
    {
#if __JHEXEN__
        if(segId == ASEG_END && SV_HxBytesLeft() < 4)
        {
            App_Log(DE2_LOG_WARNING, "Savegame lacks ASEG_END marker (unexpected end-of-file)");
            return;
        }
        if(Reader_ReadInt32(reader) != segId)
        {
            /// @throw ReadError Failed alignment check.
            throw ReadError("MapStateReader::checkSegment", "Corrupt save game, segment " + de::String::number(segId) + " failed alignment check");
        }
#else
        DENG_UNUSED(segId);
#endif
    }

    /// Special case check for the top-level map state segment.
    void checkMapSegment()
    {
#if __JHEXEN__
        int segId = Reader_ReadInt32(reader);
        if(segId != ASEG_MAP_HEADER2 && segId != ASEG_MAP_HEADER)
        {
            /// @throw ReadError Failed alignment check.
            throw ReadError("MapStateReader::checkSegment", "Corrupt save game, segment " + de::String::number(segId) + " failed alignment check");
        }

        // Maps have their own version number, in Hexen.
        mapVersion = (segId == ASEG_MAP_HEADER2? Reader_ReadByte(reader) : 2);
#else
        checkSegment(ASEG_MAP_HEADER2);
#endif
    }

    void beginMapSegment()
    {
        checkMapSegment();

#if __JHEXEN__
        thingArchiveVersion = mapVersion >= 4? 1 : 0;
#endif

#if __JHEXEN__
        // Read the map timer.
        mapTime = Reader_ReadInt32(reader);
#endif

        // Read the material archive for the map.
#ifdef __JHEXEN__
        materialArchive = MaterialArchive_NewEmpty(true /* segment checks */);
#else
        materialArchive = MaterialArchive_NewEmpty(false);
#endif
#if !__JHEXEN__
        if(mapVersion >= 4)
#endif
        {
            MaterialArchive_Read(materialArchive, reader, mapVersion < 6? 0 : -1);
        }

        sideArchive = new dmu_lib::SideArchive;
    }

    void endMapSegment()
    {
        checkSegment(ASEG_END);

        delete sideArchive; sideArchive = 0;
        MaterialArchive_Delete(materialArchive); materialArchive = 0;
    }

    void readElements()
    {
        checkSegment(ASEG_MAP_ELEMENTS);

        // Sectors.
        for(int i = 0; i < numsectors; ++i)
        {
            SV_ReadSector((Sector *)P_ToPtr(DMU_SECTOR, i), thisPublic);
        }

        // Lines.
        for(int i = 0; i < numlines; ++i)
        {
            SV_ReadLine((Line *)P_ToPtr(DMU_LINE, i), thisPublic);
        }
    }

    void readPolyobjs()
    {
#if __JHEXEN__
        checkSegment(ASEG_POLYOBJS);

        int const writtenPolyobjCount = Reader_ReadInt32(reader);
        DENG_ASSERT(writtenPolyobjCount == numpolyobjs);
        for(int i = 0; i < writtenPolyobjCount; ++i)
        {
            SV_ReadPolyObj(thisPublic);
        }
#endif
    }

    static int removeThinkerWorker(thinker_t *th, void * /*context*/)
    {
        if(th->function == (thinkfunc_t) P_MobjThinker)
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

    static int rebuildCorpseQueueWorker(thinker_t *th, void * /*context*/)
    {
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
    void rebuildCorpseQueue()
    {
        P_InitCorpseQueue();
        // Search the thinker list for corpses and place them in the queue.
        Thinker_Iterate((thinkfunc_t) P_MobjThinker, rebuildCorpseQueueWorker, NULL/*no params*/);
    }
#endif

    static int restoreMobjLinksWorker(thinker_t *th, void *context)
    {
        int const mapVersion = *static_cast<int const *>(context);

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

#if !__JHEXEN__
        DENG_UNUSED(mapVersion);
#endif
    }

    void relinkThinkers()
    {
#if __JHEXEN__
        Thinker_Iterate((thinkfunc_t) P_MobjThinker, restoreMobjLinksWorker, &mapVersion);

        P_CreateTIDList();
        rebuildCorpseQueue();

#else
        if(IS_SERVER)
        {
            Thinker_Iterate((thinkfunc_t) P_MobjThinker, restoreMobjLinksWorker, &mapVersion);

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

    void readThinkers()
    {
        bool const formatHasStasisInfo = (mapVersion >= 6);

        removeLoadSpawnedThinkers();

#if __JHEXEN__
        if(mapVersion < 4)
            checkSegment(ASEG_MOBJS);
        else
#endif
            checkSegment(ASEG_THINKERS);

#if __JHEXEN__
        SV_InitTargetPlayers();
        SV_InitThingArchiveForLoad(Reader_ReadInt32(reader) /* num elements */);
#endif

        // Read in saved thinkers.
#if __JHEXEN__
        int i = 0;
        bool reachedSpecialsBlock = (mapVersion >= 4);
#else
        bool reachedSpecialsBlock = (mapVersion >= 5);
#endif

        byte tClass = 0;
        forever
        {
#if __JHEXEN__
            if(reachedSpecialsBlock)
#endif
                tClass = Reader_ReadByte(reader);

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
                    checkSegment(ASEG_THINKERS);
                    // We have reached the begining of the "specials" block.
                    reachedSpecialsBlock = true;
                    continue;
                }
            }
#else
            if(mapVersion < 5)
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

            ThinkerClassInfo *thInfo = SV_ThinkerInfoForClass(thinkerclass_t(tClass));
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

            bool putThinkerInStasis = (formatHasStasisInfo? CPP_BOOL(Reader_ReadByte(reader)) : false);

            if(thInfo->readFunc(th, thisPublic))
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

    void readACScriptData()
    {
#if __JHEXEN__
        checkSegment(ASEG_SCRIPTS);
        Game_ACScriptInterpreter().readMapScriptData(thisPublic);
#endif
    }

    void readSoundSequences()
    {
#if __JHEXEN__
        checkSegment(ASEG_SOUNDS);
        SN_ReadSequences(reader, mapVersion);
#endif
    }

    void readMisc()
    {
#if __JHEXEN__
        checkSegment(ASEG_MISC);

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            localQuakeHappening[i] = Reader_ReadInt32(reader);
        }
#endif
    }

    void readBrain()
    {
#if __JDOOM__
        DENG_ASSERT(bossBrain != 0);
        bossBrain->read(thisPublic);
#endif
    }

    void readSoundTargets()
    {
#if !__JHEXEN__
        // Not for us?
        if(!IS_SERVER) return;

        // Sound target data was introduced in ver 5
        if(mapVersion < 5) return;

        int numTargets = Reader_ReadInt32(reader);
        for(int i = 0; i < numTargets; ++i)
        {
            xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, Reader_ReadInt32(reader)));
            DENG_ASSERT(xsec != 0);

            if(!xsec)
            {
                DENG_UNUSED(Reader_ReadInt16(reader));
                continue;
            }

            xsec->soundTarget = INT2PTR(mobj_t, Reader_ReadInt16(reader));
            xsec->soundTarget =
                SV_GetArchiveThing(PTR2INT(xsec->soundTarget), &xsec->soundTarget);
        }
#endif
    }
};

MapStateReader::MapStateReader(int saveVersion)
    : d(new Instance(this))
{
    d->saveVersion = saveVersion;
    d->mapVersion  = saveVersion; // Default: mapVersion == saveVersion
}

void MapStateReader::read(Reader *reader)
{
    DENG_ASSERT(reader != 0);
    d->reader = reader;

    d->beginMapSegment();
    {
        d->readElements();
        d->readPolyobjs();
        d->readThinkers();
        d->readACScriptData();
        d->readSoundSequences();
        d->readMisc();
        d->readBrain();
        d->readSoundTargets();
    }
    d->endMapSegment();
}

Reader *MapStateReader::reader()
{
    DENG_ASSERT(d->reader != 0);
    return d->reader;
}

int MapStateReader::mapVersion()
{
    return d->mapVersion;
}

Material *MapStateReader::material(materialarchive_serialid_t serialId, int group)
{
    DENG_ASSERT(d->materialArchive != 0);
    return MaterialArchive_Find(d->materialArchive, serialId, group);
}

dmu_lib::SideArchive &MapStateReader::sideArchive()
{
    DENG_ASSERT(d->sideArchive != 0);
    return *d->sideArchive;
}
