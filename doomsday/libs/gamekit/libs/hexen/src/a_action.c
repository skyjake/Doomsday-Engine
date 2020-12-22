/** @file a_action.c
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "jhexen.h"

#include <string.h>
#include <math.h>
#include "g_common.h"
#include "mobj.h"
#include "p_map.h"

#define TELEPORT_LIFE           (1)

static coord_t* orbitTableX = NULL;
static coord_t* orbitTableY = NULL;

coord_t* FloatBobOffset = NULL;

int localQuakeHappening[MAXPLAYERS];
int localQuakeTimeout[MAXPLAYERS];

void X_CreateLUTs(void)
{
#define ORBITRES            256

    uint i;

    orbitTableX = Z_Malloc(sizeof(coord_t) * ORBITRES, PU_GAMESTATIC, 0);
    for(i = 0; i < ORBITRES; ++i)
        orbitTableX[i] = cos(((coord_t) i) / 40.74f) * 15;

    orbitTableY = Z_Malloc(sizeof(coord_t) * ORBITRES, PU_GAMESTATIC, 0);
    for(i = 0; i < ORBITRES; ++i)
        orbitTableY[i] = sin(((coord_t) i) / 40.74f) * 15;

    FloatBobOffset = Z_Malloc(sizeof(coord_t) * FLOATBOBRES, PU_GAMESTATIC, 0);
    for(i = 0; i < FLOATBOBRES; ++i)
        FloatBobOffset[i] = sin(((coord_t) i) / 10.186f) * 8;

#undef ORBITRES
}

void X_DestroyLUTs(void)
{
    Z_Free(orbitTableX);
    Z_Free(orbitTableY);
    Z_Free(FloatBobOffset);
}

void C_DECL A_PotteryExplode(mobj_t* actor)
{
    int i, maxBits = (P_Random() & 3) + 3;
    mobj_t *potteryBit = NULL;

    for(i = 0; i < maxBits; ++i)
    {
        if((potteryBit = P_SpawnMobj(MT_POTTERYBIT1, actor->origin, P_Random() << 24, 0)))
        {
            P_MobjChangeState(potteryBit, P_GetState(potteryBit->type, SN_SPAWN) + (P_Random() % 5));

            potteryBit->mom[MZ] = FIX2FLT(((P_Random() & 7) + 5) * (3 * FRACUNIT / 4));
            potteryBit->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
            potteryBit->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        }
    }

    S_StartSound(SFX_POTTERY_EXPLODE, potteryBit);

    if(actor->args[0])
    {
        // Spawn an item.
        if(!gfw_Rule(noMonsters) ||
           !(MOBJINFO[TranslateThingType[actor->args[0]]].
             flags & MF_COUNTKILL))
        {
            // Only spawn monsters if not -nomonsters.
            P_SpawnMobj(TranslateThingType[actor->args[0]], actor->origin,
                           actor->angle, 0);
        }
    }

    P_MobjRemove(actor, false);
}

void C_DECL A_PotteryChooseBit(mobj_t* actor)
{
    P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH) + (P_Random() % 5) + 1);
    actor->tics = 256 + (P_Random() << 1);
}

void C_DECL A_PotteryCheck(mobj_t* actor)
{
    mobj_t* pmo;

    if(!IS_NETGAME)
    {
        pmo = players[CONSOLEPLAYER].plr->mo;
        if(P_CheckSight(actor, pmo) &&
           (abs((int32_t)(M_PointToAngle2(pmo->origin, actor->origin) - pmo->angle)) <= ANGLE_45))
        {
            // Previous state (pottery bit waiting state).
            P_MobjChangeState(actor, actor->state - &STATES[0] - 1);
        }

        return;
    }
    else
    {
        int i;
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame) continue;

            pmo = players[i].plr->mo;
            if(P_CheckSight(actor, pmo) &&
               (abs((int32_t)(M_PointToAngle2(pmo->origin, actor->origin) - pmo->angle)) <= ANGLE_45))
            {
                // Previous state (pottery bit waiting state).
                P_MobjChangeState(actor, actor->state - &STATES[0] - 1);
                return;
            }
        }
    }
}

void C_DECL A_CorpseBloodDrip(mobj_t* actor)
{
    if(P_Random() > 128) return;

    P_SpawnMobjXYZ(MT_CORPSEBLOODDRIP, actor->origin[VX], actor->origin[VY],
                                       actor->origin[VZ] + actor->height / 2, actor->angle, 0);
}

void C_DECL A_CorpseExplode(mobj_t* actor)
{
    int i, n;
    mobj_t* mo;

    for(i = (P_Random() & 3) + 3; i; i--)
    {
        if((mo = P_SpawnMobj(MT_CORPSEBIT, actor->origin, P_Random() << 24, 0)))
        {
            P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN) + (P_Random() % 3));

            mo->mom[MZ] = FIX2FLT((P_Random() & 7) + 5) * .75f;
            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        }
    }

    // Spawn a skull.
    if((mo = P_SpawnMobj(MT_CORPSEBIT, actor->origin, P_Random() << 24, 0)))
    {
        P_MobjChangeState(mo, S_CORPSEBIT_4);

        n = (P_Random() & 7) + 5;
        mo->mom[MZ] = FIX2FLT(n) * .75f;
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        S_StartSound(SFX_FIRED_DEATH, mo);
    }
    P_MobjRemove(actor, false);
}

#ifdef MSVC
// I guess the compiler gets confused by the multitude of P_Random()s...
#  pragma optimize("g", off)
#endif

void C_DECL A_LeafSpawn(mobj_t* actor)
{
    coord_t pos[3];
    mobj_t* mo;
    int i;

    for(i = (P_Random() & 3) + 1; i; i--)
    {
        pos[VX] = actor->origin[VX];
        pos[VY] = actor->origin[VY];
        pos[VZ] = actor->origin[VZ];

        pos[VX] += FIX2FLT((P_Random() - P_Random()) << 14);
        pos[VY] += FIX2FLT((P_Random() - P_Random()) << 14);
        pos[VZ] += FIX2FLT(P_Random() << 14);

        /// @todo  We should not be using the original indices to determine
        ///         the mobjtype. Use a local table instead.
        if((mo = P_SpawnMobj(MT_LEAF1 + (P_Random() & 1), pos,
                                actor->angle, 0)))
        {
            P_ThrustMobj(mo, actor->angle, FIX2FLT(P_Random() << 9) + 3);
            mo->target = actor;
            mo->special1 = 0;
        }
    }
}
#ifdef MSVC
#  pragma optimize("", on)
#endif

void C_DECL A_LeafThrust(mobj_t *actor)
{
    if(P_Random() > 96)
    {
        return;
    }
    actor->mom[MZ] += FIX2FLT(P_Random() << 9) + 1;
}

void C_DECL A_LeafCheck(mobj_t *actor)
{
    int                 n;

    actor->special1++;
    if(actor->special1 >= 20)
    {
        P_MobjChangeState(actor, S_NULL);
        return;
    }

    if(P_Random() > 64)
    {
        if(IS_ZERO(actor->mom[MX]) && IS_ZERO(actor->mom[MY]))
        {
            P_ThrustMobj(actor, actor->target->angle,
                        FIX2FLT(P_Random() << 9) + 1);
        }
        return;
    }

    P_MobjChangeState(actor, S_LEAF1_8);
    n = P_Random();
    actor->mom[MZ] = FIX2FLT(n << 9) + 1;
    P_ThrustMobj(actor, actor->target->angle, FIX2FLT(P_Random() << 9) + 2);
    actor->flags |= MF_MISSILE;
}

/**
 * Bridge variables
 *  Parent
 *      special1    true == removing from world
 *
 *  Child
 *      target      pointer to center mobj
 *      args[0]     angle of ball
 */

