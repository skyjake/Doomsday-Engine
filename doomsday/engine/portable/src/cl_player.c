/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
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

#define TOP_PSPY        32
#define BOTTOM_PSPY     128

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

playerstate_t playerstate[MAXPLAYERS];
int     psp_move_speed = 6 * FRACUNIT;
int     cplr_thrust_mul = FRACUNIT;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int fixspeed = 15;
static int xfix, yfix, fixtics;
static float pspy;

// Console player demo momentum (used to smooth out abrupt momentum changes).
static int cp_momx[LOCALCAM_WRITE_TICS], cp_momy[LOCALCAM_WRITE_TICS];

// CODE --------------------------------------------------------------------

/*
 * Clears the player state table.
 */
void Cl_InitPlayers(void)
{
    int     i;

    memset(playerstate, 0, sizeof(playerstate));
    xfix = yfix = fixtics = 0;
    pspy = 0;
    memset(cp_momx, 0, sizeof(cp_momx));
    memset(cp_momy, 0, sizeof(cp_momy));

    // Clear psprites. The server will send them.
    for(i = 0; i < MAXPLAYERS; i++)
    {
        //players[i].psprites[0].stateptr = NULL;
        //players[i].psprites[1].stateptr = NULL;
        memset(clients[i].lastCmd, 0, sizeof(*clients[i].lastCmd));
    }
}

/*
 * Updates the state of the local player by looking at lastCmd.
 */
void Cl_LocalCommand(void)
{
    ddplayer_t *pl = players + consoleplayer;
    client_t *cl = clients + consoleplayer;
    playerstate_t *s = playerstate + consoleplayer;

    if(levelTime < 0.333)
    {
        // In the very beginning of a level, moving is not allowed.
        memset(cl->lastCmd, 0, TICCMD_SIZE);
        if(s->cmo)
        {
            s->cmo->mo.momx = 0;
            s->cmo->mo.momy = 0;
        }
    }

    s->forwardMove = cl->lastCmd->forwardMove * 2048;
    s->sideMove = cl->lastCmd->sideMove * 2048;
    s->angle = pl->mo->angle; //pl->clAngle; /* $unifiedangles */
    s->turnDelta = 0;
}

/*
 * Reads a single player delta from the message buffer and applies
 * it to the player in question. Returns false only if the list of
 * deltas ends.
 *
 * THIS FUNCTION IS NOW OBSOLETE (only used with PSV_FRAME packets)
 */
