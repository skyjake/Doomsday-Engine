/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
#include "de_refresh.h"
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

static int fixSpeed = 15;
static float fixPos[3];
static int fixTics;
static float pspY;

// Console player demo momentum (used to smooth out abrupt momentum changes).
static float cpMom[3][LOCALCAM_WRITE_TICS];

// CODE --------------------------------------------------------------------

/**
 * Clears the player state table.
 */
void Cl_InitPlayers(void)
{
    fixTics = 0;
    pspY = 0;
    memset(&clPlayerStates, 0, sizeof(clPlayerStates));
    memset(fixPos, 0, sizeof(fixPos));
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
struct mobj_s* ClPlayer_ClMobj(int plrNum)
{
    assert(plrNum >= 0 && plrNum < DDMAXPLAYERS);
    return ClMobj_Find(clPlayerStates[plrNum].clMobjId);
}

/**
 * Move the (hidden, unlinked) client player mobj to the same coordinates
 * where the real mobj of the player is.
 */
void ClPlayer_UpdatePos(int plrNum)
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
    memcpy(remoteClientMobj->pos, localMobj->pos, sizeof(localMobj->pos));
    P_MobjLink(remoteClientMobj, 0); // Update subsector pointer.
    remoteClientMobj->floorZ = localMobj->floorZ;
    remoteClientMobj->ceilingZ = localMobj->ceilingZ;
    remoteClientMobj->mom[MX] = localMobj->mom[MX];
    remoteClientMobj->mom[MY] = localMobj->mom[MY];
    remoteClientMobj->mom[MZ] = localMobj->mom[MZ];
}

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

void ClPlayer_ApplyPendingFixes(int plrNum)
{
    clplayerstate_t *state = ClPlayer_State(plrNum);
    player_t        *plr = &ddPlayers[plrNum];
    mobj_t          *clmo = ClPlayer_ClMobj(plrNum);
    ddplayer_t      *ddpl = &plr->shared;
    mobj_t          *mo = ddpl->mo;

    // If either mobj is missing, the fix cannot be applied yet.
    if(!mo || !clmo) return;

    if(clmo->thinker.id != state->pendingFixTargetClMobjId)
        return;

    assert(clmo->thinker.id == state->clMobjId);

    if(state->pendingFixes & DDPF_FIXANGLES)
    {
        state->pendingFixes &= ~DDPF_FIXANGLES;

#ifdef _DEBUG
        Con_Message("ClPlayer_ApplyPendingFixes: Applying angle %x to mobj %p and clmo %i...\n",
                    state->pendingAngleFix, mo, clmo->thinker.id);
#endif
        clmo->angle = mo->angle = state->pendingAngleFix;
        ddpl->lookDir = state->pendingLookDirFix;
    }

    if(state->pendingFixes & DDPF_FIXPOS)
    {
        state->pendingFixes &= ~DDPF_FIXPOS;

#ifdef _DEBUG
        Con_Message("ClPlayer_ApplyPendingFixes: Applying pos (%f, %f, %f) to mobj %p and clmo %i...\n",
                    state->pendingPosFix[VX], state->pendingPosFix[VY], state->pendingPosFix[VZ],
                    mo, clmo->thinker.id);
#endif
        P_MobjSetPos(mo, state->pendingPosFix[VX], state->pendingPosFix[VY], state->pendingPosFix[VZ]);
        mo->reactionTime = 18;

        // The position is now known.
        ddpl->flags &= ~DDPF_UNDEFINED_POS;

        ClPlayer_UpdatePos(plrNum);
    }

    if(state->pendingFixes & DDPF_FIXMOM)
    {
        state->pendingFixes &= ~DDPF_FIXMOM;

#ifdef _DEBUG
        Con_Message("ClPlayer_ApplyPendingFixes: Applying mom (%f, %f, %f) to mobj %p and clmo %i...\n",
                    state->pendingMomFix[VX], state->pendingMomFix[VY], state->pendingMomFix[VZ],
                    mo, clmo->thinker.id);
#endif
        mo->mom[MX] = clmo->mom[VX] = state->pendingMomFix[VX];
        mo->mom[MY] = clmo->mom[VY] = state->pendingMomFix[VY];
        mo->mom[MZ] = clmo->mom[VZ] = state->pendingMomFix[VZ];
    }
}

