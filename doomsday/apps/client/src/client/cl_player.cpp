/** @file cl_player.cpp  Clientside player management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
#include "client/cl_player.h"
#include "api_client.h"
#include "network/net_main.h"
#include "world/map.h"
#include "world/p_players.h"
#include <doomsday/network/protocol.h>
#include <doomsday/world/bspleaf.h>
#include <doomsday/world/sector.h>

#include <de/logbuffer.h>
#include <de/vector.h>

using namespace de;
using world::World;

#define TOP_PSPY            (32)
#define BOTTOM_PSPY         (128)

float pspMoveSpeed = 6;
float cplrThrustMul = 1;

static float pspY;

// Console player demo momentum (used to smooth out abrupt momentum changes).
static float cpMom[3][LOCALCAM_WRITE_TICS];

void Cl_InitPlayers()
{
    DoomsdayApp::players().forAll([] (Player &plr) {
        zap(plr.as<ClientPlayer>().clPlayerState());
        return LoopContinue;
    });

    pspY = 0;
    de::zap(cpMom);
}

clplayerstate_t *ClPlayer_State(int plrNum)
{
    return &DD_Player(plrNum)->clPlayerState();
}

#undef ClPlayer_ClMobj
DE_EXTERN_C struct mobj_s *ClPlayer_ClMobj(int plrNum)
{
    if (plrNum < 0 || plrNum >= DDMAXPLAYERS) return 0;
    return ClMobj_Find(ClPlayer_State(plrNum)->clMobjId);
}

void ClPlayer_UpdateOrigin(int plrNum)
{
    DE_ASSERT(plrNum >= 0 && plrNum < DDMAXPLAYERS);

    player_t *plr = DD_Player(plrNum);
    clplayerstate_t *s = ClPlayer_State(plrNum);

    if (!s->clMobjId || !plr->publicData().mo)
        return;                 // Must have a mobj!

    mobj_t *remoteClientMobj = ClMobj_Find(s->clMobjId);
    mobj_t *localMobj = plr->publicData().mo;

    // The client mobj is never solid.
    remoteClientMobj->ddFlags &= ~DDMF_SOLID;

    remoteClientMobj->angle = localMobj->angle;

    // The player's client mobj is not linked to any lists, so position
    // can be updated without any hassles.
    std::memcpy(remoteClientMobj->origin, localMobj->origin, sizeof(localMobj->origin));
    Mobj_Link(remoteClientMobj, 0); // Update bspLeaf pointer.
    remoteClientMobj->floorZ = localMobj->floorZ;
    remoteClientMobj->ceilingZ = localMobj->ceilingZ;
    remoteClientMobj->mom[MX] = localMobj->mom[MX];
    remoteClientMobj->mom[MY] = localMobj->mom[MY];
    remoteClientMobj->mom[MZ] = localMobj->mom[MZ];
}

void ClPlayer_ApplyPendingFixes(int plrNum)
{
    LOG_AS("ClPlayer_ApplyPendingFixes");

    clplayerstate_t *state = ClPlayer_State(plrNum);
    player_t *plr = DD_Player(plrNum);
    mobj_t *clmo = ClPlayer_ClMobj(plrNum);
    ddplayer_t *ddpl = &plr->publicData();
    mobj_t *mo = ddpl->mo;
    bool sendAck = false;

    // If either mobj is missing, the fix cannot be applied yet.
    if (!mo || !clmo) return;

    if (clmo->thinker.id != state->pendingFixTargetClMobjId)
        return;

    DE_ASSERT(clmo->thinker.id == state->clMobjId);

    if (state->pendingFixes & DDPF_FIXANGLES)
    {
        state->pendingFixes &= ~DDPF_FIXANGLES;
        ddpl->fixAcked.angles = ddpl->fixCounter.angles;
        sendAck = true;

        LOGDEV_NET_MSG("Applying angle %x to mobj %p and clmo %i")
                << state->pendingAngleFix << mo << clmo->thinker.id;

        clmo->angle = mo->angle = state->pendingAngleFix;
        ddpl->lookDir = state->pendingLookDirFix;
    }

    if (state->pendingFixes & DDPF_FIXORIGIN)
    {
        state->pendingFixes &= ~DDPF_FIXORIGIN;
        ddpl->fixAcked.origin = ddpl->fixCounter.origin;
        sendAck = true;

        LOGDEV_NET_MSG("Applying pos %s to mobj %p and clmo %i")
                << Vec3d(state->pendingOriginFix).asText()
                << mo << clmo->thinker.id;

        Mobj_SetOrigin(mo, state->pendingOriginFix[VX], state->pendingOriginFix[VY], state->pendingOriginFix[VZ]);
        mo->reactionTime = 18;

        // The position is now known.
        ddpl->flags &= ~DDPF_UNDEFINED_ORIGIN;

        Smoother_Clear(DD_Player(plrNum)->smoother());
        ClPlayer_UpdateOrigin(plrNum);
    }

    if (state->pendingFixes & DDPF_FIXMOM)
    {
        state->pendingFixes &= ~DDPF_FIXMOM;
        ddpl->fixAcked.mom = ddpl->fixCounter.mom;
        sendAck = true;

        LOGDEV_NET_MSG("Applying mom %s to mobj %p and clmo %i")
                << Vec3d(state->pendingMomFix).asText()
                << mo << clmo->thinker.id;

        mo->mom[MX] = clmo->mom[VX] = state->pendingMomFix[VX];
        mo->mom[MY] = clmo->mom[VY] = state->pendingMomFix[VY];
        mo->mom[MZ] = clmo->mom[VZ] = state->pendingMomFix[VZ];
    }

    // We'll only need to ack fixes targeted to the consoleplayer.
    if (sendAck && plrNum == consolePlayer)
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

void ClPlayer_HandleFix()
{
    LOG_AS("Cl_HandlePlayerFix");

    // Target player.
    int plrNum = Reader_ReadByte(msgReader);
    player_t *plr = DD_Player(plrNum);
    ddplayer_t *ddpl = &plr->publicData();
    clplayerstate_t *state = ClPlayer_State(plrNum);

    // What to fix?
    int fixes = Reader_ReadUInt32(msgReader);

    state->pendingFixTargetClMobjId = Reader_ReadUInt16(msgReader);

    LOGDEV_NET_MSG("Fixing player %i") << plrNum;

    if (fixes & 1) // fix angles?
    {
        ddpl->fixCounter.angles = Reader_ReadInt32(msgReader);
        state->pendingAngleFix = Reader_ReadUInt32(msgReader);
        state->pendingLookDirFix = Reader_ReadFloat(msgReader);
        state->pendingFixes |= DDPF_FIXANGLES;

        LOGDEV_NET_VERBOSE("Pending fix angles %i: angle=%x, lookdir=%f")
                << ddpl->fixAcked.angles << state->pendingAngleFix << state->pendingLookDirFix;
    }

    if (fixes & 2) // fix pos?
    {
        ddpl->fixCounter.origin = Reader_ReadInt32(msgReader);
        state->pendingOriginFix[VX] = Reader_ReadFloat(msgReader);
        state->pendingOriginFix[VY] = Reader_ReadFloat(msgReader);
        state->pendingOriginFix[VZ] = Reader_ReadFloat(msgReader);
        state->pendingFixes |= DDPF_FIXORIGIN;

        LOGDEV_NET_VERBOSE("Pending fix pos %i: %s")
                << ddpl->fixAcked.origin << Vec3d(state->pendingOriginFix).asText();
    }

    if (fixes & 4) // fix momentum?
    {
        ddpl->fixCounter.mom = Reader_ReadInt32(msgReader);
        state->pendingMomFix[VX] = Reader_ReadFloat(msgReader);
        state->pendingMomFix[VY] = Reader_ReadFloat(msgReader);
        state->pendingMomFix[VZ] = Reader_ReadFloat(msgReader);
        state->pendingFixes |= DDPF_FIXMOM;

        LOGDEV_NET_VERBOSE("Pending fix momentum %i: %s")
                << ddpl->fixAcked.mom << Vec3d(state->pendingMomFix).asText();
    }

    ClPlayer_ApplyPendingFixes(plrNum);
}

void ClPlayer_MoveLocal(coord_t dx, coord_t dy, coord_t z, bool onground)
{
    player_t *plr = DD_Player(consolePlayer);
    ddplayer_t *ddpl = &plr->publicData();
    mobj_t *mo = ddpl->mo;
    if (!mo) return;

    // Place the new momentum in the appropriate place.
    cpMom[MX][SECONDS_TO_TICKS(gameTime) % LOCALCAM_WRITE_TICS] = dx;
    cpMom[MY][SECONDS_TO_TICKS(gameTime) % LOCALCAM_WRITE_TICS] = dy;

    // Calculate an average.
    Vec2d mom;
    for (int i = 0; i < LOCALCAM_WRITE_TICS; ++i)
    {
        mom += Vec2d(cpMom[MX][i], cpMom[MY][i]);
    }
    mom /= LOCALCAM_WRITE_TICS;

    mo->mom[MX] = mom.x;
    mo->mom[MY] = mom.y;

    if (dx != 0 || dy != 0)
    {
        Mobj_Unlink(mo);
        mo->origin[VX] += dx;
        mo->origin[VY] += dy;
        Mobj_Link(mo, MLF_SECTOR | MLF_BLOCKMAP);
    }

    mo->_bspLeaf = &Mobj_Map(*mo).bspLeafAt_FixedPrecision(Mobj_Origin(*mo));
    mo->floorZ   = Mobj_Sector(mo)->floor().height();
    mo->ceilingZ = Mobj_Sector(mo)->ceiling().height();

    if (onground)
    {
        mo->origin[VZ] = z - 1;
    }
    else
    {
        mo->origin[VZ] = z;
    }

    ClPlayer_UpdateOrigin(consolePlayer);
}

void ClPlayer_ReadDelta()
{
    LOG_AS("ClPlayer_ReadDelta2");

    /// @todo Do not assume the CURRENT map.
    Map &map = World::get().map().as<Map>();

    dint df = 0;
    ushort num;

    // The first byte consists of a player number and some flags.
    num = Reader_ReadByte(msgReader);
    df = (num & 0xf0) << 8;
    df |= Reader_ReadByte(msgReader); // Second byte is just flags.
    num &= 0xf; // Clear the upper bits of the number.

    clplayerstate_t *s = ClPlayer_State(num);
    ddplayer_t *ddpl = &DD_Player(num)->publicData();

    if (df & PDF_MOBJ)
    {
        mobj_t *old  = map.clMobjFor(s->clMobjId);
        ushort newId = Reader_ReadUInt16(msgReader);

        // Make sure the 'new' mobj is different than the old one;
        // there will be linking problems otherwise.
        if (newId != s->clMobjId)
        {
            // We are now changing the player's mobj.
            mobj_t *clmo = 0;
            //clmoinfo_t* info = 0;
            bool justCreated = false;

            s->clMobjId = newId;

            // Find the new mobj.
            clmo = map.clMobjFor(s->clMobjId);
            //info = ClMobj_GetInfo(clmo);
            if (!clmo)
            {
                LOGDEV_NET_NOTE("Player %i's new clmobj is %i, but we haven't received it yet")
                        << num << newId;

                // This mobj hasn't yet been sent to us.
                // We should be receiving the rest of the info very shortly.
                clmo = map.clMobjFor(s->clMobjId, true/*create*/);
                //info = ClMobj_GetInfo(clmo);
                /*
                if (num == consolePlayer)
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
                Mobj_Unlink(clmo);
            }

            clmo->dPlayer = ddpl;

            // Make the old clmobj a non-player one (if any).
            if (old)
            {
                old->dPlayer = NULL;
                ClMobj_Link(old);
            }

            // If it was just created, the coordinates are not yet correct.
            // The update will be made when the mobj data is received.
            if (!justCreated) // && num != consolePlayer)
            {
                LOGDEV_NET_XVERBOSE("Copying clmo %i state to real player %i mobj %p",
                                    newId << num << ddpl->mo);

                Cl_UpdateRealPlayerMobj(ddpl->mo, clmo, 0xffffffff, true);
            }

            LOGDEV_NET_VERBOSE("Player %i: mobj=%i old=%p x=%.1f y=%.1f z=%.1f Fz=%.1f Cz=%.1f")
                    << num << s->clMobjId << old
                    << clmo->origin[VX] << clmo->origin[VY] << clmo->origin[VZ]
                    << clmo->floorZ << clmo->ceilingZ;
            LOGDEV_NET_VERBOSE("Player %i using mobj id %i") << num << s->clMobjId;
        }
    }

    if (df & PDF_FORWARDMOVE)
    {
        s->forwardMove = (char) Reader_ReadByte(msgReader) * 2048;
    }

    if (df & PDF_SIDEMOVE)
    {
        s->sideMove = (char) Reader_ReadByte(msgReader) * 2048;
    }

    if (df & PDF_ANGLE)
    {
        //s->angle = Reader_ReadByte(msgReader) << 24;
        DE_UNUSED(Reader_ReadByte(msgReader));
    }

    if (df & PDF_TURNDELTA)
    {
        s->turnDelta = ((char) Reader_ReadByte(msgReader) << 24) / 16;
    }

    if (df & PDF_FRICTION)
    {
        s->friction = Reader_ReadByte(msgReader) << 8;
    }

    if (df & PDF_EXTRALIGHT)
    {
        int val = Reader_ReadByte(msgReader);
        ddpl->fixedColorMap = val & 7;
        ddpl->extraLight    = val & 0xf8;
    }

    if (df & PDF_FILTER)
    {
        uint filter = Reader_ReadUInt32(msgReader);

        ddpl->filterColor[CR] = (filter & 0xff) / 255.f;
        ddpl->filterColor[CG] = ((filter >> 8) & 0xff) / 255.f;
        ddpl->filterColor[CB] = ((filter >> 16) & 0xff) / 255.f;
        ddpl->filterColor[CA] = ((filter >> 24) & 0xff) / 255.f;

        if (ddpl->filterColor[CA] > 0)
        {
            ddpl->flags |= DDPF_REMOTE_VIEW_FILTER;
        }
        else
        {
            ddpl->flags &= ~DDPF_REMOTE_VIEW_FILTER;
        }
        LOG_NET_XVERBOSE("View filter color set remotely to %s",
                         Vec4f(ddpl->filterColor).asText());
    }

    if (df & PDF_PSPRITES)
    {
        for (int i = 0; i < 2; ++i)
        {
            // First the flags.
            int psdf = Reader_ReadByte(msgReader);
            ddpsprite_t *psp = ddpl->pSprites + i;

            if (psdf & PSDF_STATEPTR)
            {
                int idx = Reader_ReadPackedUInt16(msgReader);
                if (!idx)
                {
                    psp->statePtr = 0;
                }
                else if (idx < runtimeDefs.states.size())
                {
                    psp->statePtr = &runtimeDefs.states[idx - 1];
                    psp->tics = psp->statePtr->tics;
                }
            }

            /*if (psdf & PSDF_LIGHT)
            {
                psp->light = Reader_ReadByte(msgReader) / 255.0f;
            }*/

            if (psdf & PSDF_ALPHA)
            {
                psp->alpha = Reader_ReadByte(msgReader) / 255.0f;
            }

            if (psdf & PSDF_STATE)
            {
                psp->state = Reader_ReadByte(msgReader);
            }

            if (psdf & PSDF_OFFSET)
            {
                psp->offset[VX] = (char) Reader_ReadByte(msgReader) * 2;
                psp->offset[VY] = (char) Reader_ReadByte(msgReader) * 2;
            }
        }
    }
}

mobj_t *ClPlayer_LocalGameMobj(int plrNum)
{
    return DD_Player(plrNum)->publicData().mo;
}

bool ClPlayer_IsFreeToMove(int plrNum)
{
    mobj_t *mo = ClPlayer_LocalGameMobj(plrNum);
    if (!mo) return false;

    return (mo->origin[VZ] >= mo->floorZ &&
            mo->origin[VZ] + mo->height <= mo->ceilingZ);
}