int Cl_ReadPlayerDelta(void)
{
    int     df, psdf, i, idx;
    int     num = Msg_ReadByte();
    playerstate_t *s;
    ddplayer_t *pl;
    ddpsprite_t *psp;

    if(num == 0xff)
        return false;           // End of list.

    // The first byte consists of a player number and some flags.
    df = (num & 0xf0) << 8;
    df |= Msg_ReadByte();       // Second byte is just flags.
    num &= 0xf;                 // Clear the upper bits of the number.

    s = playerstate + num;
    pl = players + num;

    if(df & PDF_MOBJ)
    {
        clmobj_t *old = s->cmo;
        int     newid = Msg_ReadShort();

        // Make sure the 'new' mobj is different than the old one;
        // there will be linking problems otherwise.
        // FIXME: What causes the duplicate sending of mobj ids?
        if(newid != s->mobjId)
        {
            s->mobjId = newid;

            // Find the new mobj.
            s->cmo = Cl_FindMobj(s->mobjId);
#ifdef _DEBUG
            Con_Message("Pl%i: mobj=%i old=%x\n", num, s->mobjId, old);
            Con_Message("  x=%x y=%x z=%x\n", s->cmo->mo.pos[VX],
                        s->cmo->mo.pos[VY], s->cmo->mo.pos[VZ]);
#endif
            s->cmo->mo.dplayer = pl;

#ifdef _DEBUG
            Con_Message("Cl_RPlD: pl=%i => moid=%i\n",
                        s->cmo->mo.dplayer - players, s->mobjId);
#endif

            // Unlink this cmo (not interactive or visible).
            Cl_UnsetThingPosition(s->cmo);
            // Make the old clmobj a non-player one.
            if(old)
            {
                old->mo.dplayer = NULL;
                Cl_SetThingPosition(old);
                Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, ~0);
            }
            else
            {
                //Cl_UpdatePlayerPos(pl);

                // Replace the hidden client mobj with the real player mobj.
                Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, ~0);
            }
            // Update the real player mobj.
            //Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, ~0);
        }
    }
    if(df & PDF_FORWARDMOVE)
        s->forwardMove = (char) Msg_ReadByte() * 2048;
    if(df & PDF_SIDEMOVE)
        s->sideMove = (char) Msg_ReadByte() * 2048;
    if(df & PDF_ANGLE)
        /*s->angle =*/ Msg_ReadByte() << 24;
    if(df & PDF_TURNDELTA)
    {
        s->turnDelta = ((char) Msg_ReadByte() << 24) / 16;
    }
    if(df & PDF_FRICTION)
        s->friction = Msg_ReadByte() << 8;
    if(df & PDF_EXTRALIGHT)
    {
        i = Msg_ReadByte();
        pl->fixedcolormap = i & 7;
        pl->extralight = i & 0xf8;
    }
    if(df & PDF_FILTER)
        pl->filter = Msg_ReadLong();
    if(df & PDF_CLYAW)          // Only sent when Fixangles is used.
        /*pl->clAngle = */ Msg_ReadShort() << 16; /* $unifiedangles */
    if(df & PDF_CLPITCH)        // Only sent when Fixangles is used.
        /*pl->clLookDir =*/ Msg_ReadShort() * 110.0 / DDMAXSHORT; /* $unifiedangles */
    if(df & PDF_PSPRITES)
    {
        for(i = 0; i < 2; i++)
        {
            // First the flags.
            psdf = Msg_ReadByte();
            psp = pl->psprites + i;
            if(psdf & PSDF_STATEPTR)
            {
                idx = Msg_ReadPackedShort();
                if(!idx)
                    psp->stateptr = 0;
                else if(idx < count_states.num)
                {
                    psp->stateptr = states + (idx - 1);
                    psp->tics = psp->stateptr->tics;
                }
            }
            //if(psdf & PSDF_SPRITE) psp->sprite = Msg_ReadPackedShort() - 1;
            //if(psdf & PSDF_FRAME) psp->frame = Msg_ReadByte();
            //if(psdf & PSDF_NEXT/*FRAME*/) psp->nextframe = (char) Msg_ReadByte();
            //if(psdf & PSDF_NEXT/*TIME*/) psp->nexttime = (char) Msg_ReadByte();
            //if(psdf & PSDF_TICS) psp->tics = (char) Msg_ReadByte();
            if(psdf & PSDF_LIGHT)
                psp->light = Msg_ReadByte() / 255.0f;
            if(psdf & PSDF_ALPHA)
                psp->alpha = Msg_ReadByte() / 255.0f;
            if(psdf & PSDF_STATE)
                psp->state = Msg_ReadByte();
            if(psdf & PSDF_OFFSET)
            {
                psp->offx = (char) Msg_ReadByte() * 2;
                psp->offy = (char) Msg_ReadByte() * 2;
            }
        }
    }

    // Continue reading.
    return true;
}

/*
 * Thrust (with a multiplier).
 */
void Cl_ThrustMul(mobj_t *mo, angle_t angle, fixed_t move, fixed_t thmul)
{
    // Make a fine angle.
    angle >>= ANGLETOFINESHIFT;
    move = FixedMul(move, thmul);
    mo->momx += FixedMul(move, finecosine[angle]);
    mo->momy += FixedMul(move, finesine[angle]);
}

void Cl_Thrust(mobj_t *mo, angle_t angle, fixed_t move)
{
    Cl_ThrustMul(mo, angle, move, FRACUNIT);
}

/*
 * Predict the movement of the given player.
 */
