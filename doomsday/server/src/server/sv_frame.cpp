/** @file sv_frame.cpp Frame Generation and Transmission
 * @ingroup server
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_play.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

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

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void            Sv_SendFrame(int playerNumber);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int allowFrames = false;
int frameInterval = 1; // Skip every second frame by default (17.5fps)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#ifdef _DEBUG
static int byteCounts[256];
static int totalFrameCount;
#endif

static int lastTransmitTic = 0;

// CODE --------------------------------------------------------------------

/**
 * Send all the relevant information to each client.
 */
void Sv_TransmitFrame(void)
{
    int                 i, cTime, numInGame, pCount;

    // Obviously clients don't transmit anything.
    if(!allowFrames || isClient || Sys_IsShuttingDown())
    {
        return;
    }

    if(!netGame)
    {
        // When not running a netGame, only generate deltas when somebody
        // is recording a demo.
        for(i = 0; i < DDMAXPLAYERS; ++i)
            if(Sv_IsFrameTarget(i))
                break;

        if(i == DDMAXPLAYERS)
        {
            // Nobody is a frame target.
            return;
        }
    }

    if(SECONDS_TO_TICKS(gameTime) == lastTransmitTic)
    {
        // We were just here!
        return;
    }
    lastTransmitTic = SECONDS_TO_TICKS(gameTime);

    // Generate new deltas for the frame.
    Sv_GenerateFrameDeltas();

    // How many players currently in the game?
    numInGame = Sv_GetNumPlayers();

    for(i = 0, pCount = 0; i < DDMAXPLAYERS; ++i)
    {
        if(!Sv_IsFrameTarget(i))
        {
            // This player is not a valid target for frames.
            continue;
        }

        // When the interval is greater than zero, this causes the frames
        // to be sent at different times for each player.
        pCount++;
        cTime = SECONDS_TO_TICKS(gameTime);
        if(frameInterval > 0 && numInGame > 1)
        {
            cTime += (pCount * frameInterval) / numInGame;
        }
        if(cTime <= clients[i].lastTransmit + frameInterval)
        {
            // Still too early to send.
            continue;
        }
        clients[i].lastTransmit = cTime;

        if(clients[i].ready) // && clients[i].updateCount > 0)
        {
/*#ifdef _DEBUG
            Con_Message("Sv_TransmitFrame: Sending at tic %i to plr %i", lastTransmitTic, i);
#endif*/
            // A frame will be sent to this client. If the client
            // doesn't send ticcmds, the updatecount will eventually
            // decrease back to zero.
            //clients[i].updateCount--;

            Sv_SendFrame(i);
        }
#ifdef _DEBUG
        else
        {
            Con_Message("Sv_TransmitFrame: NOT sending at tic %i to plr %i (ready:%i)", lastTransmitTic, i,
                        clients[i].ready);
        }
#endif
    }
}

/**
 * Shutdown routine for the server.
 */
