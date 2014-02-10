/** @file mapstatewriter.cpp  Saved map state writer.
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
#include "mapstatewriter.h"

#include "dmu_lib.h"
#include "p_saveg.h"
#include "p_saveio.h"
#include "thinkerinfo.h"

DENG2_PIMPL(MapStateWriter)
{
    bool excludePlayers;
    Writer *writer;
    MaterialArchive *materialArchive;

    Instance(Public *i)
        : Base(i)
        , excludePlayers(false)
        , writer(0)
        , materialArchive(0)
    {}

    void beginSegment(int segmentId)
    {
        SV_BeginSegment(segmentId);
    }

    void endSegment()
    {
        SV_EndSegment();
    }

    void beginMapSegment()
    {
        beginSegment(ASEG_MAP_HEADER2);

#if __JHEXEN__
        Writer_WriteByte(writer, MY_SAVE_VERSION); // Map version also.

        // Write the map timer
        Writer_WriteInt32(writer, mapTime);
#endif

        // Create and populate the MaterialArchive.
#ifdef __JHEXEN__
        materialArchive = MaterialArchive_New(true /* segment check */);
#else
        materialArchive = MaterialArchive_New(false);
#endif
        MaterialArchive_Write(materialArchive, writer);
    }

    void endMapSegment()
    {
        endSegment();
        MaterialArchive_Delete(materialArchive); materialArchive = 0;
    }

    void writeElements()
    {
        beginSegment(ASEG_MAP_ELEMENTS);

        for(int i = 0; i < numsectors; ++i)
        {
            SV_WriteSector((Sector *)P_ToPtr(DMU_SECTOR, i), thisPublic);
        }

        for(int i = 0; i < numlines; ++i)
        {
            SV_WriteLine((Line *)P_ToPtr(DMU_LINE, i), thisPublic);
        }
    }

    void writePolyobjs()
    {
#if __JHEXEN__
        beginSegment(ASEG_POLYOBJS);

        Writer_WriteInt32(writer, numpolyobjs);
        for(int i = 0; i < numpolyobjs; ++i)
        {
            SV_WritePolyObj(Polyobj_ById(i), thisPublic);
        }
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
        writethinkerworker_params_t &p = *static_cast<writethinkerworker_params_t *>(context);

        // We are only concerned with thinkers we have save info for.
        ThinkerClassInfo *thInfo = SV_ThinkerInfo(*th);
        if(!thInfo) return false;

        // Are we excluding players?
        if(p.excludePlayers)
        {
            if(th->function == (thinkfunc_t) P_MobjThinker && ((mobj_t *) th)->player)
                return false; // Continue iteration.
        }

        // Only the server saves this class of thinker?
        if((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT)
            return false;

        // Write the header block for this thinker.
        Writer_WriteByte(p.msw->writer(), thInfo->thinkclass); // Thinker type byte.
        Writer_WriteByte(p.msw->writer(), th->inStasis? 1 : 0); // In stasis?

        // Write the thinker data.
        thInfo->writeFunc(th, p.msw);

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
    void writeThinkers()
    {
        beginSegment(ASEG_THINKERS);

#if __JHEXEN__
        Writer_WriteInt32(writer, thingArchiveSize); // number of mobjs.
#endif

        // Serialize qualifying thinkers.
        writethinkerworker_params_t parm; de::zap(parm);
        parm.msw = thisPublic;
        parm.excludePlayers = excludePlayers;
        Thinker_Iterate(0/*all thinkers*/, writeThinkerWorker, &parm);

        // Mark the end of the thinkers.
        Writer_WriteByte(writer, TC_END);
    }

    void writeACScriptData()
    {
#if __JHEXEN__
        beginSegment(ASEG_SCRIPTS);
        Game_ACScriptInterpreter().writeMapScriptData(writer);
#endif
    }

    void writeSoundSequences()
    {
#if __JHEXEN__
        beginSegment(ASEG_SOUNDS);
        SN_WriteSequences(writer);
#endif
    }

    void writeBrain()
    {
#if __JDOOM__
        DENG_ASSERT(bossBrain != 0);
        bossBrain->write(thisPublic);
#endif
    }

    void writeSoundTargets()
    {
#if !__JHEXEN__
        // Not for us?
        if(!IS_SERVER) return;

        // Write the total number.
        int count = 0;
        for(int i = 0; i < numsectors; ++i)
        {
            xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, i));
            if(xsec->soundTarget)
            {
                count += 1;
            }
        }
        Writer_WriteInt32(writer, count);

        // Write the mobj references using the mobj archive.
        for(int i = 0; i < numsectors; ++i)
        {
            xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, i));

            if(xsec->soundTarget)
            {
                Writer_WriteInt32(writer, i);
                Writer_WriteInt16(writer, SV_ThingArchiveId(xsec->soundTarget));
            }
        }
#endif
    }

    void writeMisc()
    {
#if __JHEXEN__
        beginSegment(ASEG_MISC);

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            Writer_WriteInt32(writer, localQuakeHappening[i]);
        }
#endif
    }
};

MapStateWriter::MapStateWriter(bool excludePlayers)
    : d(new Instance(this))
{
    d->excludePlayers = excludePlayers;
}

void MapStateWriter::write(Writer *writer)
{
    DENG_ASSERT(writer != 0);
    d->writer = writer;

    d->beginMapSegment();
    {
        d->writeElements();
        d->writePolyobjs();
        d->writeThinkers();
        d->writeACScriptData();
        d->writeSoundSequences();
        d->writeMisc();
        d->writeBrain();
        d->writeSoundTargets();
    }
    d->endMapSegment();
}

Writer *MapStateWriter::writer()
{
    DENG_ASSERT(d->writer != 0);
    return d->writer;
}

materialarchive_serialid_t MapStateWriter::serialIdFor(Material *material)
{
    DENG_ASSERT(d->materialArchive != 0);
    return MaterialArchive_FindUniqueSerialId(d->materialArchive, material);
}