void Cl_MovePlayer(ddplayer_t *pl)
{
    int     num = pl - players;
    playerstate_t *st = playerstate + num;
    mobj_t *mo = pl->mo;

    if(!mo)
        return;

    if(playback && num == consoleplayer)
    {
        // This kind of player movement can't be used with demos.
        // The local player movement is recorded into the demo file as
        // coordinates.
        return;
    }

    // Move.
    P_ThingMovement2(mo, st);
    P_ThingZMovement(mo);

    // Predict change in movement (thrust).
    // The console player is always affected by the thrust multiplier.
    // (Other players are never handled because clients only receive mobj
    // information about non-local player movement.)
    if(num == consoleplayer)
    {
        fixed_t air_thrust = FRACUNIT / 32;
        boolean airborne = (mo->pos[VZ] > mo->floorz && !(mo->ddflags & DDMF_FLY));

        if(!(pl->flags & DDPF_DEAD) && !mo->reactiontime)    // Dead players do not move willfully.
        {
            int     mul = (airborne ? air_thrust : cplr_thrust_mul);

            if(st->forwardMove)
                Cl_ThrustMul(mo, st->angle, st->forwardMove, mul);
            if(st->sideMove)
                Cl_ThrustMul(mo, st->angle - ANG90, st->sideMove, mul);
        }
        // Turn delta on move prediction angle.
        st->angle += st->turnDelta;
        //mo->angle += st->turnDelta;
    }

    // Mirror changes in the (hidden) client mobj.
    Cl_UpdatePlayerPos(pl);
}

/*
 * Move the (hidden, unlinked) client player mobj to the same coordinates
 * where the real mobj of the player is.
 */
void Cl_UpdatePlayerPos(ddplayer_t *pl)
{
    int     num = pl - players;
    mobj_t *clmo, *mo;

    if(!playerstate[num].cmo || !pl->mo)
        return;                 // Must have a mobj!
      
    clmo = &playerstate[num].cmo->mo;
    mo = pl->mo;
    clmo->angle = mo->angle;
    // The player's client mobj is not linked to any lists, so position
    // can be updated without any hassles.
    memcpy(clmo->pos, mo->pos, sizeof(mo->pos));
    P_LinkThing(clmo, 0);       // Update subsector pointer.
    clmo->floorz = mo->floorz;
    clmo->ceilingz = mo->ceilingz;
    clmo->momx = mo->momx;
    clmo->momy = mo->momy;
    clmo->momz = mo->momz;
}

void Cl_CoordsReceived(void)
{
    if(playback)
        return;

#ifdef _DEBUG
    Con_Printf("Cl_CoordsReceived\n");
#endif

    xfix = Msg_ReadShort() << 16;
    yfix = Msg_ReadShort() << 16;
    fixtics = fixspeed;
    xfix /= fixspeed;
    yfix /= fixspeed;
}