void ClPlayer_HandleFix(void)
{
    player_t           *plr = &ddPlayers[consolePlayer];
    //mobj_t             *clmo = ClPlayer_ClMobj(consolePlayer);
    ddplayer_t         *ddpl = &plr->shared;
    //mobj_t             *mo = ddpl->mo;
    int                 fixes = Msg_ReadLong();
    clplayerstate_t    *state = ClPlayer_State(consolePlayer);

    state->pendingFixTargetClMobjId = Msg_ReadLong();

    if(fixes & 1) // fix angles?
    {
        ddpl->fixCounter.angles = ddpl->fixAcked.angles = Msg_ReadLong();
        state->pendingAngleFix = Msg_ReadLong();
        state->pendingLookDirFix = FIX2FLT(Msg_ReadLong());
        state->pendingFixes |= DDPF_FIXANGLES;

#ifdef _DEBUG
        Con_Message("Cl_HandlePlayerFix: Fix angles %i. Angle=%f, lookdir=%f\n",
                    ddpl->fixAcked.angles, FIX2FLT(state->pendingAngleFix), state->pendingLookDirFix);
#endif
    }

    if(fixes & 2) // fix pos?
    {
        ddpl->fixCounter.pos = ddpl->fixAcked.pos = Msg_ReadLong();
        state->pendingPosFix[VX] = FIX2FLT(Msg_ReadLong());
        state->pendingPosFix[VY] = FIX2FLT(Msg_ReadLong());
        state->pendingPosFix[VZ] = FIX2FLT(Msg_ReadLong());
        state->pendingFixes |= DDPF_FIXPOS;

#ifdef _DEBUG
        Con_Message("Cl_HandlePlayerFix: Fix pos %i. Pos=%f, %f, %f\n",
                    ddpl->fixAcked.pos, state->pendingPosFix[VX], state->pendingPosFix[VY], state->pendingPosFix[VZ]);
#endif
    }

    if(fixes & 4) // fix momentum?
    {
        ddpl->fixCounter.mom = ddpl->fixAcked.mom = Msg_ReadLong();
        state->pendingMomFix[VX] = FIX2FLT(Msg_ReadLong());
        state->pendingMomFix[VY] = FIX2FLT(Msg_ReadLong());
        state->pendingMomFix[VZ] = FIX2FLT(Msg_ReadLong());
        state->pendingFixes |= DDPF_FIXMOM;
    }

    ClPlayer_ApplyPendingFixes(consolePlayer);

    // Send an acknowledgement.
    Msg_Begin(PCL_ACK_PLAYER_FIX);
    Msg_WriteLong(ddpl->fixAcked.angles);
    Msg_WriteLong(ddpl->fixAcked.pos);
    Msg_WriteLong(ddpl->fixAcked.mom);
    Net_SendBuffer(0, SPF_ORDERED | SPF_CONFIRM);
}

/**
 * Used in DEMOS. (Not in regular netgames.)
 * Applies the given dx and dy to the local player's coordinates.
 *
 * @param z             Absolute viewpoint height.
 * @param onground      If @c true the mobj's Z will be set to floorz, and
 *                      the player's viewheight is set so that the viewpoint
 *                      height is param 'z'.
 *                      If @c false the mobj's Z will be param 'z' and
 *                      viewheight is zero.
 */