void C_DECL A_BridgeOrbit(mobj_t* actor)
{
    if(!actor) return;

    if(actor->target->special1)
    {
        P_MobjChangeState(actor, S_NULL);
    }
    actor->args[0] += 3;

    P_MobjUnlink(actor);

    actor->origin[VX] = actor->target->origin[VX];
    actor->origin[VY] = actor->target->origin[VY];

    actor->origin[VX] += orbitTableX[actor->args[0]];
    actor->origin[VY] += orbitTableY[actor->args[0]];

    P_MobjLink(actor);
}

void C_DECL A_BridgeInit(mobj_t* actor)
{
    byte startangle;
    mobj_t* ball1, *ball2, *ball3;

    startangle = P_Random();
    actor->special1 = 0;

    // Spawn triad into world.
    if((ball1 = P_SpawnMobj(MT_BRIDGEBALL, actor->origin, actor->angle, 0)))
    {
        ball1->args[0] = startangle;
        ball1->target = actor;
    }

    if((ball2 = P_SpawnMobj(MT_BRIDGEBALL, actor->origin, actor->angle, 0)))
    {
        ball2->args[0] = (startangle + 85) & 255;
        ball2->target = actor;
    }

    if((ball3 = P_SpawnMobj(MT_BRIDGEBALL, actor->origin, actor->angle, 0)))
    {
        ball3->args[0] = (startangle + 170) & 255;
        ball3->target = actor;
    }

    A_BridgeOrbit(ball1);
    A_BridgeOrbit(ball2);
    A_BridgeOrbit(ball3);
}

