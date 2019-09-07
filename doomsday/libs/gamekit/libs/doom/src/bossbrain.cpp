/** @file bossbrain.cpp  Playsim "Boss Brain" management.
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

#include "jdoom.h"
#include "bossbrain.h"
#include "gamesession.h"
#include "p_saveg.h"

BossBrain *theBossBrain; // The One boss brain.

DE_PIMPL_NOREF(BossBrain)
{
    int easy;
    int targetOn;
    int numTargets;
    int maxTargets;
    mobj_t **targets;

    Impl()
        : easy(0) // Always init easy to 0.
        , targetOn(0)
        , numTargets(0)
        , maxTargets(-1)
        , targets(0)
    {}

    ~Impl()
    {
        Z_Free(targets);
    }
};

BossBrain::BossBrain() : d(new Impl)
{}

void BossBrain::clearTargets()
{
    d->numTargets = 0;
    d->targetOn = 0;
}

int BossBrain::targetCount() const
{
    return d->numTargets;
}

void BossBrain::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    // Not for us?
    if(!IS_SERVER) return;

    Writer_WriteByte(writer, 1); // Write a version byte.

    Writer_WriteInt16(writer, d->numTargets);
    Writer_WriteInt16(writer, d->targetOn);
    Writer_WriteByte(writer, d->easy != 0? 1:0);

    // Write the mobj references using the mobj archive.
    for(int i = 0; i < d->numTargets; ++i)
    {
        Writer_WriteInt16(writer, msw->serialIdFor(d->targets[i]));
    }
}

void BossBrain::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    // Not for us?
    if(!IS_SERVER) return;

    // No brain data before version 3.
    if(mapVersion < 3) return;

    clearTargets();

    int ver = (mapVersion >= 8? Reader_ReadByte(reader) : 0);
    int newTargetCount;
    if(ver >= 1)
    {
        newTargetCount = Reader_ReadInt16(reader);

        d->targetOn = Reader_ReadInt16(reader);
        d->easy     = (dd_bool)Reader_ReadByte(reader);
    }
    else
    {
        newTargetCount = Reader_ReadByte(reader);

        d->targetOn = Reader_ReadByte(reader);
        d->easy     = false;
    }

    for(int i = 0; i < newTargetCount; ++i)
    {
        addTarget(msr->mobj((int) Reader_ReadInt16(reader), 0));
    }
}

void BossBrain::addTarget(mobj_t *mo)
{
    DE_ASSERT(mo != 0);

    if(d->numTargets >= d->maxTargets)
    {
        // Do we need to alloc more targets?
        if(d->numTargets == d->maxTargets)
        {
            d->maxTargets *= 2;
            d->targets = (mobj_t **)Z_Realloc(d->targets, d->maxTargets * sizeof(*d->targets), PU_APPSTATIC);
        }
        else
        {
            d->maxTargets = 32;
            d->targets = (mobj_t **)Z_Malloc(d->maxTargets * sizeof(*d->targets), PU_APPSTATIC, NULL);
        }
    }

    d->targets[d->numTargets++] = mo;
}

mobj_t *BossBrain::nextTarget()
{
    if(!d->numTargets)
        return 0;

    d->easy ^= 1;
    if(gfw_Rule(skill) <= SM_EASY && (!d->easy))
        return 0;

    mobj_t *targ = d->targets[d->targetOn++];
    d->targetOn %= d->numTargets;

    return targ;
}

// C wrapper API ---------------------------------------------------------------

void BossBrain_ClearTargets(BossBrain *bb)
{
    DE_ASSERT(bb != 0);
    return bb->clearTargets();
}

int BossBrain_TargetCount(const BossBrain *bb)
{
    DE_ASSERT(bb != 0);
    return bb->targetCount();
}

void BossBrain_AddTarget(BossBrain *bb, mobj_t *mo)
{
    DE_ASSERT(bb != 0);
    return bb->addTarget(mo);
}

mobj_t *BossBrain_NextTarget(BossBrain *bb)
{
    DE_ASSERT(bb != 0);
    return bb->nextTarget();
}
