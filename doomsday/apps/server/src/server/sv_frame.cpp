/** @file sv_frame.cpp  Frame Generation and Transmission.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "server/sv_frame.h"
#include "def_main.h"
#include "sys_system.h"
#include "network/net_main.h"
#include "server/sv_pool.h"
#include "world/p_players.h"

#include <de/logbuffer.h>
#include <cmath>

using namespace de;

// Hitting the maximum packet size allows checks for raising BWR.
#define BWR_ADJUST_TICS     (TICSPERSEC / 2)

// The minimum frame size is used when bandwidth rating is zero (poorest
// possible connection).
#define MINIMUM_FRAME_SIZE  1800 // bytes

// The first frame should contain as much information as possible.
#define MAX_FIRST_FRAME_SIZE    64000

// The frame size is calculated by multiplying the bandwidth rating
// (max 100) with this factor (+min).
#define FRAME_SIZE_FACTOR   13
#define FIXED8_8(x)         (((x)*256) >> 16)
#define FIXED10_6(x)        (((x)*64) >> 16)
#define CLAMPED_CHAR(x)     ((x)>127? 127 : (x)<-128? -128 : (x))

// If movement is faster than this, we'll adjust the place of the point.
#define MOM_FAST_LIMIT      (127)

void Sv_SendFrame(dint playerNumber);

dint allowFrames;
dint frameInterval = 1;  ///< Skip every second frame by default (17.5fps)

#ifdef DE_DEBUG
static dint byteCounts[256];
static dint totalFrameCount;
#endif

static dint lastTransmitTic;

/**
 * Send all the relevant information to each client.
 */
void Sv_TransmitFrame()
{
    // Obviously clients don't transmit anything.
    if (!::allowFrames || ::isClient || Sys_IsShuttingDown())
    {
        return;
    }

    if (!::netGame)
    {
        // Only generate deltas when somebody is recording a demo.
        dint i = 0;
        for (; i < DDMAXPLAYERS; ++i)
        {
            if (Sv_IsFrameTarget(i))
                break;
        }
        if (i == DDMAXPLAYERS) return;  // Nobody is a frame target.
    }

    if (SECONDS_TO_TICKS(::gameTime) == ::lastTransmitTic)
    {
        // We were just here!
        return;
    }
    ::lastTransmitTic = SECONDS_TO_TICKS(::gameTime);

    LOG_AS("Sv_TransmitFrame");

    // Generate new deltas for the frame.
    Sv_GenerateFrameDeltas();

    // How many players currently in the game?
    const dint numInGame = Sv_GetNumPlayers();

    dint pCount = 0;
    for (dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        auto &plr = *DD_Player(i);

        if (!Sv_IsFrameTarget(i))
        {
            // This player is not a valid target for frames.
            continue;
        }

        // When the interval is greater than zero, this causes the frames
        // to be sent at different times for each player.
        pCount++;
        dint cTime = SECONDS_TO_TICKS(::gameTime);
        if (::frameInterval > 0 && numInGame > 1)
        {
            cTime += (pCount * ::frameInterval) / numInGame;
        }
        if (cTime <= plr.lastTransmit + ::frameInterval)
        {
            // Still too early to send.
            continue;
        }
        plr.lastTransmit = cTime;

        if (plr.ready)
        {
            // A frame will be sent to this client. If the client
            // doesn't send ticcmds, the updatecount will eventually
            // decrease back to zero.
            //::clients[i].updateCount--;

            Sv_SendFrame(i);
        }
        else
        {
            LOG_NET_XVERBOSE("NOT sending at tic %i to plr %i (ready:%b)",
                             ::lastTransmitTic << i << plr.ready);
        }
    }
}

/**
 * Shutdown routine for the server.
 */
