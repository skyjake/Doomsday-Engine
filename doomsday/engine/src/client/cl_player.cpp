/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * cl_player.c: Clientside Player Management
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

#define TOP_PSPY            (32)
#define BOTTOM_PSPY         (128)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float pspMoveSpeed = 6;
float cplrThrustMul = 1;
clplayerstate_t clPlayerStates[DDMAXPLAYERS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

//static int fixSpeed = 15;
//static float fixPos[3];
//static int fixTics;
static float pspY;

// Console player demo momentum (used to smooth out abrupt momentum changes).
static float cpMom[3][LOCALCAM_WRITE_TICS];

// CODE --------------------------------------------------------------------

/**
 * Clears the player state table.
 */
void Cl_InitPlayers(void)
{
    //fixTics = 0;
    pspY = 0;
    memset(&clPlayerStates, 0, sizeof(clPlayerStates));
    //memset(fixPos, 0, sizeof(fixPos));
    memset(cpMom, 0, sizeof(cpMom));
}

clplayerstate_t *ClPlayer_State(int plrNum)
{
    assert(plrNum >= 0 && plrNum < DDMAXPLAYERS);
    return &clPlayerStates[plrNum];
}

/**
 * Thrust (with a multiplier).
 */
void Cl_ThrustMul(mobj_t *mo, angle_t angle, float move, float thmul)
{
    // Make a fine angle.
    angle >>= ANGLETOFINESHIFT;
    move *= thmul;
    mo->mom[MX] += move * FIX2FLT(fineCosine[angle]);
    mo->mom[MY] += move * FIX2FLT(finesine[angle]);
}

void Cl_Thrust(mobj_t *mo, angle_t angle, float move)
{
    Cl_ThrustMul(mo, angle, move, 1);
}

/**
 * @param plrNum  Player number.
 *
 * @return  The engineside client mobj of a player, representing a remote mobj on the server.
 */
#undef ClPlayer_ClMobj
DENG_EXTERN_C struct mobj_s* ClPlayer_ClMobj(int plrNum)
{
    assert(plrNum >= 0 && plrNum < DDMAXPLAYERS);
    return ClMobj_Find(clPlayerStates[plrNum].clMobjId);
}

/**
 * Move the (hidden, unlinked) client player mobj to the same coordinates
 * where the real mobj of the player is.
 */
void ClPlayer_UpdateOrigin(int plrNum)
{
    player_t           *plr;
    mobj_t             *remoteClientMobj, *localMobj;
    clplayerstate_t    *s;

    assert(plrNum >= 0 && plrNum < DDMAXPLAYERS);

    plr = &ddPlayers[plrNum];
    s = ClPlayer_State(plrNum);

    if(!s->clMobjId || !plr->shared.mo)
        return;                 // Must have a mobj!

    remoteClientMobj = ClMobj_Find(s->clMobjId);
    localMobj = plr->shared.mo;

    // The client mobj is never solid.
    remoteClientMobj->ddFlags &= ~DDMF_SOLID;

    remoteClientMobj->angle = localMobj->angle;

    // The player's client mobj is not linked to any lists, so position
    // can be updated without any hassles.
    memcpy(remoteClientMobj->origin, localMobj->origin, sizeof(localMobj->origin));
    P_MobjLink(remoteClientMobj, 0); // Update bspLeaf pointer.
    remoteClientMobj->floorZ = localMobj->floorZ;
    remoteClientMobj->ceilingZ = localMobj->ceilingZ;
    remoteClientMobj->mom[MX] = localMobj->mom[MX];
    remoteClientMobj->mom[MY] = localMobj->mom[MY];
    remoteClientMobj->mom[MZ] = localMobj->mom[MZ];
}

/*
void ClPlayer_CoordsReceived(void)
{
    if(playback)
        return;

#ifdef _DEBUG
    //Con_Printf("ClPlayer_CoordsReceived\n");
#endif

    fixPos[VX] = (float) Msg_ReadShort();
    fixPos[VY] = (float) Msg_ReadShort();
    fixTics = fixSpeed;
    fixPos[VX] /= fixSpeed;
    fixPos[VY] /= fixSpeed;
}
*/

void ClPlayer_ApplyPendingFixes(int plrNum)
{
    clplayerstate_t *state = ClPlayer_State(plrNum);
    player_t        *plr = &ddPlayers[plrNum];
    mobj_t          *clmo = ClPlayer_ClMobj(plrNum);
    ddplayer_t      *ddpl = &plr->shared;
    mobj_t          *mo = ddpl->mo;
    boolean          sendAck = false;

    // If either mobj is missing, the fix cannot be applied yet.
    if(!mo || !clmo) return;

    if(clmo->thinker.id != state->pendingFixTargetClMobjId)
        return;

    assert(clmo->thinker.id == state->clMobjId);

    if(state->pendingFixes & DDPF_FIXANGLES)
    {
        state->pendingFixes &= ~DDPF_FIXANGLES;
        ddpl->fixAcked.angles = ddpl->fixCounter.angles;
        sendAck = true;

#ifdef _DEBUG
        Con_Message("ClPlayer_ApplyPendingFixes: Applying angle %x to mobj %p and clmo %i...\n",
                    state->pendingAngleFix, mo, clmo->thinker.id);
#endif
        clmo->angle = mo->angle = state->pendingAngleFix;
        ddpl->lookDir = state->pendingLookDirFix;
    }

    if(state->pendingFixes & DDPF_FIXORIGIN)
    {
        state->pendingFixes &= ~DDPF_FIXORIGIN;
        ddpl->fixAcked.origin = ddpl->fixCounter.origin;
        sendAck = true;

#ifdef _DEBUG
        Con_Message("ClPlayer_ApplyPendingFixes: Applying pos (%f, %f, %f) to mobj %p and clmo %i...\n",
                    state->pendingOriginFix[VX], state->pendingOriginFix[VY], state->pendingOriginFix[VZ],
                    mo, clmo->thinker.id);
#endif
        Mobj_SetOrigin(mo, state->pendingOriginFix[VX], state->pendingOriginFix[VY], state->pendingOriginFix[VZ]);
        mo->reactionTime = 18;

        // The position is now known.
        ddpl->flags &= ~DDPF_UNDEFINED_ORIGIN;

        Smoother_Clear(clients[plrNum].smoother);
        ClPlayer_UpdateOrigin(plrNum);
    }

    if(state->pendingFixes & DDPF_FIXMOM)
    {
        state->pendingFixes &= ~DDPF_FIXMOM;
        ddpl->fixAcked.mom = ddpl->fixCounter.mom;
        sendAck = true;

#ifdef _DEBUG
        Con_Message("ClPlayer_ApplyPendingFixes: Applying mom (%f, %f, %f) to mobj %p and clmo %i...\n",
                    state->pendingMomFix[VX], state->pendingMomFix[VY], state->pendingMomFix[VZ],
                    mo, clmo->thinker.id);
#endif
        mo->mom[MX] = clmo->mom[VX] = state->pendingMomFix[VX];
        mo->mom[MY] = clmo->mom[VY] = state->pendingMomFix[VY];
        mo->mom[MZ] = clmo->mom[VZ] = state->pendingMomFix[VZ];
    }

    // We'll only need to ack fixes targeted to the consoleplayer.
    if(sendAck && plrNum == consolePlayer)
    {
        // Send an acknowledgement.
        Msg_Begin(PCL_ACK_PLAYER_FIX);
        Writer_WriteInt32(msgWriter, ddpl->fixAcked.angles);
        Writer_WriteInt32(msgWriter, ddpl->fixAcked.origin);
        Writer_WriteInt32(msgWriter, ddpl->fixAcked.mom);
        Msg_End();
        Net_SendBuffer(0, 0);
    }
}

void ClPlayer_HandleFix(void)
{
    int plrNum = 0;
    int fixes = 0;
    player_t* plr;
    ddplayer_t* ddpl;
    clplayerstate_t* state;

    // Target player.
    plrNum = Reader_ReadByte(msgReader);
    plr = &ddPlayers[plrNum];
    ddpl = &plr->shared;
    state = ClPlayer_State(plrNum);

    // What to fix?
    fixes = Reader_ReadUInt32(msgReader);

    state->pendingFixTargetClMobjId = Reader_ReadUInt16(msgReader);

    if(fixes & 1) // fix angles?
    {
        ddpl->fixCounter.angles = Reader_ReadInt32(msgReader);
        state->pendingAngleFix = Reader_ReadUInt32(msgReader);
        state->pendingLookDirFix = Reader_ReadFloat(msgReader);
        state->pendingFixes |= DDPF_FIXANGLES;

#ifdef _DEBUG
        Con_Message("Cl_HandlePlayerFix: [Plr %i] Fix angles %i. Angle=%x, lookdir=%f\n", plrNum,
                    ddpl->fixAcked.angles, state->pendingAngleFix, state->pendingLookDirFix);
#endif
    }

    if(fixes & 2) // fix pos?
    {
        ddpl->fixCounter.origin = Reader_ReadInt32(msgReader);
        state->pendingOriginFix[VX] = Reader_ReadFloat(msgReader);
        state->pendingOriginFix[VY] = Reader_ReadFloat(msgReader);
        state->pendingOriginFix[VZ] = Reader_ReadFloat(msgReader);
        state->pendingFixes |= DDPF_FIXORIGIN;

#ifdef _DEBUG
        Con_Message("Cl_HandlePlayerFix: [Plr %i] Fix pos %i. Pos=%f, %f, %f\n", plrNum,
                    ddpl->fixAcked.origin, state->pendingOriginFix[VX], state->pendingOriginFix[VY], state->pendingOriginFix[VZ]);
#endif
    }

    if(fixes & 4) // fix momentum?
    {
        ddpl->fixCounter.mom = Reader_ReadInt32(msgReader);
        state->pendingMomFix[VX] = Reader_ReadFloat(msgReader);
        state->pendingMomFix[VY] = Reader_ReadFloat(msgReader);
        state->pendingMomFix[VZ] = Reader_ReadFloat(msgReader);
        state->pendingFixes |= DDPF_FIXMOM;
    }

    ClPlayer_ApplyPendingFixes(plrNum);
}

void ClPlayer_MoveLocal(coord_t dx, coord_t dy, coord_t z, boolean onground)
{
    player_t* plr = &ddPlayers[consolePlayer];
    ddplayer_t* ddpl = &plr->shared;
    mobj_t* mo = ddpl->mo;
    coord_t mom[3];
    int i;

    if(!mo) return;

    // Place the new momentum in the appropriate place.
    cpMom[MX][SECONDS_TO_TICKS(gameTime) % LOCALCAM_WRITE_TICS] = dx;
    cpMom[MY][SECONDS_TO_TICKS(gameTime) % LOCALCAM_WRITE_TICS] = dy;

    // Calculate an average.
    mom[MX] = mom[MY] = 0;
    for(i = 0; i < LOCALCAM_WRITE_TICS; ++i)
    {
        mom[MX] += cpMom[MX][i];
        mom[MY] += cpMom[MY][i];
    }
    mom[MX] /= LOCALCAM_WRITE_TICS;
    mom[MY] /= LOCALCAM_WRITE_TICS;

    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];

    if(dx != 0 || dy != 0)
    {
        P_MobjUnlink(mo);
        mo->origin[VX] += dx;
        mo->origin[VY] += dy;
        P_MobjLink(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
    }

    mo->bspLeaf = P_BspLeafAtPoint(mo->origin);
    mo->floorZ = mo->bspLeaf->sector->SP_floorheight;
    mo->ceilingZ = mo->bspLeaf->sector->SP_ceilheight;

    if(onground)
    {
        mo->origin[VZ] = z - 1;
    }
    else
    {
        mo->origin[VZ] = z;
    }

    ClPlayer_UpdateOrigin(consolePlayer);
}

/**
 * Reads a single PSV_FRAME2 player delta from the message buffer and
 * applies it to the player in question.
 */
void ClPlayer_ReadDelta2(boolean skip)
{
    static player_t     dummyPlayer;
    static clplayerstate_t dummyClState;

    int                 df = 0, psdf, i, idx;
    clplayerstate_t    *s;
    ddplayer_t         *ddpl;
    ddpsprite_t        *psp;
    unsigned short num, newId;
    short               junk;

    // The first byte consists of a player number and some flags.
    num = Reader_ReadByte(msgReader);
    df = (num & 0xf0) << 8;
    df |= Reader_ReadByte(msgReader); // Second byte is just flags.
    num &= 0xf; // Clear the upper bits of the number.

    if(!skip)
    {
        s = &clPlayerStates[num];
        ddpl = &ddPlayers[num].shared;
    }
    else
    {
        // We're skipping, read the data into dummies.
        s = &dummyClState;
        ddpl = &dummyPlayer.shared;
    }

    if(df & PDF_MOBJ)
    {
        mobj_t *old = ClMobj_Find(s->clMobjId);

        newId = Reader_ReadUInt16(msgReader);

        // Make sure the 'new' mobj is different than the old one;
        // there will be linking problems otherwise.
        if(!skip && newId != s->clMobjId)
        {
            // We are now changing the player's mobj.
            mobj_t* clmo = 0;
            clmoinfo_t* info = 0;
            boolean justCreated = false;

            s->clMobjId = newId;

            // Find the new mobj.
            clmo = ClMobj_Find(s->clMobjId);
            info = ClMobj_GetInfo(clmo);
            if(!clmo)
            {
#ifdef _DEBUG
                Con_Message("ClPlayer_ReadDelta2: Player %i's new clmobj is %i, but we don't know it yet.\n",
                            num, newId);
#endif
                // This mobj hasn't yet been sent to us.
                // We should be receiving the rest of the info very shortly.
                clmo = ClMobj_Create(s->clMobjId);
                info = ClMobj_GetInfo(clmo);
                /*
                if(num == consolePlayer)
                {
                    // Mark everything known about our local player.
                    //info->flags |= CLMF_KNOWN;
                }*/
                justCreated = true;
            }
            else
            {
                // The client mobj is already known to us.
                // Unlink it (not interactive or visible).
                ClMobj_Unlink(clmo);
            }

            clmo->dPlayer = ddpl;

            // Make the old clmobj a non-player one (if any).
            if(old)
            {
                old->dPlayer = NULL;
                ClMobj_Link(old);
            }

            // If it was just created, the coordinates are not yet correct.
            // The update will be made when the mobj data is received.
            if(!justCreated) // && num != consolePlayer)
            {
#ifdef _DEBUG
                Con_Message("ClPlayer_ReadDelta2: Copying clmo %i state to real player %i mobj %p.\n",
                            newId, num, ddpl->mo);
#endif
                Cl_UpdateRealPlayerMobj(ddpl->mo, clmo, 0xffffffff, true);
            }
            /*
            else if(ddpl->mo)
            {
                // Update the new client mobj's information from the real
                // mobj, which is already known.
#if _DEBUG
                Con_Message("Cl_RdPlrD2: Pl%i: Copying pos&angle from real mobj to clmobj.\n", num);
                Con_Message("  x=%g y=%g z=%g\n", ddpl->mo->origin[VX], ddpl->mo->origin[VY], ddpl->mo->origin[VZ]);
#endif
                clmo->pos[VX] = ddpl->mo->origin[VX];
                clmo->pos[VY] = ddpl->mo->origin[VY];
                clmo->pos[VZ] = ddpl->mo->origin[VZ];
                clmo->angle = ddpl->mo->angle;
                if(!skip)
                    ClPlayer_UpdateOrigin(num);
            }
            */

#if _DEBUG
            Con_Message("ClPlr_RdD2: Pl%i: mobj=%i old=%p\n", num, s->clMobjId, old);
            Con_Message("  x=%g y=%g z=%g fz=%g cz=%g\n", clmo->origin[VX],
                        clmo->origin[VY], clmo->origin[VZ], clmo->floorZ, clmo->ceilingZ);
            Con_Message("ClPlr_RdD2: pl=%i => moid=%i\n", (skip? -1 : num), s->clMobjId);
#endif
        }
    }

    if(df & PDF_FORWARDMOVE)
        s->forwardMove = (char) Reader_ReadByte(msgReader) * 2048;
    if(df & PDF_SIDEMOVE)
        s->sideMove = (char) Reader_ReadByte(msgReader) * 2048;
    if(df & PDF_ANGLE)
        //s->angle = Reader_ReadByte(msgReader) << 24;
        junk = Reader_ReadByte(msgReader);
    if(df & PDF_TURNDELTA)
    {
        s->turnDelta = ((char) Reader_ReadByte(msgReader) << 24) / 16;
    }
    if(df & PDF_FRICTION)
        s->friction = Reader_ReadByte(msgReader) << 8;
    if(df & PDF_EXTRALIGHT)
    {
        i = Reader_ReadByte(msgReader);
        ddpl->fixedColorMap = i & 7;
        ddpl->extraLight = i & 0xf8;
    }
    if(df & PDF_FILTER)
    {
        unsigned int filter = Reader_ReadUInt32(msgReader);

        ddpl->filterColor[CR] = (filter & 0xff) / 255.f;
        ddpl->filterColor[CG] = ((filter >> 8) & 0xff) / 255.f;
        ddpl->filterColor[CB] = ((filter >> 16) & 0xff) / 255.f;
        ddpl->filterColor[CA] = ((filter >> 24) & 0xff) / 255.f;

        if(ddpl->filterColor[CA] > 0)
        {
            ddpl->flags |= DDPF_REMOTE_VIEW_FILTER;
        }
        else
        {
            ddpl->flags &= ~DDPF_REMOTE_VIEW_FILTER;
        }
#ifdef _DEBUG
        Con_Message("ClPlayer_ReadDelta2: Filter color set remotely to (%f,%f,%f,%f)\n",
                    ddpl->filterColor[CR],
                    ddpl->filterColor[CG],
                    ddpl->filterColor[CB],
                    ddpl->filterColor[CA]);
#endif
    }
    if(df & PDF_PSPRITES)
    {
        for(i = 0; i < 2; ++i)
        {
            // First the flags.
            psdf = Reader_ReadByte(msgReader);
            psp = ddpl->pSprites + i;
            if(psdf & PSDF_STATEPTR)
            {
                idx = Reader_ReadPackedUInt16(msgReader);
                if(!idx)
                    psp->statePtr = 0;
                else if(idx < countStates.num)
                {
                    psp->statePtr = states + (idx - 1);
                    psp->tics = psp->statePtr->tics;
                }
            }

            /*if(psdf & PSDF_LIGHT)
                psp->light = Reader_ReadByte(msgReader) / 255.0f;*/
            if(psdf & PSDF_ALPHA)
                psp->alpha = Reader_ReadByte(msgReader) / 255.0f;
            if(psdf & PSDF_STATE)
                psp->state = Reader_ReadByte(msgReader);
            if(psdf & PSDF_OFFSET)
            {
                psp->offset[VX] = (char) Reader_ReadByte(msgReader) * 2;
                psp->offset[VY] = (char) Reader_ReadByte(msgReader) * 2;
            }
        }
    }
}

/**
 * @return  The gameside local mobj of a player.
 */
mobj_t *ClPlayer_LocalGameMobj(int plrNum)
{
    return ddPlayers[plrNum].shared.mo;
}

/**
 * Used by the client plane mover.
 *
 * @return              @c true, if the player is free to move according to
 *                      floorz and ceilingz.
 */
boolean ClPlayer_IsFreeToMove(int plrNum)
{
    mobj_t* mo = ClPlayer_LocalGameMobj(plrNum);
    if(!mo) return false;

    return (mo->origin[VZ] >= mo->floorZ &&
            mo->origin[VZ] + mo->height <= mo->ceilingZ);
}