void C_DECL A_BridgeRemove(mobj_t *actor)
{
    actor->special1 = true; // Removing the bridge.
    actor->flags &= ~MF_SOLID;
    P_MobjChangeState(actor, S_FREE_BRIDGE1);
}

void C_DECL A_HideThing(mobj_t *actor)
{
    actor->flags2 |= MF2_DONTDRAW;
}

void C_DECL A_UnHideThing(mobj_t *actor)
{
    actor->flags2 &= ~MF2_DONTDRAW;
}

void C_DECL A_SetShootable(mobj_t *actor)
{
    actor->flags2 &= ~MF2_NONSHOOTABLE;
    actor->flags |= MF_SHOOTABLE;
}

void C_DECL A_UnSetShootable(mobj_t *actor)
{
    actor->flags2 |= MF2_NONSHOOTABLE;
    actor->flags &= ~MF_SHOOTABLE;
}

void C_DECL A_SetAltShadow(mobj_t *actor)
{
    actor->flags &= ~MF_SHADOW;
    actor->flags |= MF_ALTSHADOW;
}

void C_DECL A_ContMobjSound(mobj_t *actor)
{
    switch(actor->type)
    {
    case MT_SERPENTFX:
        S_StartSound(SFX_SERPENTFX_CONTINUOUS, actor);
        break;

    case MT_HAMMER_MISSILE:
        S_StartSound(SFX_FIGHTER_HAMMER_CONTINUOUS, actor);
        break;

    case MT_QUAKE_FOCUS:
        S_StartSound(SFX_EARTHQUAKE, actor);
        break;

    default:
        break;
    }
}

void C_DECL A_ESound(mobj_t *mo)
{
    int                 sound;

    switch(mo->type)
    {
    case MT_SOUNDWIND:
        sound = SFX_WIND;
        break;

    default:
        sound = SFX_NONE;
        break;
    }
    S_StartSound(sound, mo);
}

/**
 * NOTE: See p_enemy for variable descriptions.
 */
void C_DECL A_Summon(mobj_t* actor)
{
    mobj_t*             mo;

    if((mo = P_SpawnMobj(MT_MINOTAUR, actor->origin, actor->angle, 0)))
    {
        mobj_t*             master;

        if(P_TestMobjLocation(mo) == false || !actor->tracer)
        {   // Didn't fit - change back to item.
            P_MobjChangeState(mo, S_NULL);

            if((mo = P_SpawnMobj(MT_SUMMONMAULATOR, actor->origin, actor->angle, 0)))
                mo->flags2 |= MF2_DROPPED;

            return;
        }

        memcpy((void *) mo->args, &mapTime, sizeof(mapTime));
        master = actor->tracer;
        if(master->flags & MF_CORPSE)
        {   // Master dead.
            mo->tracer = NULL; // No master.
        }
        else
        {
            mo->tracer = actor->tracer; // Pointer to master (mobj_t *)
            P_GivePower(master->player, PT_MINOTAUR);
        }

        // Make smoke puff.
        P_SpawnMobj(MT_MNTRSMOKE, actor->origin, P_Random() << 24, 0);
        S_StartSound(SFX_MAULATOR_ACTIVE, actor);
    }
}

/**
 * Fog Variables:
 *
 *      args[0]     Speed (0..10) of fog
 *      args[1]     Angle of spread (0..128)
 *      args[2]     Frequency of spawn (1..10)
 *      args[3]     Lifetime countdown
 *      args[4]     Boolean: fog moving?
 *      special1        Internal:  Counter for spawn frequency
 *      special2        Internal:  Index into floatbob table
 */