void Cl_HandlePlayerFix(void)
{
    ddplayer_t* plr = &players[consoleplayer];
    int fixes = Msg_ReadLong();
    angle_t angle;
    float lookdir;
    fixed_t pos[3];
    mobj_t *mo = plr->mo;
    clmobj_t *clmo = playerstate[consoleplayer].cmo;
       
    if(fixes & 1) // fix angles?
    {
        plr->fixcounter.angles = plr->fixacked.angles = Msg_ReadLong();
        angle = Msg_ReadLong();
        lookdir = FIX2FLT(Msg_ReadLong());

#ifdef _DEBUG
        Con_Message("Cl_HandlePlayerFix: Fix angles %i. Angle=%f, lookdir=%f\n",
                    plr->fixacked.angles, FIX2FLT(angle), lookdir);
#endif
        if(mo)
        {
#ifdef _DEBUG
            Con_Message("  Applying to mobj %p...\n", mo);
#endif
            mo->angle = angle;
            plr->lookdir = lookdir;
        }
        if(clmo)
        {
#ifdef _DEBUG
            Con_Message("  Applying to clmobj %i...\n", clmo->mo.thinker.id);
#endif
            clmo->mo.angle = angle;
        }       
    }

    if(fixes & 2) // fix pos?
    {
        plr->fixcounter.pos = plr->fixacked.pos = Msg_ReadLong();
        pos[0] = Msg_ReadLong();
        pos[1] = Msg_ReadLong();
        pos[2] = Msg_ReadLong();
        
#ifdef _DEBUG
        Con_Message("Cl_HandlePlayerFix: Fix pos %i. Pos=%f, %f, %f\n",
                    plr->fixacked.pos, FIX2FLT(pos[0]),
                    FIX2FLT(pos[1]), FIX2FLT(pos[2]));
#endif
        if(mo)
        {
#ifdef _DEBUG
            Con_Message("  Applying to mobj %p...\n", mo);
#endif
            Sv_PlaceThing(mo, pos[0], pos[1], pos[2], false);
            mo->reactiontime = 18;
        }
        if(clmo)
        {
#ifdef _DEBUG
            Con_Message("  Applying to clmobj %i...\n", clmo->mo.thinker.id);
#endif
            Cl_UpdatePlayerPos(plr);
        }       
    }

    if(fixes & 4) // fix momentum?
    {
        plr->fixcounter.mom = plr->fixacked.mom = Msg_ReadLong();
        pos[0] = Msg_ReadLong();
        pos[1] = Msg_ReadLong();
        pos[2] = Msg_ReadLong();
        
#ifdef _DEBUG
        Con_Message("Cl_HandlePlayerFix: Fix momentum %i. Mom=%f, %f, %f\n",
                    plr->fixacked.mom, FIX2FLT(pos[0]),
                    FIX2FLT(pos[1]), FIX2FLT(pos[2]));
#endif
        if(mo)
        {
#ifdef _DEBUG
            Con_Message("  Applying to mobj %p...\n", mo);
#endif
            mo->momx = pos[0];
            mo->momy = pos[1];
            mo->momz = pos[2];
        }
        if(clmo)
        {
#ifdef _DEBUG
            Con_Message("  Applying to clmobj %i...\n", clmo->mo.thinker.id);
#endif
            clmo->mo.momx = pos[0];
            clmo->mo.momy = pos[1];
            clmo->mo.momz = pos[2];
        }       
    }
    
    // Send an acknowledgement.
    Msg_Begin(PCL_ACK_PLAYER_FIX);
    Msg_WriteLong(plr->fixacked.angles);
    Msg_WriteLong(plr->fixacked.pos);
    Msg_WriteLong(plr->fixacked.mom);
    Net_SendBuffer(0, SPF_ORDERED | SPF_CONFIRM);
}

/*
 * Used in DEMOS. (Not in regular netgames.)
 * Applies the given dx and dy to the local player's coordinates.
 * The Z coordinate is given as the absolute viewpoint height.
 * If onground is true, the mobj's Z will be set to floorz, and the
 * player's viewheight is set so that the viewpoint height is 'z'.
 * If onground is false, the mobj's Z will be 'z' and viewheight is zero.
 */