void Sv_Shutdown(void)
{
#ifdef _DEBUG
if(totalFrameCount > 0)
{
    uint                i;

    // Byte probabilities.
    for(i = 0; i < 256; ++i)
    {
        Con_Printf("Byte %02x: %f\n", i,
                   byteCounts[i] / (float) totalFrameCount);
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
    const mobjdelta_t*  delta = reinterpret_cast<mobjdelta_t const *>(deltaPtr);
    const dt_mobj_t*    d = &delta->mo;
    int                 df = delta->delta.flags;
    byte                moreFlags = 0;

    // Do we have fast momentum?
    if(fabs(d->mom[MX]) >= MOM_FAST_LIMIT ||
       fabs(d->mom[MY]) >= MOM_FAST_LIMIT ||
       fabs(d->mom[MZ]) >= MOM_FAST_LIMIT)
    {
        df |= MDF_MORE_FLAGS;
        moreFlags |= MDFE_FAST_MOM;
    }

    // Any translucency?
    if(df & MDFC_TRANSLUCENCY)
    {
        df |= MDF_MORE_FLAGS;
        moreFlags |= MDFE_TRANSLUCENCY;
    }

    // A fade target?
    if(df & MDFC_FADETARGET)
    {
        df |= MDF_MORE_FLAGS;
        moreFlags |= MDFE_FADETARGET;
    }

    // On the floor?
    if(df & MDFC_ON_FLOOR)
    {
        df |= MDF_MORE_FLAGS;
        moreFlags |= MDFE_Z_FLOOR;
    }

    // Mobj type?
    if(df & MDFC_TYPE)
    {
        df |= MDF_MORE_FLAGS;
        moreFlags |= MDFE_TYPE;
    }

    // Flags. What elements are included in the delta?
    if(d->selector & ~DDMOBJ_SELECTOR_MASK)
        df |= MDF_SELSPEC;

    // Omit NULL state.
    if(!d->state)
    {
        df &= ~MDF_STATE;
    }

    /*
    // Floor/ceiling z?
    if(df & MDF_ORIGIN_Z)
    {
        if(d->pos[VZ] == DDMINFLOAT || d->pos[VZ] == DDMAXFLOAT)
        {
            df &= ~MDF_ORIGIN_Z;
            df |= MDF_MORE_FLAGS;
            moreFlags |= (d->pos[VZ] == DDMINFLOAT ? MDFE_Z_FLOOR : MDFE_Z_CEILING);
        }
    }
    */

#ifdef _DEBUG
    if(df & MDFC_NULL)
    {
        Con_Error("Sv_WriteMobjDelta: We don't write Null deltas.\n");
    }
    if((df & 0xffff) == 0)
    {
        Con_Printf("Sv_WriteMobjDelta: This delta id%i [%x] is empty.\n", delta->delta.id, df);
    }
#endif

    // First the mobj ID number and flags.
    Writer_WriteUInt16(msgWriter, delta->delta.id);
    Writer_WriteUInt16(msgWriter, df & 0xffff);

    // More flags?
    if(df & MDF_MORE_FLAGS)
    {
        Writer_WriteByte(msgWriter, moreFlags);
    }

    // Coordinates with three bytes.
    if(df & MDF_ORIGIN_X)
    {
        fixed_t vx = FLT2FIX(d->origin[VX]);

        Writer_WriteInt16(msgWriter, vx >> FRACBITS);
        Writer_WriteByte(msgWriter, vx >> 8);
    }
    if(df & MDF_ORIGIN_Y)
    {
        fixed_t vy = FLT2FIX(d->origin[VY]);

        Writer_WriteInt16(msgWriter, vy >> FRACBITS);
        Writer_WriteByte(msgWriter, vy >> 8);
    }

    if(df & MDF_ORIGIN_Z)
    {
        fixed_t vz = FLT2FIX(d->origin[VZ]);
        Writer_WriteInt16(msgWriter, vz >> FRACBITS);
        Writer_WriteByte(msgWriter, vz >> 8);

        Writer_WriteFloat(msgWriter, d->floorZ);
        Writer_WriteFloat(msgWriter, d->ceilingZ);
    }

    // Momentum using 8.8 fixed point.
    if(df & MDF_MOM_X)
    {
        fixed_t mx = FLT2FIX(d->mom[MX]);
        Writer_WriteInt16(msgWriter, moreFlags & MDFE_FAST_MOM ? FIXED10_6(mx) : FIXED8_8(mx));
    }

    if(df & MDF_MOM_Y)
    {
        fixed_t my = FLT2FIX(d->mom[MY]);
        Writer_WriteInt16(msgWriter, moreFlags & MDFE_FAST_MOM ? FIXED10_6(my) : FIXED8_8(my));
    }

    if(df & MDF_MOM_Z)
    {
        fixed_t mz = FLT2FIX(d->mom[MZ]);
        Writer_WriteInt16(msgWriter, moreFlags & MDFE_FAST_MOM ? FIXED10_6(mz) : FIXED8_8(mz));
    }

    // Angles with 16-bit accuracy.
    if(df & MDF_ANGLE)
        Writer_WriteInt16(msgWriter, d->angle >> 16);

    if(df & MDF_SELECTOR)
        Writer_WritePackedUInt16(msgWriter, d->selector);
    if(df & MDF_SELSPEC)
        Writer_WriteByte(msgWriter, d->selector >> 24);

    if(df & MDF_STATE)
    {
        assert(d->state != 0);
        Writer_WritePackedUInt16(msgWriter, d->state - states);
    }

    if(df & MDF_FLAGS)
    {
        Writer_WriteUInt32(msgWriter, d->ddFlags & DDMF_PACK_MASK);
        Writer_WriteUInt32(msgWriter, d->flags);
        Writer_WriteUInt32(msgWriter, d->flags2);
        Writer_WriteUInt32(msgWriter, d->flags3);
    }

    if(df & MDF_HEALTH)
        Writer_WriteInt32(msgWriter, d->health);

    if(df & MDF_RADIUS)
        Writer_WriteFloat(msgWriter, d->radius);

    if(df & MDF_HEIGHT)
        Writer_WriteFloat(msgWriter, d->height);

    if(df & MDF_FLOORCLIP)
        Writer_WriteFloat(msgWriter, d->floorClip);

    if(df & MDFC_TRANSLUCENCY)
        Writer_WriteByte(msgWriter, d->translucency);

    if(df & MDFC_FADETARGET)
        Writer_WriteByte(msgWriter, (byte)(d->visTarget +1));

    if(df & MDFC_TYPE)
        Writer_WriteInt32(msgWriter, d->type);
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WritePlayerDelta(const void* deltaPtr)
{
    const playerdelta_t* delta = reinterpret_cast<playerdelta_t const *>(deltaPtr);
    const dt_player_t*  d = &delta->player;
    const ddpsprite_t*  psp;
    int                 df = delta->delta.flags;
    int                 psdf, i, k;

    // First the player number. Upper three bits contain flags.
    Writer_WriteByte(msgWriter, delta->delta.id | (df >> 8));

    // Flags. What elements are included in the delta?
    Writer_WriteByte(msgWriter, df & 0xff);

    if(df & PDF_MOBJ)
        Writer_WriteUInt16(msgWriter, d->mobj);
    if(df & PDF_FORWARDMOVE)
        Writer_WriteByte(msgWriter, d->forwardMove);
    if(df & PDF_SIDEMOVE)
        Writer_WriteByte(msgWriter, d->sideMove);
    /*if(df & PDF_ANGLE)
        Writer_WriteByte(msgWriter, d->angle >> 24);*/
    if(df & PDF_TURNDELTA)
        Writer_WriteByte(msgWriter, (d->turnDelta * 16) >> 24);
    if(df & PDF_FRICTION)
        Writer_WriteByte(msgWriter, FLT2FIX(d->friction) >> 8);
    if(df & PDF_EXTRALIGHT)
    {
        // Three bits is enough for fixedcolormap.
        i = d->fixedColorMap;
        if(i < 0)
            i = 0;
        if(i > 7)
            i = 7;
        // Write the five upper bytes of extraLight.
        Writer_WriteByte(msgWriter, i | (d->extraLight & 0xf8));
    }
    if(df & PDF_FILTER)
    {
        Writer_WriteUInt32(msgWriter, d->filter);
#ifdef _DEBUG
        Con_Message("Sv_WritePlayerDelta: Plr %i, filter %08x", delta->delta.id, d->filter);
#endif
    }
    if(df & PDF_PSPRITES)       // Only set if there's something to write.
    {
        for(i = 0; i < 2; ++i)
        {
            psdf = df >> (16 + i * 8);
            psp = d->psp + i;
            // First the flags.
            Writer_WriteByte(msgWriter, psdf);
            if(psdf & PSDF_STATEPTR)
            {
                Writer_WritePackedUInt16(msgWriter, psp->statePtr? (psp->statePtr - states + 1) : 0);
            }
            /*if(psdf & PSDF_LIGHT)
            {
                k = psp->light * 255;
                if(k < 0)
                    k = 0;
                if(k > 255)
                    k = 255;
                Writer_WriteByte(msgWriter, k);
            }*/
            if(psdf & PSDF_ALPHA)
            {
                k = psp->alpha * 255;
                if(k < 0)
                    k = 0;
                if(k > 255)
                    k = 255;
                Writer_WriteByte(msgWriter, k);
            }
            if(psdf & PSDF_STATE)
            {
                Writer_WriteByte(msgWriter, psp->state);
            }
            if(psdf & PSDF_OFFSET)
            {
                Writer_WriteByte(msgWriter, CLAMPED_CHAR(psp->offset[VX] / 2));
                Writer_WriteByte(msgWriter, CLAMPED_CHAR(psp->offset[VY] / 2));
            }
        }
    }
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WriteSectorDelta(const void* deltaPtr)
{
    const sectordelta_t* delta = reinterpret_cast<sectordelta_t const *>(deltaPtr);
    const dt_sector_t*  d = &delta->sector;
    int                 df = delta->delta.flags, spd;
    byte                floorspd = 0, ceilspd = 0;

    // Is there need to use 4.4 fixed-point speeds?
    // (7.1 is too inaccurate for very slow movement)
    if(df & SDF_FLOOR_SPEED)
    {
        spd = FLT2FIX(fabs(d->planes[PLN_FLOOR].speed));
        floorspd = spd >> 15;
        if(!floorspd)
        {
            df |= SDF_FLOOR_SPEED_44;
            floorspd = spd >> 12;
        }
    }
    if(df & SDF_CEILING_SPEED)
    {
        spd = FLT2FIX(fabs(d->planes[PLN_CEILING].speed));
        ceilspd = spd >> 15;
        if(!ceilspd)
        {
            df |= SDF_CEILING_SPEED_44;
            ceilspd = spd >> 12;
        }
    }

    // Sector number first.
    Writer_WriteUInt16(msgWriter, delta->delta.id);

    // Flags.
    Writer_WritePackedUInt32(msgWriter, df);

    if(df & SDF_FLOOR_MATERIAL)
        Writer_WritePackedUInt16(msgWriter, Sv_IdForMaterial(d->planes[PLN_FLOOR].surface.material));
    if(df & SDF_CEILING_MATERIAL)
        Writer_WritePackedUInt16(msgWriter, Sv_IdForMaterial(d->planes[PLN_CEILING].surface.material));
    if(df & SDF_LIGHT)
    {
        // Must fit into a byte.
        int lightlevel = (int) (255.0f * d->lightLevel);
        lightlevel = (lightlevel < 0 ? 0 : lightlevel > 255 ? 255 : lightlevel);

        Writer_WriteByte(msgWriter, (byte) lightlevel);
    }
    if(df & SDF_FLOOR_HEIGHT)
    {
        Writer_WriteInt16(msgWriter, FLT2FIX(d->planes[PLN_FLOOR].height) >> 16);
    }
    if(df & SDF_CEILING_HEIGHT)
    {
#ifdef _DEBUG
        VERBOSE( Con_Printf("Sv_WriteSectorDelta: (%i) Absolute ceiling height=%f\n",
                    delta->delta.id, d->planes[PLN_CEILING].height) );
#endif

        Writer_WriteInt16(msgWriter, FLT2FIX(d->planes[PLN_CEILING].height) >> 16);
    }
    if(df & SDF_FLOOR_TARGET)
        Writer_WriteInt16(msgWriter, FLT2FIX(d->planes[PLN_FLOOR].target) >> 16);
    if(df & SDF_FLOOR_SPEED)    // 7.1/4.4 fixed-point
        Writer_WriteByte(msgWriter, floorspd);
    if(df & SDF_CEILING_TARGET)
        Writer_WriteInt16(msgWriter, FLT2FIX(d->planes[PLN_CEILING].target) >> 16);
    if(df & SDF_CEILING_SPEED)  // 7.1/4.4 fixed-point
        Writer_WriteByte(msgWriter, ceilspd);
    if(df & SDF_COLOR_RED)
        Writer_WriteByte(msgWriter, (byte) (255 * d->rgb[0]));
    if(df & SDF_COLOR_GREEN)
        Writer_WriteByte(msgWriter, (byte) (255 * d->rgb[1]));
    if(df & SDF_COLOR_BLUE)
        Writer_WriteByte(msgWriter, (byte) (255 * d->rgb[2]));

    if(df & SDF_FLOOR_COLOR_RED)
        Writer_WriteByte(msgWriter, (byte) (255 * d->planes[PLN_FLOOR].surface.rgba[0]));
    if(df & SDF_FLOOR_COLOR_GREEN)
        Writer_WriteByte(msgWriter, (byte) (255 * d->planes[PLN_FLOOR].surface.rgba[1]));
    if(df & SDF_FLOOR_COLOR_BLUE)
        Writer_WriteByte(msgWriter, (byte) (255 * d->planes[PLN_FLOOR].surface.rgba[2]));

    if(df & SDF_CEIL_COLOR_RED)
        Writer_WriteByte(msgWriter, (byte) (255 * d->planes[PLN_CEILING].surface.rgba[0]));
    if(df & SDF_CEIL_COLOR_GREEN)
        Writer_WriteByte(msgWriter, (byte) (255 * d->planes[PLN_CEILING].surface.rgba[1]));
    if(df & SDF_CEIL_COLOR_BLUE)
        Writer_WriteByte(msgWriter, (byte) (255 * d->planes[PLN_CEILING].surface.rgba[2]));
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WriteSideDelta(const void* deltaPtr)
{
    const sidedelta_t*  delta = (sidedelta_t const *) deltaPtr;
    const dt_side_t*    d = &delta->side;
    int                 df = delta->delta.flags;

    // Side number first.
    Writer_WriteUInt16(msgWriter, delta->delta.id);

    // Flags.
    Writer_WritePackedUInt32(msgWriter, df);

    if(df & SIDF_TOP_MATERIAL)
        Writer_WritePackedUInt16(msgWriter, Sv_IdForMaterial(d->top.material));
    if(df & SIDF_MID_MATERIAL)
        Writer_WritePackedUInt16(msgWriter, Sv_IdForMaterial(d->middle.material));
    if(df & SIDF_BOTTOM_MATERIAL)
        Writer_WritePackedUInt16(msgWriter, Sv_IdForMaterial(d->bottom.material));

    if(df & SIDF_LINE_FLAGS)
        Writer_WriteByte(msgWriter, d->lineFlags);

    if(df & SIDF_TOP_COLOR_RED)
        Writer_WriteByte(msgWriter, (byte) (255 * d->top.rgba[0]));
    if(df & SIDF_TOP_COLOR_GREEN)
        Writer_WriteByte(msgWriter, (byte) (255 * d->top.rgba[1]));
    if(df & SIDF_TOP_COLOR_BLUE)
        Writer_WriteByte(msgWriter, (byte) (255 * d->top.rgba[2]));

    if(df & SIDF_MID_COLOR_RED)
        Writer_WriteByte(msgWriter, (byte) (255 * d->middle.rgba[0]));
    if(df & SIDF_MID_COLOR_GREEN)
        Writer_WriteByte(msgWriter, (byte) (255 * d->middle.rgba[1]));
    if(df & SIDF_MID_COLOR_BLUE)
        Writer_WriteByte(msgWriter, (byte) (255 * d->middle.rgba[2]));
    if(df & SIDF_MID_COLOR_ALPHA)
        Writer_WriteByte(msgWriter, (byte) (255 * d->middle.rgba[3]));

    if(df & SIDF_BOTTOM_COLOR_RED)
        Writer_WriteByte(msgWriter, (byte) (255 * d->bottom.rgba[0]));
    if(df & SIDF_BOTTOM_COLOR_GREEN)
        Writer_WriteByte(msgWriter, (byte) (255 * d->bottom.rgba[1]));
    if(df & SIDF_BOTTOM_COLOR_BLUE)
        Writer_WriteByte(msgWriter, (byte) (255 * d->bottom.rgba[2]));

    if(df & SIDF_MID_BLENDMODE)
        Writer_WriteInt32(msgWriter, d->middle.blendMode);

    if(df & SIDF_FLAGS)
        Writer_WriteByte(msgWriter, d->flags);
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WritePolyDelta(const void* deltaPtr)
{
    const polydelta_t*  delta = (polydelta_t const *) deltaPtr;
    const dt_poly_t*    d = &delta->po;
    int                 df = delta->delta.flags;

    if(d->destAngle == (unsigned) -1)
    {
        // Send Perpetual Rotate instead of Dest Angle flag.
        df |= PODF_PERPETUAL_ROTATE;
        df &= ~PODF_DEST_ANGLE;
    }

    // Poly number first.
    Writer_WritePackedUInt16(msgWriter, delta->delta.id);

    // Flags.
    Writer_WriteByte(msgWriter, df & 0xff);

    if(df & PODF_DEST_X)
        Writer_WriteFloat(msgWriter, d->dest[VX]);
    if(df & PODF_DEST_Y)
        Writer_WriteFloat(msgWriter, d->dest[VY]);
    if(df & PODF_SPEED)
        Writer_WriteFloat(msgWriter, d->speed);
    if(df & PODF_DEST_ANGLE)
        Writer_WriteInt16(msgWriter, d->destAngle >> 16);
    if(df & PODF_ANGSPEED)
        Writer_WriteInt16(msgWriter, d->angleSpeed >> 16);
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WriteSoundDelta(const void* deltaPtr)
{
    const sounddelta_t* delta = (sounddelta_t const *) deltaPtr;
    int                 df = delta->delta.flags;

    // This is either the sound ID, emitter ID or sector index.
    Writer_WriteUInt16(msgWriter, delta->delta.id);

    // First the flags byte.
    Writer_WriteByte(msgWriter, df & 0xff);

    switch(delta->delta.type)
    {
    case DT_MOBJ_SOUND:
    case DT_SECTOR_SOUND:
    case DT_POLY_SOUND:
        // The sound ID.
        Writer_WriteUInt16(msgWriter, delta->sound);
        break;

    default:
        break;
    }

    // The common parts.
    if(df & SNDDF_VOLUME)
    {
        if(delta->volume > 1)
        {
            // Very loud indeed.
            Writer_WriteByte(msgWriter, 255);
        }
        else if(delta->volume <= 0)
        {
            // Silence.
            Writer_WriteByte(msgWriter, 0);
        }
        else
        {
            Writer_WriteByte(msgWriter, delta->volume * 127 + 0.5f);
        }
    }
}

/**
 * Write the type and possibly the set number (for Unacked deltas).
 */
void Sv_WriteDeltaHeader(byte type, const delta_t* delta)
{
#ifdef _DEBUG
if(type >= NUM_DELTA_TYPES)
{
    Con_Error("Sv_WriteDeltaHeader: Invalid delta type %i.\n", type);
}
#endif

#ifdef _DEBUG
    // Once sent, the deltas can be discarded and there is no need for resending.
    assert(delta->state != DELTA_UNACKED);
#endif

    if(delta->state == DELTA_UNACKED)
    {
        assert(false);
        // Flag this as Resent.
        type |= DT_RESENT;
    }

    Writer_WriteByte(msgWriter, type);

    // Include the set number?
    if(type & DT_RESENT)
    {
        // The client will use this to avoid dupes. If the client has already
        // received the set this delta belongs to, it means the delta has
        // already been received. This is needed in the situation where the
        // ack is lost or delayed.
        Writer_WriteByte(msgWriter, delta->set);

        // Also send the unique ID of this delta. If the client has already
        // received a delta with this ID, the delta is discarded. This is
        // needed in the situation where the set is lost.
        Writer_WriteByte(msgWriter, delta->resend);
    }
}

/**
 * The delta is written to the message buffer.
 */
void Sv_WriteDelta(const delta_t* delta)
{
    byte                type = delta->type;
#ifdef _NETDEBUG
    int                 lengthOffset;
    int                 endOffset;
#endif

#ifdef _NETDEBUG
    // Extra length field in debug builds.
    lengthOffset = Msg_Offset();
    Msg_WriteLong(0);
#endif

    // Null mobj deltas are special.
    if(type == DT_MOBJ)
    {
        if(delta->flags & MDFC_NULL)
        {
            // This'll be the entire delta. No more data is needed.
            Sv_WriteDeltaHeader(DT_NULL_MOBJ, delta);
            Writer_WriteUInt16(msgWriter, delta->id);
#ifdef _NETDEBUG
            goto writeDeltaLength;
#else
            return;
#endif
        }
    }

    // First the type of the delta.
    Sv_WriteDeltaHeader(type, delta);

    switch(delta->type)
    {
    case DT_MOBJ:
        Sv_WriteMobjDelta(delta);
        break;

    case DT_PLAYER:
        Sv_WritePlayerDelta(delta);
        break;

    case DT_SECTOR:
        Sv_WriteSectorDelta(delta);
        break;

    case DT_SIDE:
        Sv_WriteSideDelta(delta);
        break;

    case DT_POLY:
        Sv_WritePolyDelta(delta);
        break;

    case DT_SOUND:
    case DT_MOBJ_SOUND:
    case DT_SECTOR_SOUND:
    case DT_POLY_SOUND:
        Sv_WriteSoundDelta(delta);
        break;

        /*case DT_LUMP:
           Sv_WriteLumpDelta(delta);
           break; */

    default:
        Con_Error("Sv_WriteDelta: Unknown delta type %i.\n", delta->type);
    }

#ifdef _NETDEBUG
writeDeltaLength:
    // Update the length of the delta.
    endOffset = Msg_Offset();
    Msg_SetOffset(lengthOffset);
    Msg_WriteLong(endOffset - lengthOffset);
    Msg_SetOffset(endOffset);
#endif
}

/**
 * @return              An estimate for the maximum frame size appropriate
 *                      for the client. The bandwidth rating is updated
 *                      whenever a frame is sent.
 */
size_t Sv_GetMaxFrameSize(int playerNumber)
{
    size_t              size = MINIMUM_FRAME_SIZE + FRAME_SIZE_FACTOR * clients[playerNumber].bandwidthRating;

    // What about the communications medium?
    if(size > PROTOCOL_MAX_DATAGRAM_SIZE)
        size = PROTOCOL_MAX_DATAGRAM_SIZE;

    return size;
}

/**
 * @return A unique resend ID. Never returns zero.
 */
byte Sv_GetNewResendID(pool_t* pool)
{
    byte id = pool->resendDealer;

    // Advance to next ID, skipping zero.
    while(!++pool->resendDealer) {}

    return id;
}

/**
 * Send a sv_frame packet to the specified player. The amount of data sent
 * depends on the player's bandwidth rating.
 */
void Sv_SendFrame(int plrNum)
{
    pool_t*             pool = Sv_GetPool(plrNum);
    byte                oldResend;
    delta_t*            delta;
    int                 deltaCount = 0;
    size_t              lastStart, maxFrameSize; //, deltaCountOffset = 0;
/*#if _NETDEBUG
    int                 endOffset = 0;
#endif*/

    // Does the send queue allow us to send this packet?
    // Bandwidth rating is updated during the check.
    if(!Sv_CheckBandwidth(plrNum))
    {
        // We cannot send anything at this time. This will only happen if
        // the send queue has too many packets waiting to be sent.
        return;
    }

    // The priority queue of the client needs to be rebuilt before
    // a new frame can be sent.
    Sv_RatePool(pool);

    // This will be a new set.
    pool->setDealer++;

    // Determine the maximum size of the frame packet.
    maxFrameSize = Sv_GetMaxFrameSize(plrNum);

    // Allow more info for the first frame.
    if(pool->isFirst)
        maxFrameSize = MAX_FIRST_FRAME_SIZE;

    // If this is the first frame after a map change, use the special
    // first frame packet type.
    Msg_Begin(pool->isFirst ? PSV_FIRST_FRAME2 : PSV_FRAME2);

    // First send the gameTime of this frame.
    Writer_WriteFloat(msgWriter, gameTime);

    // Keep writing until the maximum size is reached.
    while((delta = Sv_PoolQueueExtract(pool)) != NULL &&
          (lastStart = Writer_Size(msgWriter)) < maxFrameSize)
    {
        oldResend = pool->resendDealer;

        // Is this going to be a resent?
        if(delta->state == DELTA_UNACKED && !delta->resend)
        {
            // Assign a new unique ID for this delta.
            // This ID won't be changed after this.
            delta->resend = Sv_GetNewResendID(pool);
        }

        Sv_WriteDelta(delta);

        // Did we go over the limit?
        if(Writer_Size(msgWriter) > maxFrameSize)
        {
            /*
            // Time to see if BWR needs to be adjusted.
            if(clients[plrNum].bwrAdjustTime <= 0)
            {
                clients[plrNum].bwrAdjustTime = BWR_ADJUST_TICS;
            }
            */

            // Cancel the last delta.
            Writer_SetPos(msgWriter, lastStart);

            // Restore the resend dealer.
            if(oldResend)
                pool->resendDealer = oldResend;
            break;
        }

        // Successfully written, increment counter.
        deltaCount++;
/*
#ifdef _DEBUG
if(delta->state == DELTA_UNACKED)
{
    Con_Printf("Resend: %i, type%i[%x], set%i, rsid%i\n",
               delta->id, delta->type, delta->flags,
               delta->set, delta->resend);
}
#endif
*/
        // Update the sent delta's state.
        if(delta->state == DELTA_NEW)
        {
            // New deltas are assigned to this set. Unacked deltas will
            // remain in the set they were initially sent in.
            delta->set = pool->setDealer;
            delta->timeStamp = Sv_GetTimeStamp();
            delta->state = DELTA_UNACKED;
        }
    }

    // Update the number of deltas included in the packet.
/*
#ifdef _NETDEBUG
    endOffset = Msg_Offset();
    Msg_SetOffset(deltaCountOffset);
    Msg_WriteLong(deltaCount);
    Msg_SetOffset(endOffset);
#endif
*/

    Msg_End();

    Net_SendBuffer(plrNum, 0);

    // Once sent, the delta set can be discarded.
    Sv_AckDeltaSet(plrNum, pool->setDealer, 0);

    // Now a frame has been sent.
    pool->isFirst = false;
}