void C_DECL A_FogSpawn(mobj_t* actor)
{
    mobj_t*             mo = NULL;
    mobjtype_t          type = 0;
    angle_t             delta, angle;

    if(actor->special1-- > 0)
        return;

    actor->special1 = actor->args[2]; // Reset frequency count.

    switch(P_Random() % 3)
    {
    case 0: type = MT_FOGPATCHS; break;
    case 1: type = MT_FOGPATCHM; break;
    case 2: type = MT_FOGPATCHL; break;
    }

    delta = actor->args[1];
    if(delta == 0)
        delta = 1;

    angle = ((P_Random() % delta) - (delta / 2));
    angle <<= 24;

    if((mo = P_SpawnMobj(type, actor->origin, actor->angle + angle, 0)))
    {
        mo->target = actor;
        if(actor->args[0] < 1)
            actor->args[0] = 1;
        mo->args[0] = (P_Random() % (actor->args[0])) + 1; // Random speed.
        mo->args[3] = actor->args[3]; // Set lifetime.
        mo->args[4] = 1; // Set to moving.
        mo->special2 = P_Random() & 63;
    }
}

void C_DECL A_FogMove(mobj_t* actor)
{
    coord_t speed = (coord_t) actor->args[0];
    uint an;

    if(!(actor->args[4]))
        return;

    if(actor->args[3]-- <= 0)
    {
        P_MobjChangeStateNoAction(actor, P_GetState(actor->type, SN_DEATH));
        return;
    }

    // Move the fog slightly/slowly up and down. Some fog patches are supposed
    // to move higher and some are supposed to stay close to the ground.
    // Unlike in the original Hexen, the move is done by applying momentum
    // to the cloud so that the movement is smooth.
    if((actor->args[3] % 4) == 0)
    {
        uint weaveindex = actor->special2;
        actor->mom[VZ] = FLOATBOBOFFSET(weaveindex) / TICSPERSEC;
        actor->special2 = (weaveindex + 1) & 63;
    }

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = speed * FIX2FLT(finecosine[an]);
    actor->mom[MY] = speed * FIX2FLT(finesine[an]);
}

void C_DECL A_PoisonBagInit(mobj_t *actor)
{
    mobj_t *mo;

    if((mo = P_SpawnMobjXYZ(MT_POISONCLOUD, actor->origin[VX], actor->origin[VY],
                           actor->origin[VZ] + 28, P_Random() << 24, 0)))
    {
        // Missile objects must move to impact other objects.
        mo->mom[MX] = FIX2FLT(1);
        mo->special1 = 24 + (P_Random() & 7);
        mo->special2 = 0;
        mo->target = actor->target;
        mo->radius = 20;
        mo->height = 30;
        mo->flags &= ~MF_NOCLIP;

        // Vanilla quirk: poison clouds spawned by mushrooms cannot be blasted (issue 911).
        if (actor->type == MT_ZPOISONSHROOM)
        {
            mo->flags3 |= MF3_NOBLAST;
        }
    }
}

void C_DECL A_PoisonBagCheck(mobj_t *actor)
{
    if(!--actor->special1)
    {
        P_MobjChangeState(actor, S_POISONCLOUD_X1);
    }
    else
    {
        return;
    }
}

void C_DECL A_PoisonBagDamage(mobj_t* actor)
{
    int bobIndex;
    coord_t z;

    A_Explode(actor);

    bobIndex = actor->special2;
    z = FLOATBOBOFFSET(bobIndex);
    actor->origin[VZ] += z / 16;
    actor->special2 = (bobIndex + 1) & 63;
}

void C_DECL A_PoisonShroom(mobj_t* actor)
{
    actor->tics = 128 + (P_Random() << 1);
}

void C_DECL A_CheckThrowBomb(mobj_t* actor)
{
    if(fabs(actor->mom[MX]) < 1.5f && fabs(actor->mom[MY]) < 1.5f &&
       actor->mom[MZ] < 2 && actor->state == &STATES[S_THROWINGBOMB6])
    {
        P_MobjChangeState(actor, S_THROWINGBOMB7);
        actor->origin[VZ] = actor->floorZ;
        actor->mom[MZ] = 0;
        actor->flags2 &= ~MF2_FLOORBOUNCE;
        actor->flags &= ~MF_MISSILE;
        actor->flags |= MF_VIEWALIGN;
    }

    if(!--actor->health)
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH));
    }
}