void Cl_MoveLocalPlayer(int dx, int dy, int z, boolean onground)
{
    ddplayer_t *pl = players + consoleplayer;
    mobj_t *mo;
    int     i;

    mo = pl->mo;
    if(!mo)
        return;

    // Place the new momentum in the appropriate place.
    cp_momx[SECONDS_TO_TICKS(gameTime) % LOCALCAM_WRITE_TICS] = dx;
    cp_momy[SECONDS_TO_TICKS(gameTime) % LOCALCAM_WRITE_TICS] = dy;

    // Calculate an average.
    for(mo->momx = mo->momy = i = 0; i < LOCALCAM_WRITE_TICS; i++)
    {
        mo->momx += cp_momx[i];
        mo->momy += cp_momy[i];
    }
    mo->momx /= LOCALCAM_WRITE_TICS;
    mo->momy /= LOCALCAM_WRITE_TICS;

    if(dx || dy)
    {
        P_UnlinkThing(mo);
        mo->pos[VX] += dx;
        mo->pos[VY] += dy;
        P_LinkThing(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
    }

    mo->subsector = R_PointInSubsector(mo->pos[VX], mo->pos[VY]);
    mo->floorz = mo->subsector->sector->SP_floorheight;
    mo->ceilingz = mo->subsector->sector->SP_ceilheight;

    if(onground)
    {
        mo->pos[VZ] = z - 1;
        pl->viewheight = 1;
    }
    else
    {
        mo->pos[VZ] = z;
        pl->viewheight = 0;
    }

    Cl_UpdatePlayerPos(players + consoleplayer);
}

/*
 * Animates the player sprites based on their states (up, down, etc.)
 */
/*
void Cl_MovePsprites(void)
{
    ddplayer_t *pl = players + consoleplayer;
    ddpsprite_t *psp = pl->psprites;
    int     i;

    for(i = 0; i < 2; i++)
        if(psp[i].tics > 0)
            psp[i].tics--;

    switch (psp->state)
    {
    case DDPSP_UP:
        pspy -= FIX2FLT(psp_move_speed);
        if(pspy <= TOP_PSPY)
        {
            pspy = TOP_PSPY;
            psp->state = DDPSP_BOBBING;
        }
        psp->y = pspy;
        break;

    case DDPSP_DOWN:
        pspy += FIX2FLT(psp_move_speed);
        if(pspy > BOTTOM_PSPY)
            pspy = BOTTOM_PSPY;
        psp->y = pspy;
        break;

    case DDPSP_FIRE:
        pspy = TOP_PSPY;
        //psp->x = 0;
        psp->y = pspy;
        break;

    case DDPSP_BOBBING:
        pspy = TOP_PSPY;
        // Get bobbing from the Game DLL.
        psp->x = FIX2FLT((fixed_t) gx.GetInteger(DD_PSPRITE_BOB_X));
        psp->y = FIX2FLT((fixed_t) gx.GetInteger(DD_PSPRITE_BOB_Y));
        break;
    }
    if(psp->state != DDPSP_BOBBING)
    {
        if(psp->offx)
            psp->x = psp->offx;
        if(psp->offy)
            psp->y = psp->offy;
    }

    // The other psprite gets the same coords.
    psp[1].x = psp->x;
    psp[1].y = psp->y;
}*/

/*
 * Reads a single PSV_FRAME2 player delta from the message buffer and
 * applies it to the player in question.
 */
void Cl_ReadPlayerDelta2(boolean skip)
{
    int     df = 0, psdf, i, idx;
    playerstate_t *s;
    ddplayer_t *pl;
    ddpsprite_t *psp;
    static playerstate_t dummyState;
    static ddplayer_t dummyPlayer;
    int     num, newId;

    // The first byte consists of a player number and some flags.
    num = Msg_ReadByte();
    df = (num & 0xf0) << 8;
    df |= Msg_ReadByte();       // Second byte is just flags.
    num &= 0xf;                 // Clear the upper bits of the number.

    if(!skip)
    {
        s = playerstate + num;
        pl = players + num;
    }
    else
    {
        // We're skipping, read the data into dummies.
        s = &dummyState;
        pl = &dummyPlayer;
    }

    if(df & PDF_MOBJ)
    {
        clmobj_t *old = s->cmo;

        newId = Msg_ReadShort();

        // Make sure the 'new' mobj is different than the old one;
        // there will be linking problems otherwise.
        if(!skip && newId != s->mobjId)
        {
            boolean justCreated = false;

            s->mobjId = newId;

            // Find the new mobj.
            s->cmo = Cl_FindMobj(s->mobjId);
            if(!s->cmo)
            {
                // This mobj hasn't yet been sent to us.
                // We should be receiving the rest of the info very shortly.
                s->cmo = Cl_CreateMobj(s->mobjId);
                if(num == consoleplayer)
                {
                    // Mark everything known about our local player.
                    s->cmo->flags |= CLMF_KNOWN;
                }
                justCreated = true;
            }
            else
            {
                // The client mobj is already known to us.
                // Unlink it (not interactive or visible).
                Cl_UnsetThingPosition(s->cmo);
            }

            s->cmo->mo.dplayer = pl;

            // Make the old clmobj a non-player one (if any).
            if(old)
            {
                old->mo.dplayer = NULL;
                Cl_SetThingPosition(old);
            }

            // If it was just created, the coordinates are not yet correct.
            // The update will be made when the mobj data is received.
            if(!justCreated)
            {
                // Replace the hidden client mobj with the real player mobj.
                Cl_UpdateRealPlayerMobj(pl->mo, &s->cmo->mo, 0xffffffff);
            }
            else
            {
                // Update the new client mobj's information from the real
                // mobj, which is already known.
                s->cmo->mo.pos[VX] = pl->mo->pos[VX];
                s->cmo->mo.pos[VY] = pl->mo->pos[VY];
                s->cmo->mo.pos[VZ] = pl->mo->pos[VZ];
                s->cmo->mo.angle = pl->mo->angle;
                Cl_UpdatePlayerPos(pl);
            }

#if _DEBUG
            Con_Message("Cl_RdPlrD2: Pl%i: mobj=%i old=%x\n", num, s->mobjId,
                        old);
            Con_Message("  x=%x y=%x z=%x fz=%x cz=%x\n", s->cmo->mo.pos[VX],
                        s->cmo->mo.pos[VY], s->cmo->mo.pos[VZ],
                        s->cmo->mo.floorz, s->cmo->mo.ceilingz);
            Con_Message("Cl_RdPlrD2: pl=%i => moid=%i\n",
                        s->cmo->mo.dplayer - players, s->mobjId);
#endif
        }
    }
    if(df & PDF_FORWARDMOVE)
        s->forwardMove = (char) Msg_ReadByte() * 2048;
    if(df & PDF_SIDEMOVE)
        s->sideMove = (char) Msg_ReadByte() * 2048;
    if(df & PDF_ANGLE)
        /*s->angle =*/ Msg_ReadByte() << 24;
    if(df & PDF_TURNDELTA)
    {
        s->turnDelta = ((char) Msg_ReadByte() << 24) / 16;
    }
    if(df & PDF_FRICTION)
        s->friction = Msg_ReadByte() << 8;
    if(df & PDF_EXTRALIGHT)
    {
        i = Msg_ReadByte();
        pl->fixedcolormap = i & 7;
        pl->extralight = i & 0xf8;
    }
    if(df & PDF_FILTER)
        pl->filter = Msg_ReadLong();
    if(df & PDF_CLYAW)          // Only sent when Fixangles is used.
        /*pl->clAngle =*/ Msg_ReadShort() << 16; /* $unifiedangles */
    if(df & PDF_CLPITCH)        // Only sent when Fixangles is used.
        /*pl->clLookDir =*/ Msg_ReadShort() * 110.0 / DDMAXSHORT; /* $unifiedangles */
    if(df & PDF_PSPRITES)
    {
        for(i = 0; i < 2; i++)
        {
            // First the flags.
            psdf = Msg_ReadByte();
            psp = pl->psprites + i;
            if(psdf & PSDF_STATEPTR)
            {
                idx = Msg_ReadPackedShort();
                if(!idx)
                    psp->stateptr = 0;
                else if(idx < count_states.num)
                {
                    psp->stateptr = states + (idx - 1);
                    psp->tics = psp->stateptr->tics;
                }
            }
            if(psdf & PSDF_LIGHT)
                psp->light = Msg_ReadByte() / 255.0f;
            if(psdf & PSDF_ALPHA)
                psp->alpha = Msg_ReadByte() / 255.0f;
            if(psdf & PSDF_STATE)
                psp->state = Msg_ReadByte();
            if(psdf & PSDF_OFFSET)
            {
                psp->offx = (char) Msg_ReadByte() * 2;
                psp->offy = (char) Msg_ReadByte() * 2;
            }
        }
    }
}

/*
 * Returns true if the player is free to move according to floorz and
 * ceilingz. This test is used by the client plane mover.
 */
boolean Cl_IsFreeToMove(int player)
{
    mobj_t *mo = players[player].mo;

    if(!mo)
        return false;
    return (mo->pos[VZ] >= mo->floorz && mo->pos[VZ] + mo->height <= mo->ceilingz);
}