void Sv_Shutdown()
{
#ifdef DE_DEBUG
    if (::totalFrameCount > 0)
    {
        // Byte probabilities.
        for (dint i = 0; i < 256; ++i)
        {
            LOGDEV_NET_NOTE("Byte %02x: %f")
                << i << (byteCounts[i] / dfloat( totalFrameCount ));
        }
    }
#endif

    Sv_ShutdownPools();
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WriteMobjDelta(const void* deltaPtr)
{
    const auto *delta  = reinterpret_cast<const mobjdelta_t *>(deltaPtr);
    const dt_mobj_t *d = &delta->mo;
    dint df            = delta->delta.flags;

    byte moreFlags = 0;

    // Do we have fast momentum?
    if (fabs(d->mom[MX]) >= MOM_FAST_LIMIT ||
       fabs(d->mom[MY]) >= MOM_FAST_LIMIT ||
       fabs(d->mom[MZ]) >= MOM_FAST_LIMIT)
    {
        df |= MDF_MORE_FLAGS;
        moreFlags |= MDFE_FAST_MOM;
    }

    // Any translucency?
    if (df & MDFC_TRANSLUCENCY)
    {
        df |= MDF_MORE_FLAGS;
        moreFlags |= MDFE_TRANSLUCENCY;
    }

    // A fade target?
    if (df & MDFC_FADETARGET)
    {
        df |= MDF_MORE_FLAGS;
        moreFlags |= MDFE_FADETARGET;
    }

    // On the floor?
    if (df & MDFC_ON_FLOOR)
    {
        df |= MDF_MORE_FLAGS;
        moreFlags |= MDFE_Z_FLOOR;
    }

    // Mobj type?
    if (df & MDFC_TYPE)
    {
        df |= MDF_MORE_FLAGS;
        moreFlags |= MDFE_TYPE;
    }

    // Flags. What elements are included in the delta?
    if (d->selector & ~DDMOBJ_SELECTOR_MASK)
        df |= MDF_SELSPEC;

    // Omit NULL state.
    if (!d->state)
    {
        df &= ~MDF_STATE;
    }

    /*
    // Floor/ceiling z?
    if (df & MDF_ORIGIN_Z)
    {
        if (d->pos[VZ] == DDMINFLOAT || d->pos[VZ] == DDMAXFLOAT)
        {
            df &= ~MDF_ORIGIN_Z;
            df |= MDF_MORE_FLAGS;
            moreFlags |= (d->pos[VZ] == DDMINFLOAT ? MDFE_Z_FLOOR : MDFE_Z_CEILING);
        }
    }
    */

    DE_ASSERT(!(df & MDFC_NULL));     // don't write NULL deltas
    DE_ASSERT((df & 0xffff) != 0);    // don't write empty deltas

    // First the mobj ID number and flags.
    Writer_WriteUInt16(::msgWriter, delta->delta.id);
    Writer_WriteUInt16(::msgWriter, df & 0xffff);

    // More flags?
    if (df & MDF_MORE_FLAGS)
    {
        Writer_WriteByte(::msgWriter, moreFlags);
    }

    // Coordinates with three bytes.
    if (df & MDF_ORIGIN_X)
    {
        fixed_t vx = FLT2FIX(d->origin[VX]);

        Writer_WriteInt16(::msgWriter, vx >> FRACBITS);
        Writer_WriteByte(::msgWriter, vx >> 8);
    }
    if (df & MDF_ORIGIN_Y)
    {
        fixed_t vy = FLT2FIX(d->origin[VY]);

        Writer_WriteInt16(::msgWriter, vy >> FRACBITS);
        Writer_WriteByte(::msgWriter, vy >> 8);
    }

    if (df & MDF_ORIGIN_Z)
    {
        fixed_t vz = FLT2FIX(d->origin[VZ]);
        Writer_WriteInt16(::msgWriter, vz >> FRACBITS);
        Writer_WriteByte(::msgWriter, vz >> 8);

        Writer_WriteFloat(::msgWriter, d->floorZ);
        Writer_WriteFloat(::msgWriter, d->ceilingZ);
    }

    // Momentum using 8.8 fixed point.
    if (df & MDF_MOM_X)
    {
        fixed_t mx = FLT2FIX(d->mom[MX]);
        Writer_WriteInt16(::msgWriter, moreFlags & MDFE_FAST_MOM ? FIXED10_6(mx) : FIXED8_8(mx));
    }

    if (df & MDF_MOM_Y)
    {
        fixed_t my = FLT2FIX(d->mom[MY]);
        Writer_WriteInt16(::msgWriter, moreFlags & MDFE_FAST_MOM ? FIXED10_6(my) : FIXED8_8(my));
    }

    if (df & MDF_MOM_Z)
    {
        fixed_t mz = FLT2FIX(d->mom[MZ]);
        Writer_WriteInt16(::msgWriter, moreFlags & MDFE_FAST_MOM ? FIXED10_6(mz) : FIXED8_8(mz));
    }

    // Angles with 16-bit accuracy.
    if (df & MDF_ANGLE)
        Writer_WriteInt16(::msgWriter, d->angle >> 16);

    if (df & MDF_SELECTOR)
        Writer_WritePackedUInt16(::msgWriter, d->selector);
    if (df & MDF_SELSPEC)
        Writer_WriteByte(::msgWriter, d->selector >> 24);

    if (df & MDF_STATE)
    {
        DE_ASSERT(d->state != 0);
        Writer_WritePackedUInt16(::msgWriter, ::runtimeDefs.states.indexOf(d->state));
    }

    if (df & MDF_FLAGS)
    {
        Writer_WriteUInt32(::msgWriter, d->ddFlags & DDMF_PACK_MASK);
        Writer_WriteUInt32(::msgWriter, d->flags);
        Writer_WriteUInt32(::msgWriter, d->flags2);
        Writer_WriteUInt32(::msgWriter, d->flags3);
    }

    if (df & MDF_HEALTH)
        Writer_WriteInt32(::msgWriter, d->health);

    if (df & MDF_RADIUS)
        Writer_WriteFloat(::msgWriter, d->radius);

    if (df & MDF_HEIGHT)
        Writer_WriteFloat(::msgWriter, d->height);

    if (df & MDF_FLOORCLIP)
        Writer_WriteFloat(::msgWriter, d->floorClip);

    if (df & MDFC_TRANSLUCENCY)
        Writer_WriteByte(::msgWriter, d->translucency);

    if (df & MDFC_FADETARGET)
        Writer_WriteByte(::msgWriter, byte( d->visTarget + 1 ));

    if (df & MDFC_TYPE)
        Writer_WriteInt32(::msgWriter, d->type);
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WritePlayerDelta(const void *deltaPtr)
{
    const auto *delta    = reinterpret_cast<const playerdelta_t *>(deltaPtr);
    const dt_player_t *d = &delta->player;
    dint df              = delta->delta.flags;

    // First the player number. Upper three bits contain flags.
    Writer_WriteByte(::msgWriter, delta->delta.id | (df >> 8));

    // Flags. What elements are included in the delta?
    Writer_WriteByte(::msgWriter, df & 0xff);

    if (df & PDF_MOBJ)
        Writer_WriteUInt16(::msgWriter, d->mobj);
    if (df & PDF_FORWARDMOVE)
        Writer_WriteByte(::msgWriter, d->forwardMove);
    if (df & PDF_SIDEMOVE)
        Writer_WriteByte(::msgWriter, d->sideMove);
    /*if (df & PDF_ANGLE)
        Writer_WriteByte(::msgWriter, d->angle >> 24);*/
    if (df & PDF_TURNDELTA)
        Writer_WriteByte(::msgWriter, (d->turnDelta * 16) >> 24);
    if (df & PDF_FRICTION)
        Writer_WriteByte(::msgWriter, FLT2FIX(d->friction) >> 8);
    if (df & PDF_EXTRALIGHT)
    {
        // Three bits is enough for fixedcolormap.
        const dint cmap = de::clamp(0, d->fixedColorMap, 7);
        // Write the five upper bytes of extraLight.
        Writer_WriteByte(::msgWriter, cmap | (d->extraLight & 0xf8));
    }
    if (df & PDF_FILTER)
    {
        Writer_WriteUInt32(::msgWriter, d->filter);
        LOGDEV_NET_XVERBOSE_DEBUGONLY("Sv_WritePlayerDelta: Plr %i, filter %08x", delta->delta.id << d->filter);
    }
    if (df & PDF_PSPRITES)       // Only set if there's something to write.
    {
        for (dint i = 0; i < 2; ++i)
        {
            const ddpsprite_t &psp = d->psp[i];
            const dint flags       = df >> (16 + i * 8);

            // First the flags.
            Writer_WriteByte(::msgWriter, flags);
            if (flags & PSDF_STATEPTR)
            {
                Writer_WritePackedUInt16(::msgWriter, psp.statePtr ? (::runtimeDefs.states.indexOf(psp.statePtr) + 1) : 0);
            }
            /*if (flags & PSDF_LIGHT)
            {
                const dint light = de::clamp(0, psp.light * 255, 255);
                Writer_WriteByte(::msgWriter, light);
            }*/
            if (flags & PSDF_ALPHA)
            {
                const dint alpha = de::clamp(0.f, psp.alpha * 255, 255.f);
                Writer_WriteByte(::msgWriter, alpha);
            }
            if (flags & PSDF_STATE)
            {
                Writer_WriteByte(::msgWriter, psp.state);
            }
            if (flags & PSDF_OFFSET)
            {
                Writer_WriteByte(::msgWriter, CLAMPED_CHAR(psp.offset[VX] / 2));
                Writer_WriteByte(::msgWriter, CLAMPED_CHAR(psp.offset[VY] / 2));
            }
        }
    }
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WriteSectorDelta(const void *deltaPtr)
{
    const auto *delta    = reinterpret_cast<const sectordelta_t *>(deltaPtr);
    const dt_sector_t *d = &delta->sector;
    dint              df = delta->delta.flags;

    // Is there need to use 4.4 fixed-point speeds?
    // (7.1 is too inaccurate for very slow movement)
    byte floorSpd = 0;
    if (df & SDF_FLOOR_SPEED)
    {
        const dint spd = FLT2FIX(fabs(d->planes[PLN_FLOOR].speed));
        floorSpd = spd >> 15;
        if (!floorSpd)
        {
            df |= SDF_FLOOR_SPEED_44;
            floorSpd = spd >> 12;
        }
    }
    byte ceilSpd = 0;
    if (df & SDF_CEILING_SPEED)
    {
        const dint spd = FLT2FIX(fabs(d->planes[PLN_CEILING].speed));
        ceilSpd = spd >> 15;
        if (!ceilSpd)
        {
            df |= SDF_CEILING_SPEED_44;
            ceilSpd = spd >> 12;
        }
    }

    // Sector number first.
    Writer_WriteUInt16(::msgWriter, delta->delta.id);

    // Flags.
    Writer_WritePackedUInt32(::msgWriter, df);

    if (df & SDF_FLOOR_MATERIAL)
        Writer_WritePackedUInt16(::msgWriter, Sv_IdForMaterial(d->planes[PLN_FLOOR].surface.material));
    if (df & SDF_CEILING_MATERIAL)
        Writer_WritePackedUInt16(::msgWriter, Sv_IdForMaterial(d->planes[PLN_CEILING].surface.material));
    if (df & SDF_LIGHT)
    {
        // Must fit into a byte.
        auto lightlevel = dint( 255.0f * d->lightLevel );
        lightlevel = (lightlevel < 0 ? 0 : lightlevel > 255 ? 255 : lightlevel);

        Writer_WriteByte(::msgWriter, byte( lightlevel ));
    }
    if (df & SDF_FLOOR_HEIGHT)
    {
        Writer_WriteInt16(::msgWriter, FLT2FIX(d->planes[PLN_FLOOR].height) >> 16);
    }
    if (df & SDF_CEILING_HEIGHT)
    {
        LOGDEV_NET_XVERBOSE_DEBUGONLY("Sv_WriteSectorDelta: (%i) Absolute ceiling height=%f",
                                     delta->delta.id << d->planes[PLN_CEILING].height);

        Writer_WriteInt16(::msgWriter, FLT2FIX(d->planes[PLN_CEILING].height) >> 16);
    }
    if (df & SDF_FLOOR_TARGET)
        Writer_WriteInt16(::msgWriter, FLT2FIX(d->planes[PLN_FLOOR].target) >> 16);
    if (df & SDF_FLOOR_SPEED)    // 7.1/4.4 fixed-point
        Writer_WriteByte(::msgWriter, floorSpd);
    if (df & SDF_CEILING_TARGET)
        Writer_WriteInt16(::msgWriter, FLT2FIX(d->planes[PLN_CEILING].target) >> 16);
    if (df & SDF_CEILING_SPEED)  // 7.1/4.4 fixed-point
        Writer_WriteByte(::msgWriter, ceilSpd);
    if (df & SDF_COLOR_RED)
        Writer_WriteByte(::msgWriter, byte( 255 * d->rgb[0] ));
    if (df & SDF_COLOR_GREEN)
        Writer_WriteByte(::msgWriter, byte( 255 * d->rgb[1] ));
    if (df & SDF_COLOR_BLUE)
        Writer_WriteByte(::msgWriter, byte( 255 * d->rgb[2] ));

    if (df & SDF_FLOOR_COLOR_RED)
        Writer_WriteByte(::msgWriter, byte( 255 * d->planes[PLN_FLOOR].surface.rgba[0] ));
    if (df & SDF_FLOOR_COLOR_GREEN)
        Writer_WriteByte(::msgWriter, byte( 255 * d->planes[PLN_FLOOR].surface.rgba[1] ));
    if (df & SDF_FLOOR_COLOR_BLUE)
        Writer_WriteByte(::msgWriter, byte( 255 * d->planes[PLN_FLOOR].surface.rgba[2] ));

    if (df & SDF_CEIL_COLOR_RED)
        Writer_WriteByte(::msgWriter, byte( 255 * d->planes[PLN_CEILING].surface.rgba[0] ));
    if (df & SDF_CEIL_COLOR_GREEN)
        Writer_WriteByte(::msgWriter, byte( 255 * d->planes[PLN_CEILING].surface.rgba[1] ));
    if (df & SDF_CEIL_COLOR_BLUE)
        Writer_WriteByte(::msgWriter, byte( 255 * d->planes[PLN_CEILING].surface.rgba[2] ));
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WriteSideDelta(const void *deltaPtr)
{
    const auto *delta  = (const sidedelta_t *) deltaPtr;
    const dt_side_t *d = &delta->side;
    dint            df = delta->delta.flags;

    // Side number first.
    Writer_WriteUInt16(::msgWriter, delta->delta.id);

    // Flags.
    Writer_WritePackedUInt32(::msgWriter, df);

    if (df & SIDF_TOP_MATERIAL)
        Writer_WritePackedUInt16(::msgWriter, Sv_IdForMaterial(d->top.material));
    if (df & SIDF_MID_MATERIAL)
        Writer_WritePackedUInt16(::msgWriter, Sv_IdForMaterial(d->middle.material));
    if (df & SIDF_BOTTOM_MATERIAL)
        Writer_WritePackedUInt16(::msgWriter, Sv_IdForMaterial(d->bottom.material));

    if (df & SIDF_LINE_FLAGS)
        Writer_WriteByte(::msgWriter, d->lineFlags);

    if (df & SIDF_TOP_COLOR_RED)
        Writer_WriteByte(::msgWriter, byte( 255 * d->top.rgba[0] ));
    if (df & SIDF_TOP_COLOR_GREEN)
        Writer_WriteByte(::msgWriter, byte( 255 * d->top.rgba[1] ));
    if (df & SIDF_TOP_COLOR_BLUE)
        Writer_WriteByte(::msgWriter, byte( 255 * d->top.rgba[2] ));

    if (df & SIDF_MID_COLOR_RED)
        Writer_WriteByte(::msgWriter, byte( 255 * d->middle.rgba[0] ));
    if (df & SIDF_MID_COLOR_GREEN)
        Writer_WriteByte(::msgWriter, byte( 255 * d->middle.rgba[1] ));
    if (df & SIDF_MID_COLOR_BLUE)
        Writer_WriteByte(::msgWriter, byte( 255 * d->middle.rgba[2] ));
    if (df & SIDF_MID_COLOR_ALPHA)
        Writer_WriteByte(::msgWriter, byte( 255 * d->middle.rgba[3] ));

    if (df & SIDF_BOTTOM_COLOR_RED)
        Writer_WriteByte(::msgWriter, byte( 255 * d->bottom.rgba[0] ));
    if (df & SIDF_BOTTOM_COLOR_GREEN)
        Writer_WriteByte(::msgWriter, byte( 255 * d->bottom.rgba[1] ));
    if (df & SIDF_BOTTOM_COLOR_BLUE)
        Writer_WriteByte(::msgWriter, byte( 255 * d->bottom.rgba[2] ));

    if (df & SIDF_MID_BLENDMODE)
        Writer_WriteInt32(::msgWriter, d->middle.blendMode);

    if (df & SIDF_FLAGS)
        Writer_WriteByte(::msgWriter, d->flags);
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WritePolyDelta(const void *deltaPtr)
{
    const auto *delta = (const polydelta_t *) deltaPtr;
    const dt_poly_t *d = &delta->po;
    dint            df = delta->delta.flags;

    if (d->destAngle == (unsigned) -1)
    {
        // Send Perpetual Rotate instead of Dest Angle flag.
        df |= PODF_PERPETUAL_ROTATE;
        df &= ~PODF_DEST_ANGLE;
    }

    // Poly number first.
    Writer_WritePackedUInt16(::msgWriter, delta->delta.id);

    // Flags.
    Writer_WriteByte(::msgWriter, df & 0xff);

    if (df & PODF_DEST_X)
        Writer_WriteFloat(::msgWriter, d->dest[VX]);
    if (df & PODF_DEST_Y)
        Writer_WriteFloat(::msgWriter, d->dest[VY]);
    if (df & PODF_SPEED)
        Writer_WriteFloat(::msgWriter, d->speed);
    if (df & PODF_DEST_ANGLE)
        Writer_WriteInt16(::msgWriter, d->destAngle >> 16);
    if (df & PODF_ANGSPEED)
        Writer_WriteInt16(::msgWriter, d->angleSpeed >> 16);
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WriteSoundDelta(const void *deltaPtr)
{
    const auto *delta = (const sounddelta_t *) deltaPtr;
    dint           df = delta->delta.flags;

    // This is either the sound ID, emitter ID or sector index.
    Writer_WriteUInt16(::msgWriter, delta->delta.id);

    // First the flags byte.
    Writer_WriteByte(::msgWriter, df & 0xff);

    switch (delta->delta.type)
    {
    case DT_MOBJ_SOUND:
    case DT_SECTOR_SOUND:
    case DT_SIDE_SOUND:
    case DT_POLY_SOUND:
        // The sound ID.
        Writer_WriteUInt16(::msgWriter, delta->sound);
        break;

    default: break;
    }

    // The common parts.
    if (df & SNDDF_VOLUME)
    {
        if (delta->volume > 1)
        {
            // Very loud indeed.
            Writer_WriteByte(::msgWriter, 255);
        }
        else if (delta->volume <= 0)
        {
            // Silence.
            Writer_WriteByte(::msgWriter, 0);
        }
        else
        {
            Writer_WriteByte(::msgWriter, delta->volume * 127 + 0.5f);
        }
    }
}

/**
 * Write the type and possibly the set number (for Unacked deltas).
 */
void Sv_WriteDeltaHeader(byte type, const delta_t *delta)
{
#ifdef DE_DEBUG
    if (type >= NUM_DELTA_TYPES)
    {
        App_Error("Sv_WriteDeltaHeader: Invalid delta type %i.\n", type);
    }
#endif

    // Once sent, the deltas can be discarded and there is no need for resending.
    DE_ASSERT(delta->state != DELTA_UNACKED);

    if (delta->state == DELTA_UNACKED)
    {
        DE_ASSERT_FAIL("Unacked");
        // Flag this as Resent.
        type |= DT_RESENT;
    }

    Writer_WriteByte(::msgWriter, type);

    // Include the set number?
    if (type & DT_RESENT)
    {
        // The client will use this to avoid dupes. If the client has already
        // received the set this delta belongs to, it means the delta has
        // already been received. This is needed in the situation where the
        // ack is lost or delayed.
        Writer_WriteByte(::msgWriter, delta->set);

        // Also send the unique ID of this delta. If the client has already
        // received a delta with this ID, the delta is discarded. This is
        // needed in the situation where the set is lost.
        Writer_WriteByte(::msgWriter, delta->resend);
    }
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WriteDelta(const delta_t *delta)
{
    DE_ASSERT(delta);

#ifdef _NETDEBUG
    // Extra length field in debug builds.
    const dint lengthOffset = Msg_Offset();
    Msg_WriteLong(0);
#endif

    // Null mobj deltas are special.
    if (delta->type == DT_MOBJ)
    {
        if (delta->flags & MDFC_NULL)
        {
            // This'll be the entire delta. No more data is needed.
            Sv_WriteDeltaHeader(DT_NULL_MOBJ, delta);
            Writer_WriteUInt16(::msgWriter, delta->id);
#ifdef _NETDEBUG
            goto writeDeltaLength;
#else
            return;
#endif
        }
    }

    // First the type of the delta.
    Sv_WriteDeltaHeader(delta->type, delta);

    switch (delta->type)
    {
    //case DT_LUMP:   Sv_WriteLumpDelta(delta);   break;

    case DT_MOBJ:   Sv_WriteMobjDelta(delta);   break;
    case DT_PLAYER: Sv_WritePlayerDelta(delta); break;
    case DT_SECTOR: Sv_WriteSectorDelta(delta); break;
    case DT_SIDE:   Sv_WriteSideDelta(delta);   break;
    case DT_POLY:   Sv_WritePolyDelta(delta);   break;

    case DT_SOUND:
    case DT_MOBJ_SOUND:
    case DT_SECTOR_SOUND:
    case DT_SIDE_SOUND:
    case DT_POLY_SOUND:
        Sv_WriteSoundDelta(delta);
        break;

    default: App_Error("Sv_WriteDelta: Unknown delta type %i.\n", delta->type);
    }

#ifdef _NETDEBUG
writeDeltaLength:
    // Update the length of the delta.
    dint endOffset = Msg_Offset();
    Msg_SetOffset(lengthOffset);
    Msg_WriteLong(endOffset - lengthOffset);
    Msg_SetOffset(endOffset);
#endif
}

/**
 * Returns an estimate for the maximum frame size appropriate for the client.
 * The bandwidth rating is updated whenever a frame is sent.
 */
dsize Sv_GetMaxFrameSize(dint playerNumber)
{
    DE_UNUSED(playerNumber);
    DE_ASSERT(playerNumber >= 0 && playerNumber < DDMAXPLAYERS);
    dsize size = MINIMUM_FRAME_SIZE + FRAME_SIZE_FACTOR * 40 /* BWR_DEFAULT */;

    // What about the communications medium?
    if (size > PROTOCOL_MAX_DATAGRAM_SIZE)
        size = PROTOCOL_MAX_DATAGRAM_SIZE;

    return size;
}

/**
 * @return A unique resend ID. Never returns zero.
 */
byte Sv_GetNewResendID(pool_t *pool)
{
    DE_ASSERT(pool);

    byte id = pool->resendDealer;
    // Advance to next ID, skipping zero.
    while (!++pool->resendDealer) {}
    return id;
}

/**
 * Send a sv_frame packet to the specified player. The amount of data sent
 * depends on the player's bandwidth rating.
 */
void Sv_SendFrame(dint plrNum)
{
    pool_t *pool = Sv_GetPool(plrNum);

    // Does the send queue allow us to send this packet?
    // Bandwidth rating is updated during the check.
    if (!Sv_CheckBandwidth(plrNum))
    {
        // We cannot send anything at this time. This will only happen if
        // the send queue has too many packets waiting to be sent.
        return;
    }

    // The priority queue of the client needs to be rebuilt before
    // a new frame can be sent.
    Sv_RatePool(pool);

    // This will be a new set.
    DE_ASSERT(pool);
    pool->setDealer++;

    // Determine the maximum size of the frame packet.
    dsize maxFrameSize = Sv_GetMaxFrameSize(plrNum);
    if (pool->isFirst)
    {
        // Allow more info for the first frame.
        maxFrameSize = MAX_FIRST_FRAME_SIZE;
    }

    // If this is the first frame after a map change, use the special
    // first frame packet type.
    Msg_Begin(pool->isFirst ? PSV_FIRST_FRAME2 : PSV_FRAME2);

    // First send the gameTime of this frame.
    Writer_WriteFloat(::msgWriter, ::gameTime);

    // Keep writing until the maximum size is reached.
    delta_t *delta;
    size_t lastStart;
    while ((delta = Sv_PoolQueueExtract(pool)) != nullptr &&
          (lastStart = Writer_Size(::msgWriter)) < maxFrameSize)
    {
        const byte oldResend = pool->resendDealer;

        // Is this going to be a resent?
        if (delta->state == DELTA_UNACKED && !delta->resend)
        {
            // Assign a new unique ID for this delta.
            // This ID won't be changed after this.
            delta->resend = Sv_GetNewResendID(pool);
        }

        Sv_WriteDelta(delta);

        // Did we go over the limit?
        if (Writer_Size(::msgWriter) > maxFrameSize)
        {
            /*
            // Time to see if BWR needs to be adjusted.
            if (::clients[plrNum].bwrAdjustTime <= 0)
            {
                ::clients[plrNum].bwrAdjustTime = BWR_ADJUST_TICS;
            }
            */

            // Cancel the last delta.
            Writer_SetPos(::msgWriter, lastStart);

            // Restore the resend dealer.
            if (oldResend)
                pool->resendDealer = oldResend;
            break;
        }

        // Successfully written.
        // Update the sent delta's state.
        if (delta->state == DELTA_NEW)
        {
            // New deltas are assigned to this set. Unacked deltas will
            // remain in the set they were initially sent in.
            delta->set       = pool->setDealer;
            delta->timeStamp = Sv_GetTimeStamp();
            delta->state     = DELTA_UNACKED;
        }
    }

    Msg_End();

    Net_SendBuffer(plrNum, 0);

    // Once sent, the delta set can be discarded.
    Sv_AckDeltaSet(plrNum, pool->setDealer, 0);

    // Now a frame has been sent.
    pool->isFirst = false;
}