/**
 * Quake variables
 *
 *      args[0]     Intensity on richter scale (2..9)
 *      args[1]     Duration in tics
 *      args[2]     Radius for damage
 *      args[3]     Radius for tremor
 *      args[4]     TID of map thing for focus of quake
 */

dd_bool A_LocalQuake(byte* args, mobj_t* actor)
{
    mobj_t* focus, *target;
    int lastfound = 0;
    int success = false;

    DE_UNUSED(actor);

    // Find all quake foci.
    do
    {
        if((target = P_FindMobjFromTID(args[4], &lastfound)))
        {
            if((focus = P_SpawnMobj(MT_QUAKE_FOCUS, target->origin, 0, 0)))
            {
                focus->args[0] = args[0];
                focus->args[1] = args[1] / 2; // Decremented every 2 tics.
                focus->args[2] = args[2];
                focus->args[3] = args[3];
                focus->args[4] = args[4];
                success = true;
            }
        }
    } while(target != NULL);

    return success;
}

void C_DECL A_Quake(mobj_t* actor)
{
    angle_t angle;
    player_t* player;
    mobj_t* victim;
    int richters = actor->args[0];
    int playnum;
    coord_t dist;

    if(actor->args[1]-- > 0)
    {
        for(playnum = 0; playnum < MAXPLAYERS; ++playnum)
        {
            player = &players[playnum];
            if(!players[playnum].plr->inGame)
                continue;

            victim = player->plr->mo;
            dist =  M_ApproxDistance(actor->origin[VX] - victim->origin[VX],
                                     actor->origin[VY] - victim->origin[VY]);

            dist = FIX2FLT(FLT2FIX(dist) >> (FRACBITS + 6));

            // Tested in tile units (64 pixels).
            if(dist < FIX2FLT(actor->args[3])) // In tremor radius.
            {
                localQuakeHappening[playnum] = richters;
                players[playnum].update |= PSF_LOCAL_QUAKE;
            }

            // Check if in damage radius.
            if(dist < FIX2FLT(actor->args[2]) &&
               victim->origin[VZ] <= victim->floorZ)
            {
                if(P_Random() < 50)
                {
                    P_DamageMobj(victim, NULL, NULL, HITDICE(1), false);
                }

                // Thrust player around.
                angle = victim->angle + ANGLE_1 * P_Random();
                P_ThrustMobj(victim, angle, FIX2FLT(richters << (FRACBITS - 1)));
            }
        }
    }
    else
    {
        for(playnum = 0; playnum < MAXPLAYERS; playnum++)
        {
            localQuakeHappening[playnum] = false;
            players[playnum].update |= PSF_LOCAL_QUAKE;
        }
        P_MobjChangeState(actor, S_NULL);
    }
}

static void telospawn(mobjtype_t type, mobj_t* mo)
{
    mobj_t*                 pmo;

    if((pmo = P_SpawnMobj(MT_TELOTHER_FX2, mo->origin, mo->angle, 0)))
    {
        pmo->special1 = TELEPORT_LIFE; // Lifetime countdown.
        pmo->target = mo->target;
        pmo->mom[MX] = mo->mom[MX] / 2;
        pmo->mom[MY] = mo->mom[MY] / 2;
        pmo->mom[MZ] = mo->mom[MZ] / 2;
    }
}

void C_DECL A_TeloSpawnA(mobj_t* mo)
{
    telospawn(MT_TELOTHER_FX2, mo);
}

void C_DECL A_TeloSpawnB(mobj_t* mo)
{
    telospawn(MT_TELOTHER_FX3, mo);
}

void C_DECL A_TeloSpawnC(mobj_t* mo)
{
    telospawn(MT_TELOTHER_FX4, mo);
}

void C_DECL A_TeloSpawnD(mobj_t* mo)
{
    telospawn(MT_TELOTHER_FX5, mo);
}

void C_DECL A_CheckTeleRing(mobj_t* actor)
{
    if(actor->special1-- <= 0)
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH));
    }
}