void ClPlayer_MoveLocal(float dx, float dy, float z, boolean onground)
{
    player_t           *plr = &ddPlayers[consolePlayer];
    ddplayer_t         *ddpl = &plr->shared;
    mobj_t             *mo = ddpl->mo;
    int                 i;
    float               mom[3];

    if(!mo)
        return;

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
        mo->pos[VX] += dx;
        mo->pos[VY] += dy;
        P_MobjLink(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
    }

    mo->subsector = R_PointInSubsector(mo->pos[VX], mo->pos[VY]);
    mo->floorZ = mo->subsector->sector->SP_floorheight;
    mo->ceilingZ = mo->subsector->sector->SP_ceilheight;

    if(onground)
    {
        mo->pos[VZ] = z - 1;
    }
    else
    {
        mo->pos[VZ] = z;
    }

    ClPlayer_UpdatePos(consolePlayer);
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
    int                 num, newId;
    short               junk;

    // The first byte consists of a player number and some flags.
    num = Msg_ReadByte();
    df = (num & 0xf0) << 8;
    df |= Msg_ReadByte(); // Second byte is just flags.
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

        newId = Msg_ReadShort();

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
                ClMobj_UnsetPosition(clmo);
            }

            clmo->dPlayer = ddpl;

            // Make the old clmobj a non-player one (if any).
            if(old)
            {
                old->dPlayer = NULL;
                ClMobj_SetPosition(old);
            }

            // If it was just created, the coordinates are not yet correct.
            // The update will be made when the mobj data is received.
            if(!justCreated) // && num != consolePlayer)
            {
#ifdef _DEBUG
                Con_Message("ClPlayer_ReadDelta2: Copying clmo %i state to real player %i mobj %p.\n",
                            newId, num, ddpl->mo);
#endif
                Cl_UpdateRealPlayerMobj(ddpl->mo, clmo, 0xffffffff);
            }
            /*
            else if(ddpl->mo)
            {
                // Update the new client mobj's information from the real
                // mobj, which is already known.
#if _DEBUG
                Con_Message("Cl_RdPlrD2: Pl%i: Copying pos&angle from real mobj to clmobj.\n", num);
                Con_Message("  x=%g y=%g z=%g\n", ddpl->mo->pos[VX], ddpl->mo->pos[VY], ddpl->mo->pos[VZ]);
#endif
                clmo->pos[VX] = ddpl->mo->pos[VX];
                clmo->pos[VY] = ddpl->mo->pos[VY];
                clmo->pos[VZ] = ddpl->mo->pos[VZ];
                clmo->angle = ddpl->mo->angle;
                if(!skip)
                    ClPlayer_UpdatePos(num);
            }
            */

#if _DEBUG
            Con_Message("ClPlr_RdD2: Pl%i: mobj=%i old=%p\n", num, s->clMobjId, old);
            Con_Message("  x=%g y=%g z=%g fz=%g cz=%g\n", clmo->pos[VX],
                        clmo->pos[VY], clmo->pos[VZ], clmo->floorZ, clmo->ceilingZ);
            Con_Message("ClPlr_RdD2: pl=%i => moid=%i\n", (skip? -1 : num), s->clMobjId);
#endif
        }
    }

    if(df & PDF_FORWARDMOVE)
        s->forwardMove = (char) Msg_ReadByte() * 2048;
    if(df & PDF_SIDEMOVE)
        s->sideMove = (char) Msg_ReadByte() * 2048;
    if(df & PDF_ANGLE)
        //s->angle = Msg_ReadByte() << 24;
        junk = Msg_ReadByte();
    if(df & PDF_TURNDELTA)
    {
        s->turnDelta = ((char) Msg_ReadByte() << 24) / 16;
    }
    if(df & PDF_FRICTION)
        s->friction = Msg_ReadByte() << 8;
    if(df & PDF_EXTRALIGHT)
    {
        i = Msg_ReadByte();
        ddpl->fixedColorMap = i & 7;
        ddpl->extraLight = i & 0xf8;
    }
    if(df & PDF_FILTER)
    {
        unsigned int filter = Msg_ReadLong();

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
    if(df & PDF_CLYAW) // Only sent when Fixangles is used.
        //ddpl->clAngle = Msg_ReadShort() << 16; /* $unifiedangles */
        junk = Msg_ReadShort();
    if(df & PDF_CLPITCH) // Only sent when Fixangles is used.
        //ddpl->clLookDir = Msg_ReadShort() * 110.0 / DDMAXSHORT; /* $unifiedangles */
        junk = Msg_ReadShort();
    if(df & PDF_PSPRITES)
    {
        for(i = 0; i < 2; ++i)
        {
            // First the flags.
            psdf = Msg_ReadByte();
            psp = ddpl->pSprites + i;
            if(psdf & PSDF_STATEPTR)
            {
                idx = Msg_ReadPackedShort();
                if(!idx)
                    psp->statePtr = 0;
                else if(idx < countStates.num)
                {
                    psp->statePtr = states + (idx - 1);
                    psp->tics = psp->statePtr->tics;
                }
            }

            /*if(psdf & PSDF_LIGHT)
                psp->light = Msg_ReadByte() / 255.0f;*/
            if(psdf & PSDF_ALPHA)
                psp->alpha = Msg_ReadByte() / 255.0f;
            if(psdf & PSDF_STATE)
                psp->state = Msg_ReadByte();
            if(psdf & PSDF_OFFSET)
            {
                psp->offset[VX] = (char) Msg_ReadByte() * 2;
                psp->offset[VY] = (char) Msg_ReadByte() * 2;
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

    return (mo->pos[VZ] >= mo->floorZ &&
            mo->pos[VZ] + mo->height <= mo->ceilingZ);
}
