/** @file mapstatewriter.cpp  Saved map state writer.
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

#include "common.h"
#include "mapstatewriter.h"

#include "dmu_lib.h"
#include "g_game.h"
#include "gamesession.h"
#include "p_savedef.h"   // MY_SAVE_VERSION
#include "p_saveg.h"     // SV_WriteSector, SV_WriteLine
#include "polyobjs.h"
#include "thinkerinfo.h"
#include "acs/system.h"

#include <doomsday/world/materialarchive.h>
#include <doomsday/world/thinkerdata.h>

namespace internal
{
    static bool MapStateWriter_useMaterialArchiveSegments() {
#if __JHEXEN__
        return true;
#else
        return false;
#endif
    }
}

DE_PIMPL(MapStateWriter)
{
    ThingArchive *thingArchive;
    world::MaterialArchive *materialArchive;
    Writer1 *writer; // Not owned.

    Impl(Public *i)
        : Base(i)
        , thingArchive(0)
        , materialArchive(0)
        , writer(0)
    {}

    ~Impl()
    {
        delete materialArchive;
        delete thingArchive;
    }

    void beginSegment(int segId)
    {
#if __JHEXEN__
        Writer_WriteInt32(writer, segId);
#else
        DE_UNUSED(segId);
#endif
    }

    void endSegment()
    {
        beginSegment(ASEG_END);
    }

    void writeConsistencyBytes()
    {
#if !__JHEXEN__
        Writer_WriteByte(writer, CONSISTENCY);
#endif
    }

    void writeMapHeader()
    {
#if __JHEXEN__
        // Maps have their own version number.
        Writer_WriteByte(writer, MY_SAVE_VERSION);

        // Write the map timer
        Writer_WriteInt32(writer, mapTime);
#endif
    }

    void writeMaterialArchive()
    {
        materialArchive->write(*writer);
    }

    void writePlayers()
    {
        beginSegment(ASEG_PLAYER_HEADER);
        playerheader_t plrHdr;
        plrHdr.write(writer);

        beginSegment(ASEG_PLAYERS);
        {
#if __JHEXEN__
            for (int i = 0; i < MAXPLAYERS; ++i)
            {
                Writer_WriteByte(writer, players[i].plr->inGame);
            }
#endif

            for (int i = 0; i < MAXPLAYERS; ++i)
            {
                player_t *plr = players + i;
                if (!plr->plr->inGame)
                    continue;

                Writer_WriteInt32(writer, Net_GetPlayerID(i));
                plr->write(writer, plrHdr);
            }
        }
        endSegment();
    }

    void writeElements()
    {
        beginSegment(ASEG_MAP_ELEMENTS);

        for (int i = 0; i < numsectors; ++i)
        {
            SV_WriteSector((Sector *)P_ToPtr(DMU_SECTOR, i), thisPublic);
        }

        for (int i = 0; i < numlines; ++i)
        {
            SV_WriteLine((Line *)P_ToPtr(DMU_LINE, i), thisPublic);
        }

        // endSegment();
    }

    void writePolyobjs()
    {
#if __JHEXEN__
        beginSegment(ASEG_POLYOBJS);

        Writer_WriteInt32(writer, numpolyobjs);
        for (int i = 0; i < numpolyobjs; ++i)
        {
            Polyobj *po = Polyobj_ById(i);
            DE_ASSERT(po != 0);
            po->write(thisPublic);
        }

        // endSegment();
#endif
    }

    struct writethinkerworker_params_t
    {
        MapStateWriter *msw;
        bool excludePlayers;
    };

    /**
     * Serializes the specified thinker and writes it to save state.
     */
    static int writeThinkerWorker(thinker_t *th, void *context)
    {
        const writethinkerworker_params_t &p = *static_cast<writethinkerworker_params_t *>(context);

        // We are only concerned with thinkers we have save info for.
        ThinkerClassInfo *thInfo = SV_ThinkerInfo(*th);
        if (!thInfo) return false;

        // Are we excluding players?
        if (p.excludePlayers)
        {
            if (th->function == P_MobjThinker && ((mobj_t *) th)->player)
            {
                return false;
            }
        }

        // Only the server saves this class of thinker?
        if ((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT)
        {
            return false;
        }

        // Write the header block for this thinker.
        Writer_WriteByte(p.msw->writer(), thInfo->thinkclass); // Thinker type byte.
        Writer_WriteByte(p.msw->writer(), Thinker_InStasis(th)? 1 : 0); // In stasis?

        // Private identifier of the thinker.
        const de::Id::Type privateId = (th->d? THINKER_DATA(*th, ThinkerData).id().asUInt32() : 0);
        Writer_WriteUInt32(p.msw->writer(), privateId);

        // Write the thinker data.
        thInfo->writeFunc(th, p.msw);

        return false;
    }

    /**
     * Serializes thinkers for both client and server.
     *
     * @note Clients do not save data for all thinkers. In some cases the server will send it
     * anyway (so saving it would just bloat client save states).
     *
     * @note Some thinker classes are NEVER saved by clients.
     */
    void writeThinkers()
    {
        beginSegment(ASEG_THINKERS);

#if __JHEXEN__
        Writer_WriteInt32(writer, thingArchive->size()); // number of mobjs.
#endif

        // Serialize qualifying thinkers.
        writethinkerworker_params_t parm; de::zap(parm);
        parm.msw            = thisPublic;
        parm.excludePlayers = thingArchive->excludePlayers();
        Thinker_Iterate(0/*all thinkers*/, writeThinkerWorker, &parm);

        // Mark the end of the thinkers.
        // endSegment();
        Writer_WriteByte(writer, TC_END);
    }

    void writeACScriptData()
    {
#if __JHEXEN__
        beginSegment(ASEG_SCRIPTS);
        gfw_Session()->acsSystem().writeMapState(thisPublic);
        // endSegment();
#endif
    }

    void writeSoundSequences()
    {
#if __JHEXEN__
        beginSegment(ASEG_SOUNDS);
        SN_WriteSequences(writer);
        // endSegment();
#endif
    }

    void writeMisc()
    {
#if __JHEXEN__
        beginSegment(ASEG_MISC);
        for (int i = 0; i < MAXPLAYERS; ++i)
        {
            Writer_WriteInt32(writer, localQuakeHappening[i]);
        }
#endif
#if __JDOOM__
        DE_ASSERT(theBossBrain != 0);
        theBossBrain->write(thisPublic);
#endif
    }

    void writeSoundTargets()
    {
#if !__JHEXEN__
        if (!IS_SERVER) return; // Not for us.

        // Write the total number.
        int count = 0;
        for (int i = 0; i < numsectors; ++i)
        {
            xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, i));
            if (xsec->soundTarget)
            {
                count += 1;
            }
        }

        // beginSegment();
        Writer_WriteInt32(writer, count);

        // Write the mobj references using the mobj archive.
        for (int i = 0; i < numsectors; ++i)
        {
            xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, i));
            if (xsec->soundTarget)
            {
                Writer_WriteInt32(writer, i);
                Writer_WriteInt16(writer, thingArchive->serialIdFor(xsec->soundTarget));
            }
        }
        // endSegment();