void P_SpawnDirt(mobj_t* mo, coord_t radius)
{
    coord_t pos[3];
    int dtype = 0;
    mobj_t* pmo;
    uint an;

    an = P_Random() << 5;

    pos[VX] = mo->origin[VX];
    pos[VY] = mo->origin[VY];
    pos[VZ] = mo->origin[VZ];

    pos[VX] += radius * FIX2FLT(finecosine[an]);
    pos[VY] += radius * FIX2FLT(finesine[an]);
    pos[VZ] += FLT2FIX(P_Random() << 9) + 1;

    switch(P_Random() % 6)
    {
    case 0: dtype = MT_DIRT1; break;
    case 1: dtype = MT_DIRT2; break;
    case 2: dtype = MT_DIRT3; break;
    case 3: dtype = MT_DIRT4; break;
    case 4: dtype = MT_DIRT5; break;
    case 5: dtype = MT_DIRT6; break;
    }

    if((pmo = P_SpawnMobj(dtype, pos, 0, 0)))
    {
        pmo->mom[MZ] = FIX2FLT(P_Random() << 10);
    }
}

/**
 * Thrust Spike Variables
 *      tracer          pointer to dirt clump mobj
 *      special2        speed of raise
 *      args[0]     0 = lowered,  1 = raised
 *      args[1]     0 = normal,   1 = bloody
 */

void C_DECL A_ThrustInitUp(mobj_t* actor)
{
    actor->special2 = 5;
    actor->args[0] = 1;
    actor->floorClip = 0;
    actor->flags = MF_SOLID;
    actor->flags2 = MF2_NOTELEPORT | MF2_FLOORCLIP;
    actor->tracer = NULL;
}

void C_DECL A_ThrustInitDn(mobj_t* actor)
{
    mobj_t*             mo;

    actor->special2 = 5;
    actor->args[0] = 0;
    actor->floorClip = actor->info->height;
    actor->flags = 0;
    actor->flags2 = MF2_NOTELEPORT | MF2_FLOORCLIP | MF2_DONTDRAW;
    if((mo = P_SpawnMobj(MT_DIRTCLUMP, actor->origin, 0, 0)))
        actor->tracer = mo;
}

void C_DECL A_ThrustRaise(mobj_t* actor)
{
    if(A_RaiseMobj(actor))
    {   // Reached it's target height.
        actor->args[0] = 1;
        if(actor->args[1])
            P_MobjChangeStateNoAction(actor, S_BTHRUSTINIT2_1);
        else
            P_MobjChangeStateNoAction(actor, S_THRUSTINIT2_1);
    }

    // Lose the dirt clump.
    if(actor->floorClip < actor->height && actor->tracer)
    {
        P_MobjRemove(actor->tracer, false);
        actor->tracer = NULL;
    }

    // Spawn some dirt.
    if(P_Random() < 40)
        P_SpawnDirt(actor, actor->radius);
    actor->special2++; // Increase raise speed.
}

void C_DECL A_ThrustLower(mobj_t *actor)
{
    if(A_SinkMobj(actor))
    {
        actor->args[0] = 0;
        if(actor->args[1])
            P_MobjChangeStateNoAction(actor, S_BTHRUSTINIT1_1);
        else
            P_MobjChangeStateNoAction(actor, S_THRUSTINIT1_1);
    }
}

void C_DECL A_ThrustBlock(mobj_t *actor)
{
    actor->flags |= MF_SOLID;
}

void C_DECL A_ThrustImpale(mobj_t *actor)
{
    P_ThrustSpike(actor);
}

#if MSVC
#  pragma optimize("g",off)
#endif
void C_DECL A_SoAExplode(mobj_t* actor)
{
    int i;
    mobj_t* chunk = NULL;

    for(i = 0; i < 10; ++i)
    {
        coord_t pos[3];

        pos[VX] = actor->origin[VX];
        pos[VY] = actor->origin[VY];
        pos[VZ] = actor->origin[VZ];

        pos[VX] += FIX2FLT((P_Random() - 128) << 12);
        pos[VY] += FIX2FLT((P_Random() - 128) << 12);
        pos[VZ] += FIX2FLT(P_Random() * FLT2FIX(actor->height) / 256);

        if((chunk = P_SpawnMobj(MT_ZARMORCHUNK, pos, P_Random() << 24, 0)))
        {
            P_MobjChangeState(chunk, P_GetState(chunk->type, SN_SPAWN) + i);

            chunk->mom[MZ] = ((P_Random() & 7) + 5);
            chunk->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
            chunk->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        }
    }

    if(actor->args[0])
    {
        // Spawn an item.
        if(!gfw_Rule(noMonsters) ||
           !(MOBJINFO[TranslateThingType[actor->args[0]]].
             flags & MF_COUNTKILL))
        {
            // Only spawn monsters if not -nomonsters.
            P_SpawnMobj(TranslateThingType[actor->args[0]], actor->origin,
                           actor->angle, 0);
        }
    }

    S_StartSound(SFX_SUITOFARMOR_BREAK, chunk);
    P_MobjRemove(actor, false);
}
#if MSVC
#  pragma optimize("",on)
#endif

