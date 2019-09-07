/** @file polyobjs.h  Polyobject thinkers and management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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
#include "polyobjs.h"

#include "acs/script.h"
#include "dmu_lib.h"
#include "g_common.h"
#include "p_actor.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_mapsetup.h"
#include "p_start.h"

#define POBJ_PERPETUAL  0xffffffffu  // -1

/// @return  Tag of the found polyobj; otherwise @c 0.
static int findMirrorPolyobj(int tag)
{
#if __JHEXEN__
    for(int i = 0; i < numpolyobjs; ++i)
    {
        Polyobj *po = Polyobj_ById(i);
        if(po->tag == tag)
        {
            return P_ToXLine(Polyobj_FirstLine(po))->arg2;
        }
    }
#else
    DE_UNUSED(tag);
#endif
    return 0;
}

static void startSoundSequence(Polyobj *poEmitter)
{
#if __JHEXEN__
    if (poEmitter)
    {
        SN_StartSequence((mobj_t *)poEmitter, SEQ_DOOR_STONE + poEmitter->seqType);
    }
#else
    DE_UNUSED(poEmitter);
#endif
}

static void stopSoundSequence(Polyobj *poEmitter)
{
#if __JHEXEN__
    SN_StopSequence((mobj_t *)poEmitter);
#else
    DE_UNUSED(poEmitter);
#endif
}

static void PO_SetDestination(Polyobj *po, coord_t dist, uint fineAngle, float speed)
{
    DE_ASSERT(po != 0);
    DE_ASSERT(fineAngle < FINEANGLES);
    po->dest[VX] = po->origin[VX] + dist * FIX2FLT(finecosine[fineAngle]);
    po->dest[VY] = po->origin[VY] + dist * FIX2FLT(finesine[fineAngle]);
    po->speed    = speed;
}

static void PODoor_UpdateDestination(polydoor_t *pd)
{
    DE_ASSERT(pd != 0);
    Polyobj *po = Polyobj_ByTag(pd->polyobj);

    // Only sliding doors need the destination info. (Right? -jk)
    if(pd->type == PODOOR_SLIDE)
    {
        PO_SetDestination(po, FIX2FLT(pd->dist), pd->direction, FIX2FLT(pd->intSpeed));
    }
}

void T_RotatePoly(void *polyThinker)
{
    DE_ASSERT(polyThinker != 0);

    polyevent_t *pe = (polyevent_t *)polyThinker;
    uint absSpeed;
    Polyobj *po = Polyobj_ByTag(pe->polyobj);

    if(Polyobj_Rotate(po, pe->intSpeed))
    {
        absSpeed = abs(pe->intSpeed);

        if(pe->dist == POBJ_PERPETUAL)
        {
            // perpetual polyobj.
            return;
        }

        pe->dist -= absSpeed;

        if(int(pe->dist) <= 0)
        {
            if(po->specialData == pe)
                po->specialData = 0;

            stopSoundSequence(po);
            P_NotifyPolyobjFinished(po->tag);
            Thinker_Remove(&pe->thinker);
            po->angleSpeed = 0;
        }

        if(pe->dist < absSpeed)
        {
            pe->intSpeed = pe->dist * (pe->intSpeed < 0 ? -1 : 1);
        }
    }
}

dd_bool EV_RotatePoly(Line *line, byte *args, int direction, dd_bool overRide)
{
    DE_UNUSED(line);
    DE_ASSERT(args != 0);

    int tag = args[0];
    Polyobj *po = Polyobj_ByTag(tag);
    if(po)
    {
        if(po->specialData && !overRide)
        {   // Poly is already moving, so keep going...
            return false;
        }
    }
    else
    {
        Con_Error("EV_RotatePoly:  Invalid polyobj tag: %d\n", tag);
    }

    polyevent_t *pe = (polyevent_t *)Z_Calloc(sizeof(*pe), PU_MAP, 0);
    pe->thinker.function = T_RotatePoly;
    Thinker_Add(&pe->thinker);

    pe->polyobj = tag;

    if(args[2])
    {
        if(args[2] == 255)
        {
            // Perpetual rotation.
            pe->dist = POBJ_PERPETUAL;
            po->destAngle = POBJ_PERPETUAL;
        }
        else
        {
            pe->dist = args[2] * (ANGLE_90 / 64);   // Angle
            po->destAngle = po->angle + pe->dist * direction;
        }
    }
    else
    {
        pe->dist = ANGLE_MAX - 1;
        po->destAngle = po->angle + pe->dist;
    }

    pe->intSpeed = (args[1] * direction * (ANGLE_90 / 64)) >> 3;
    po->specialData = pe;
    po->angleSpeed = pe->intSpeed;
    startSoundSequence(po);

    int mirror; // tag
    while((mirror = findMirrorPolyobj(tag)) != 0)
    {
        po = Polyobj_ByTag(mirror);
        if(po && po->specialData && !overRide)
        {
            // Mirroring po is already in motion.
            break;
        }

        pe = (polyevent_t *)Z_Calloc(sizeof(*pe), PU_MAP, 0);
        pe->thinker.function = T_RotatePoly;
        Thinker_Add(&pe->thinker);

        po->specialData = pe;
        pe->polyobj = mirror;
        if(args[2])
        {
            if(args[2] == 255)
            {
                pe->dist = POBJ_PERPETUAL;
                po->destAngle = POBJ_PERPETUAL;
            }
            else
            {
                pe->dist = args[2] * (ANGLE_90 / 64); // Angle
                po->destAngle = po->angle + pe->dist * -direction;
            }
        }
        else
        {
            pe->dist = ANGLE_MAX - 1;
            po->destAngle =  po->angle + pe->dist;
        }
        direction = -direction;
        pe->intSpeed = (args[1] * direction * (ANGLE_90 / 64)) >> 3;
        po->angleSpeed = pe->intSpeed;

        po = Polyobj_ByTag(tag);
        if(po)
        {
            po->specialData = pe;
        }
        else
        {
            Con_Error("EV_RotatePoly:  Invalid polyobj num: %d\n", tag);
        }

        tag = mirror;
        startSoundSequence(po);
    }
    return true;
}

void T_MovePoly(void *polyThinker)
{
    DE_ASSERT(polyThinker != 0);

    polyevent_t *pe = (polyevent_t *)polyThinker;
    Polyobj *po = Polyobj_ByTag(pe->polyobj);

    if(Polyobj_MoveXY(po, pe->speed[MX], pe->speed[MY]))
    {
        const uint absSpeed = abs(pe->intSpeed);

        pe->dist -= absSpeed;
        if(int(pe->dist) <= 0)
        {
            if(po->specialData == pe)
                po->specialData = NULL;

            stopSoundSequence(po);
            P_NotifyPolyobjFinished(po->tag);
            Thinker_Remove(&pe->thinker);
            po->speed = 0;
        }

        if(pe->dist < absSpeed)
        {
            pe->intSpeed = pe->dist * (pe->intSpeed < 0 ? -1 : 1);

            pe->speed[MX] = FIX2FLT(FixedMul(pe->intSpeed, finecosine[pe->fangle]));
            pe->speed[MY] = FIX2FLT(FixedMul(pe->intSpeed, finesine[pe->fangle]));
        }
    }
}

void polyevent_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteInt32(writer, polyobj);
    Writer_WriteInt32(writer, intSpeed);
    Writer_WriteUInt32(writer, dist);
    Writer_WriteInt32(writer, fangle);
    Writer_WriteInt32(writer, FLT2FIX(speed[VX]));
    Writer_WriteInt32(writer, FLT2FIX(speed[VY]));
}

int polyevent_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    if(mapVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        // Start of used data members.
        polyobj     = Reader_ReadInt32(reader);
        intSpeed    = Reader_ReadInt32(reader);
        dist        = Reader_ReadUInt32(reader);
        fangle      = Reader_ReadInt32(reader);
        speed[VX]   = FIX2FLT(Reader_ReadInt32(reader));
        speed[VY]   = FIX2FLT(Reader_ReadInt32(reader));
    }
    else
    {
        // Its in the old pre V4 format which serialized polyevent_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        polyobj     = Reader_ReadInt32(reader);
        intSpeed    = Reader_ReadInt32(reader);
        dist        = Reader_ReadUInt32(reader);
        fangle      = Reader_ReadInt32(reader);
        speed[VX]   = FIX2FLT(Reader_ReadInt32(reader));
        speed[VY]   = FIX2FLT(Reader_ReadInt32(reader));
    }

    thinker.function = T_RotatePoly;

    return true; // Add this thinker.
}

void SV_WriteMovePoly(const polyevent_t *th, MapStateWriter *msw)
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteInt32(writer, th->polyobj);
    Writer_WriteInt32(writer, th->intSpeed);
    Writer_WriteUInt32(writer, th->dist);
    Writer_WriteInt32(writer, th->fangle);
    Writer_WriteInt32(writer, FLT2FIX(th->speed[VX]));
    Writer_WriteInt32(writer, FLT2FIX(th->speed[VY]));
}

int SV_ReadMovePoly(polyevent_t *th, MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    if(mapVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        // Start of used data members.
        th->polyobj     = Reader_ReadInt32(reader);
        th->intSpeed    = Reader_ReadInt32(reader);
        th->dist        = Reader_ReadUInt32(reader);
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
        th->dist        = Reader_ReadUInt32(reader);
        th->fangle      = Reader_ReadInt32(reader);
        th->speed[VX]   = FIX2FLT(Reader_ReadInt32(reader));
        th->speed[VY]   = FIX2FLT(Reader_ReadInt32(reader));
    }

    th->thinker.function = T_MovePoly;

    return true; // Add this thinker.
}

dd_bool EV_MovePoly(Line *line, byte *args, dd_bool timesEight, dd_bool override)
{
    DE_UNUSED(line);
    DE_ASSERT(args != 0);

    int tag = args[0];
    Polyobj *po = Polyobj_ByTag(tag);
    DE_ASSERT(po != 0);

    // Already moving?
    if(po->specialData && !override)
        return false;

    polyevent_t *pe = (polyevent_t *)Z_Calloc(sizeof(*pe), PU_MAP, 0);
    pe->thinker.function = T_MovePoly;
    Thinker_Add(&pe->thinker);

    pe->polyobj = tag;
    if(timesEight)
    {
        pe->dist = args[3] * 8 * FRACUNIT;
    }
    else
    {
        pe->dist = args[3] * FRACUNIT;  // Distance
    }
    pe->intSpeed = args[1] * (FRACUNIT / 8);
    po->specialData = pe;

    angle_t angle = args[2] * (ANGLE_90 / 64);

    pe->fangle = angle >> ANGLETOFINESHIFT;
    pe->speed[MX] = FIX2FLT(FixedMul(pe->intSpeed, finecosine[pe->fangle]));
    pe->speed[MY] = FIX2FLT(FixedMul(pe->intSpeed, finesine[pe->fangle]));
    startSoundSequence(po);

    PO_SetDestination(po, FIX2FLT(pe->dist), pe->fangle, FIX2FLT(pe->intSpeed));

    int mirror; // tag
    while((mirror = findMirrorPolyobj(tag)) != 0)
    {
        po = Polyobj_ByTag(mirror);

        // Is the mirror already in motion?
        if(po && po->specialData && !override)
            break;

        pe = (polyevent_t *)Z_Calloc(sizeof(*pe), PU_MAP, 0);
        pe->thinker.function = T_MovePoly;
        Thinker_Add(&pe->thinker);

        pe->polyobj = mirror;
        po->specialData = pe;
        if(timesEight)
        {
            pe->dist = args[3] * 8 * FRACUNIT;
        }
        else
        {
            pe->dist = args[3] * FRACUNIT; // Distance
        }
        pe->intSpeed = args[1] * (FRACUNIT / 8);
        angle = angle + ANGLE_180; // reverse the angle
        pe->fangle = angle >> ANGLETOFINESHIFT;
        pe->speed[MX] = FIX2FLT(FixedMul(pe->intSpeed, finecosine[pe->fangle]));
        pe->speed[MY] = FIX2FLT(FixedMul(pe->intSpeed, finesine[pe->fangle]));
        tag = mirror;
        startSoundSequence(po);

        PO_SetDestination(po, FIX2FLT(pe->dist), pe->fangle, FIX2FLT(pe->intSpeed));
    }
    return true;
}

void T_PolyDoor(void *polyDoorThinker)
{
    DE_ASSERT(polyDoorThinker != 0);

    polydoor_t *pd = (polydoor_t *)polyDoorThinker;
    Polyobj *po = Polyobj_ByTag(pd->polyobj);

    if(pd->tics)
    {
        if(!--pd->tics)
        {
            startSoundSequence(po);

            // Movement is about to begin. Update the destination.
            PODoor_UpdateDestination(pd);
        }
        return;
    }

    switch(pd->type)
    {
    case PODOOR_SLIDE:
        if(Polyobj_MoveXY(po, pd->speed[MX], pd->speed[MY]))
        {
            int absSpeed = abs(pd->intSpeed);
            pd->dist -= absSpeed;
            if(pd->dist <= 0)
            {
                stopSoundSequence(po);
                if(!pd->close)
                {
                    pd->dist = pd->totalDist;
                    pd->close = true;
                    pd->tics = pd->waitTics;
                    pd->direction = (ANGLE_MAX >> ANGLETOFINESHIFT) - pd->direction;
                    pd->speed[MX] = -pd->speed[MX];
                    pd->speed[MY] = -pd->speed[MY];
                }
                else
                {
                    if(po->specialData == pd)
                        po->specialData = NULL;

                    P_NotifyPolyobjFinished(po->tag);
                    Thinker_Remove(&pd->thinker);
                }
            }
        }
        else
        {
            if(po->crush || !pd->close)
            {
                // Continue moving if the po is a crusher, or is opening.
                return;
            }
            else
            {
                // Open back up.
                pd->dist = pd->totalDist - pd->dist;
                pd->direction = (ANGLE_MAX >> ANGLETOFINESHIFT) - pd->direction;
                pd->speed[MX] = -pd->speed[MX];
                pd->speed[MY] = -pd->speed[MY];
                // Update destination.
                PODoor_UpdateDestination(pd);
                pd->close = false;
                startSoundSequence(po);
            }
        }
        break;

    case PODOOR_SWING:
        if(Polyobj_Rotate(po, pd->intSpeed))
        {
            int absSpeed = abs(pd->intSpeed);
            if(pd->dist == -1)
            {
                // Perpetual polyobj.
                return;
            }

            pd->dist -= absSpeed;
            if(pd->dist <= 0)
            {
                stopSoundSequence(po);
                if(!pd->close)
                {
                    pd->dist = pd->totalDist;
                    pd->close = true;
                    pd->tics = pd->waitTics;
                    pd->intSpeed = -pd->intSpeed;
                }
                else
                {
                    if(po->specialData == pd)
                        po->specialData = NULL;

                    P_NotifyPolyobjFinished(po->tag);
                    Thinker_Remove(&pd->thinker);
                }
            }
        }
        else
        {
            if(po->crush || !pd->close)
            {
                // Continue moving if the po is a crusher, or is opening.
                return;
            }
            else
            {
                // Open back up and rewait.
                pd->dist = pd->totalDist - pd->dist;
                pd->intSpeed = -pd->intSpeed;
                pd->close = false;
                startSoundSequence(po);
            }
        }
        break;

    default:
        break;
    }
}

void polydoor_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // Write a version byte.

    Writer_WriteByte(writer, type);

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    Writer_WriteInt32(writer, polyobj);
    Writer_WriteInt32(writer, intSpeed);
    Writer_WriteInt32(writer, dist);
    Writer_WriteInt32(writer, totalDist);
    Writer_WriteInt32(writer, direction);
    Writer_WriteInt32(writer, FLT2FIX(speed[VX]));
    Writer_WriteInt32(writer, FLT2FIX(speed[VY]));
    Writer_WriteInt32(writer, tics);
    Writer_WriteInt32(writer, waitTics);
    Writer_WriteByte(writer, close);
}

int polydoor_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();
    int mapVersion = msr->mapVersion();

    if(mapVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ Reader_ReadByte(reader); // version byte.

        // Start of used data members.
        type        = podoortype_t(Reader_ReadByte(reader));
        polyobj     = Reader_ReadInt32(reader);
        intSpeed    = Reader_ReadInt32(reader);
        dist        = Reader_ReadInt32(reader);
        totalDist   = Reader_ReadInt32(reader);
        direction   = Reader_ReadInt32(reader);
        speed[VX]   = FIX2FLT(Reader_ReadInt32(reader));
        speed[VY]   = FIX2FLT(Reader_ReadInt32(reader));
        tics        = Reader_ReadInt32(reader);
        waitTics    = Reader_ReadInt32(reader);
        close       = Reader_ReadByte(reader);
    }
    else
    {
        // Its in the old pre V4 format which serialized polydoor_t
        // Padding at the start (an old thinker_t struct)
        byte junk[16]; // sizeof thinker_t
        Reader_Read(reader, junk, 16);

        // Start of used data members.
        polyobj     = Reader_ReadInt32(reader);
        intSpeed    = Reader_ReadInt32(reader);
        dist        = Reader_ReadInt32(reader);
        totalDist   = Reader_ReadInt32(reader);
        direction   = Reader_ReadInt32(reader);
        speed[VX]   = FIX2FLT(Reader_ReadInt32(reader));
        speed[VY]   = FIX2FLT(Reader_ReadInt32(reader));
        tics        = Reader_ReadInt32(reader);
        waitTics    = Reader_ReadInt32(reader);
        type        = podoortype_t(Reader_ReadByte(reader));
        close       = Reader_ReadByte(reader);
    }

    thinker.function = T_PolyDoor;

    return true; // Add this thinker.
}

dd_bool EV_OpenPolyDoor(Line *line, byte *args, podoortype_t type)
{
    DE_UNUSED(line);
    DE_ASSERT(args != 0);

    int tag = args[0];
    Polyobj *po = Polyobj_ByTag(tag);
    if(po)
    {
        if(po->specialData)
        {   // Is already moving.
            return false;
        }
    }
    else
    {
        Con_Error("EV_OpenPolyDoor:  Invalid polyobj num: %d\n", tag);
    }

    polydoor_t *pd = (polydoor_t *)Z_Calloc(sizeof(*pd), PU_MAP, 0);
    pd->thinker.function = T_PolyDoor;
    Thinker_Add(&pd->thinker);

    angle_t angle = 0;

    pd->type = type;
    pd->polyobj = tag;
    if(type == PODOOR_SLIDE)
    {
        pd->waitTics = args[4];
        pd->intSpeed = args[1] * (FRACUNIT / 8);
        pd->totalDist = args[3] * FRACUNIT; // Distance.
        pd->dist = pd->totalDist;
        angle = args[2] * (ANGLE_90 / 64);
        pd->direction = angle >> ANGLETOFINESHIFT;
        pd->speed[MX] = FIX2FLT(FixedMul(pd->intSpeed, finecosine[pd->direction]));
        pd->speed[MY] = FIX2FLT(FixedMul(pd->intSpeed, finesine[pd->direction]));
        startSoundSequence(po);
    }
    else if(type == PODOOR_SWING)
    {
        pd->waitTics = args[3];
        pd->direction = 1;
        pd->intSpeed = (args[1] * pd->direction * (ANGLE_90 / 64)) >> 3;
        pd->totalDist = args[2] * (ANGLE_90 / 64);
        pd->dist = pd->totalDist;
        startSoundSequence(po);
    }

    po->specialData = pd;
    PODoor_UpdateDestination(pd);

    int mirror; // tag
    while((mirror = findMirrorPolyobj(tag)) != 0)
    {
        po = Polyobj_ByTag(mirror);
        if(po && po->specialData)
        {
            // Mirroring po is already in motion.
            break;
        }

        pd = (polydoor_t *)Z_Calloc(sizeof(*pd), PU_MAP, 0);
        pd->thinker.function = T_PolyDoor;
        Thinker_Add(&pd->thinker);

        pd->polyobj = mirror;
        pd->type = type;
        po->specialData = pd;
        if(type == PODOOR_SLIDE)
        {
            pd->waitTics = args[4];
            pd->intSpeed = args[1] * (FRACUNIT / 8);
            pd->totalDist = args[3] * FRACUNIT; // Distance.
            pd->dist = pd->totalDist;
            angle = angle + ANGLE_180; // Reverse the angle.
            pd->direction = angle >> ANGLETOFINESHIFT;
            pd->speed[MX] = FIX2FLT(FixedMul(pd->intSpeed, finecosine[pd->direction]));
            pd->speed[MY] = FIX2FLT(FixedMul(pd->intSpeed, finesine[pd->direction]));
            startSoundSequence(po);
        }
        else if(type == PODOOR_SWING)
        {
            pd->waitTics = args[3];
            pd->direction = -1;
            pd->intSpeed = (args[1] * pd->direction * (ANGLE_90 / 64)) >> 3;
            pd->totalDist = args[2] * (ANGLE_90 / 64);
            pd->dist = pd->totalDist;
            startSoundSequence(po);
        }
        tag = mirror;
        PODoor_UpdateDestination(pd);
    }

    return true;
}

static void thrustMobj(struct mobj_s *mo, void *linep, void *pop)
{
    Line *line = (Line *) linep;
    Polyobj *po = (Polyobj *) pop;
    coord_t thrust[2], force;
    polyevent_t *pe;
    uint thrustAn;

    // Clients do no polyobj <-> mobj interaction.
    if(IS_CLIENT) return;

    // Cameras don't interact with polyobjs.
    if(P_MobjIsCamera(mo)) return;

    if(!(mo->flags & MF_SHOOTABLE) && !mo->player) return;

    thrustAn = (P_GetAnglep(line, DMU_ANGLE) - ANGLE_90) >> ANGLETOFINESHIFT;

    pe = (polyevent_t *) po->specialData;
    if(pe)
    {
        if(pe->thinker.function == (thinkfunc_t) T_RotatePoly)
        {
            force = FIX2FLT(pe->intSpeed >> 8);
        }
        else
        {
            force = FIX2FLT(pe->intSpeed >> 3);
        }

        force = MINMAX_OF(1, force, 4);
    }
    else
    {
        force = 1;
    }

    thrust[VX] = force * FIX2FLT(finecosine[thrustAn]);
    thrust[VY] = force * FIX2FLT(finesine[thrustAn]);
    mo->mom[MX] += thrust[VX];
    mo->mom[MY] += thrust[VY];

    if(po->crush)
    {
        if(!P_CheckPositionXY(mo, mo->origin[VX] + thrust[VX], mo->origin[VY] + thrust[VY]))
        {
            P_DamageMobj(mo, NULL, NULL, 3, false);
        }
    }
}

void PO_InitForMap()
{
#if !defined(__JHEXEN__)
    return; // Disabled -- awaiting line argument translation.
#endif

    App_Log(DE2_DEV_MAP_VERBOSE, "Initializing polyobjects for map...");

    // thrustMobj will handle polyobj <-> mobj interaction.
    Polyobj_SetCallback(thrustMobj);

    for(int i = 0; i < numpolyobjs; ++i)
    {
        Polyobj *po = Polyobj_ById(i);

        // Init game-specific properties.
        po->specialData = NULL;

        // Find the mapspot associated with this polyobj.
        uint j = 0;
        const mapspot_t *spot = 0;
        while(j < numMapSpots && !spot)
        {
            if((mapSpots[j].doomEdNum == PO_SPAWN_DOOMEDNUM ||
                mapSpots[j].doomEdNum == PO_SPAWNCRUSH_DOOMEDNUM) &&
                mapSpots[j].angle     == angle_t(po->tag))
            {
                // Polyobj mapspot.
                spot = &mapSpots[j];
            }
            else
            {
                j++;
            }
        }

        if(spot)
        {
            po->crush = (spot->doomEdNum == PO_SPAWNCRUSH_DOOMEDNUM? 1 : 0);
            Polyobj_MoveXY(po, -po->origin[VX] + spot->origin[VX],
                               -po->origin[VY] + spot->origin[VY]);
        }
        else
        {
            App_Log(DE2_MAP_WARNING, "Missing spawn spot for PolyObj #%i", i);
        }
    }
}

dd_bool PO_Busy(int tag)
{
    Polyobj *po = Polyobj_ByTag(tag);
    return (po && po->specialData);
}

void polyobj_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();

    Writer_WriteByte(writer, 1); // write a version byte (unused).

    Writer_WriteInt32(writer, tag);
    Writer_WriteInt32(writer, angle);
    Writer_WriteInt32(writer, FLT2FIX(origin[VX]));
    Writer_WriteInt32(writer, FLT2FIX(origin[VY]));
}

int polyobj_s::read(MapStateReader *msr)
{
    Reader1 *reader = msr->reader();

    angle_t newAngle = angle_t(Reader_ReadInt32(reader));
    Polyobj_Rotate(this, newAngle);
    destAngle = newAngle;

    coord_t newOrigin[2];
    newOrigin[0] = FIX2FLT(Reader_ReadInt32(reader));
    newOrigin[1] = FIX2FLT(Reader_ReadInt32(reader));
    Polyobj_MoveXY(this, newOrigin[0] - origin[0], newOrigin[1] - origin[1]);

    /// @todo What about speed? It isn't saved at all?

    return true;
}