#endif
    }
};

MapStateWriter::MapStateWriter() : d(new Impl(this))
{}

void MapStateWriter::write(Writer1 *writer, bool excludePlayers)
{
    DE_ASSERT(writer != 0);
    d->writer = writer;

    // Prepare and populate the material archive.
    d->materialArchive = new world::MaterialArchive(::internal::MapStateWriter_useMaterialArchiveSegments());
    d->materialArchive->addWorldMaterials();

    Writer_WriteInt32(writer, MY_SAVE_MAGIC);
    Writer_WriteInt32(writer, MY_SAVE_VERSION);

    // Set the mobj archive numbers.
    d->thingArchive = new ThingArchive;
    d->thingArchive->initForSave(excludePlayers);
#if !__JHEXEN__
    Writer_WriteInt32(d->writer, d->thingArchive->size());
#endif

    d->writePlayers();

    // Serialize the map.
    d->beginSegment(ASEG_MAP_HEADER2);
    {
        d->writeMapHeader();
        d->writeMaterialArchive();

        d->writeElements();
        d->writePolyobjs();
        d->writeThinkers();
        d->writeACScriptData();
        d->writeSoundSequences();
        d->writeMisc();
        d->writeSoundTargets();
    }
    d->endSegment();
    d->writeConsistencyBytes(); // To be absolutely sure...

    // Cleanup.
    delete d->materialArchive;  d->materialArchive = 0;
}

ThingArchive::SerialId MapStateWriter::serialIdFor(const mobj_t *mobj)
{
    DE_ASSERT(d->thingArchive != 0);
    return d->thingArchive->serialIdFor(mobj);
}

materialarchive_serialid_t MapStateWriter::serialIdFor(world::Material *material)
{
    DE_ASSERT(d->materialArchive != 0);
    return d->materialArchive->findUniqueSerialId(material);
}

materialarchive_serialid_t MapStateWriter::serialIdFor(material_s *material)
{
    return serialIdFor(reinterpret_cast<world::Material *>(material));
}

Writer1 *MapStateWriter::writer()
{
    DE_ASSERT(d->writer != 0);
    return d->writer;
}