void C_DECL A_BellReset1(mobj_t *actor)
{
    actor->flags |= MF_NOGRAVITY;
    actor->height *= 2*2;
}

void C_DECL A_BellReset2(mobj_t *actor)
{
    actor->flags |= MF_SHOOTABLE;
    actor->flags &= ~MF_CORPSE;
    actor->health = 5;
}

void C_DECL A_FlameCheck(mobj_t *actor)
{
    if(!actor->args[0]--) // Called every 8 tics.
    {
        P_MobjChangeState(actor, S_NULL);
    }
}

/**
 * Bat Spawner Variables
 *  special1    frequency counter
 *  special2
 *  args[0]     frequency of spawn (1=fastest, 10=slowest)
 *  args[1]     spread angle (0..255)
 *  args[2]
 *  args[3]     duration of bats (in octics)
 *  args[4]     turn amount per move (in degrees)
 *
 * Bat Variables
 *  special2    lifetime counter
 *  args[4]     turn amount per move (in degrees)
 */

void C_DECL A_BatSpawnInit(mobj_t *actor)
{
    actor->special1 = 0; // Frequency count.
}

void C_DECL A_BatSpawn(mobj_t *actor)
{
    mobj_t     *mo;
    int         delta;
    angle_t     angle;

    // Countdown until next spawn
    if(actor->special1-- > 0)
        return;

    actor->special1 = actor->args[0]; // Reset frequency count.

    delta = actor->args[1];
    if(delta == 0)
        delta = 1;
    angle = actor->angle + (((P_Random() % delta) - (delta >> 1)) << 24);

    mo = P_SpawnMissileAngle(MT_BAT, actor, angle, 0);
    if(mo)
    {
        mo->args[0] = P_Random() & 63; // floatbob index
        mo->args[4] = actor->args[4]; // turn degrees
        mo->special2 = actor->args[3] << 3; // Set lifetime
        mo->target = actor;
    }
}

void C_DECL A_BatMove(mobj_t* actor)
{
    angle_t angle;
    uint an;
    coord_t speed;

    if(actor->special2 < 0)
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH));
    }
    actor->special2 -= 2;       // Called every 2 tics

    if(P_Random() < 128)
    {
        angle = actor->angle + ANGLE_1 * actor->args[4];
    }
    else
    {
        angle = actor->angle - ANGLE_1 * actor->args[4];
    }

    // Adjust momentum vector to new direction
    an = angle >> ANGLETOFINESHIFT;
    speed = actor->info->speed * FIX2FLT(P_Random() << 10);
    actor->mom[MX] = speed * FIX2FLT(finecosine[an]);
    actor->mom[MY] = speed * FIX2FLT(finesine[an]);

    if(P_Random() < 15)
        S_StartSound(SFX_BAT_SCREAM, actor);

    // Handle Z movement
    actor->origin[VZ] = actor->target->origin[VZ] + 2 * FLOATBOBOFFSET(actor->args[0]);
    actor->args[0] = (actor->args[0] + 3) & 63;
}

void C_DECL A_TreeDeath(mobj_t* actor)
{
    if(!(actor->flags2 & MF2_FIREDAMAGE))
    {
        actor->height *= 2*2;
        actor->flags |= MF_SHOOTABLE;
        actor->flags &= ~(MF_CORPSE + MF_DROPOFF);
        actor->health = 35;
        return;
    }
    else
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_MELEE));
    }
}

void C_DECL A_NoGravity(mobj_t* actor)
{
    actor->flags |= MF_NOGRAVITY;
}
