/** @file p_mobj.c World map object interaction.
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

#include <math.h>
#include <string.h>

#include "jheretic.h"

#include "d_netcl.h"
#include "dmu_lib.h"
#include "hu_stuff.h"
#include "g_common.h"
#include "p_map.h"
#include "p_terraintype.h"
#include "player.h"
#include "p_tick.h"

#include <assert.h>

#define VANISHTICS              (2*TICSPERSEC)

#define MAX_BOB_OFFSET          (8)

#define NOMOMENTUM_THRESHOLD    (0.000001)

mobj_t *missileMobj;

void P_ExplodeMissile(mobj_t *mo)
{
    if(!mo->info) return;

    if(mo->type == MT_WHIRLWIND)
    {
        if(++mo->special2 < 60)
        {
            return;
        }
    }

    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }

    if(mo->info->deathSound)
    {
        S_StartSound(mo->info->deathSound, mo);
    }
}

void P_FloorBounceMissile(mobj_t* mo)
{
    mo->mom[MZ] = -mo->mom[MZ];
    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));
}

void P_ThrustMobj(mobj_t* mo, angle_t angle, coord_t move)
{
    uint an = angle >> ANGLETOFINESHIFT;

    mo->mom[MX] += move * FIX2FLT(finecosine[an]);
    mo->mom[MY] += move * FIX2FLT(finesine[an]);
}

/**
 * @param delta  The amount 'source' needs to turn.
 *
 * @return  @c 1, = 'source' needs to turn clockwise, or
 *          @c 0, = 'source' needs to turn counter clockwise.
 */
int P_FaceMobj(mobj_t* source, mobj_t* target, angle_t* delta)
{
    angle_t diff, angle1, angle2;

    angle1 = source->angle;
    angle2 = M_PointToAngle2(source->origin, target->origin);
    if(angle2 > angle1)
    {
        diff = angle2 - angle1;
        if(diff > ANGLE_180)
        {
            *delta = ANGLE_MAX - diff;
            return 0;
        }
        else
        {
            *delta = diff;
            return 1;
        }
    }
    else
    {
        diff = angle1 - angle2;
        if(diff > ANGLE_180)
        {
            *delta = ANGLE_MAX - diff;
            return 1;
        }
        else
        {
            *delta = diff;
            return 0;
        }
    }
}

/**
 * The missile tracer field must be the target.
 *
 * @return              @c true, if target was tracked else @c false.
 */
dd_bool P_SeekerMissile(mobj_t* actor, angle_t thresh, angle_t turnMax)
{
    int dir;
    uint an;
    coord_t dist;
    angle_t delta;
    mobj_t* target;

    target = actor->tracer;
    if(!target) return false;

    if(!(target->flags & MF_SHOOTABLE))
    {   // Target died.
        actor->tracer = NULL;
        return false;
    }

    dir = P_FaceMobj(actor, target, &delta);
    if(delta > thresh)
    {
        delta >>= 1;
        if(delta > turnMax)
        {
            delta = turnMax;
        }
    }

    if(dir)
    {   // Turn clockwise.
        actor->angle += delta;
    }
    else
    {   // Turn counter clockwise.
        actor->angle -= delta;
    }

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = actor->info->speed * FIX2FLT(finecosine[an]);
    actor->mom[MY] = actor->info->speed * FIX2FLT(finesine[an]);

    if(actor->origin[VZ]  + actor->height  < target->origin[VZ] ||
       target->origin[VZ] + target->height < actor->origin[VZ])
    {   // Need to seek vertically.
        dist = M_ApproxDistance(target->origin[VX] - actor->origin[VX],
                                target->origin[VY] - actor->origin[VY]);
        dist /= actor->info->speed;
        if(dist < 1)
            dist = 1;

        actor->mom[MZ] = (target->origin[VZ] - actor->origin[VZ]) / dist;
    }

    return true;
}

/**
 * Wind pushes the mobj, if its sector special is a wind type.
 */
void P_WindThrust(mobj_t *mo)
{
    static int windTab[3] = { 2048 * 5, 2048 * 10, 2048 * 25 };

    Sector *sec = Mobj_Sector(mo);
    int special = P_ToXSector(sec)->special;

    switch(special)
    {
    case 40: // Wind_East
    case 41:
    case 42:
        P_ThrustMobj(mo, 0, FIX2FLT(windTab[special - 40]));
        break;

    case 43: // Wind_North
    case 44:
    case 45:
        P_ThrustMobj(mo, ANG90, FIX2FLT(windTab[special - 43]));
        break;

    case 46: // Wind_South
    case 47:
    case 48:
        P_ThrustMobj(mo, ANG270, FIX2FLT(windTab[special - 46]));
        break;

    case 49: // Wind_West
    case 50:
    case 51:
        P_ThrustMobj(mo, ANG180, FIX2FLT(windTab[special - 49]));
        break;

    default:
        break;
    }
}

void P_MobjMoveXY(mobj_t *mo)
{
    coord_t pos[2], mom[2];
    //player_t *player;
    dd_bool largeNegative;

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    mom[MX] = MINMAX_OF(-MAXMOM, mo->mom[MX], MAXMOM);
    mom[MY] = MINMAX_OF(-MAXMOM, mo->mom[MY], MAXMOM);
    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];

    if(IS_ZERO(mom[MX]) && IS_ZERO(mom[MY]))
    {
        if(mo->flags & MF_SKULLFLY)
        {
            // A flying mobj slammed into something.
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
            P_MobjChangeState(mo, P_GetState(mo->type, SN_SEE));
        }
        return;
    }

    if(mo->flags2 & MF2_WINDTHRUST)
        P_WindThrust(mo);

    //player = mo->player;
    do
    {
        /**
         * DOOM.exe bug fix:
         * Large negative displacements were never considered. This explains
         * the tendency for Mancubus fireballs to pass through walls.
         */

        largeNegative = false;
        if(!cfg.moveBlock && (mom[MX] < -MAXMOMSTEP || mom[MY] < -MAXMOMSTEP))
        {
            // Make an exception for "north-only wallrunning".
            if(!(cfg.wallRunNorthOnly && mo->wallRun))
                largeNegative = true;
        }

        if(largeNegative || mom[MX] > MAXMOMSTEP || mom[MY] > MAXMOMSTEP)
        {
            pos[VX] = mo->origin[VX] + mom[MX] / 2;
            pos[VY] = mo->origin[VY] + mom[MY] / 2;
            mom[MX] /= 2;
            mom[MY] /= 2;
        }
        else
        {
            pos[VX] = mo->origin[VX] + mom[MX];
            pos[VY] = mo->origin[VY] + mom[MY];
            mom[MX] = mom[MY] = 0;
        }

        // If mobj was wallrunning - stop.
        if(mo->wallRun)
            mo->wallRun = false;

        // $dropoff_fix
        if(!P_TryMoveXY(mo, pos[VX], pos[VY], true, false))
        {   // Blocked mom.
            if(mo->flags2 & MF2_SLIDE)
            {   // Try to slide along it.
                P_SlideMove(mo);
            }
            else if(mo->flags & MF_MISSILE)
            {
                if (mo->flags3 & MF3_WALLBOUNCE)
                {
                    if (P_BounceWall(mo))
                    {
                        return;
                    }
                }

                // Explode a missile
                Sector* backSec;

                // @kludge: Prevent missiles exploding against the sky.
                if(tmCeilingLine && (backSec = P_GetPtrp(tmCeilingLine, DMU_BACK_SECTOR)))
                {
                    if((P_GetIntp(P_GetPtrp(backSec, DMU_CEILING_MATERIAL), DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->origin[VZ] > P_GetDoublep(backSec, DMU_CEILING_HEIGHT))
                    {
                        if(mo->type == MT_BLOODYSKULL)
                        {
                            mo->mom[MX] = mo->mom[MY] = 0;
                            mo->mom[MZ] = -1;
                        }
                        else
                        {
                            P_MobjRemove(mo, false);
                        }
                        return;
                    }
                }
                if(tmFloorLine && (backSec = P_GetPtrp(tmFloorLine, DMU_BACK_SECTOR)))
                {
                    if((P_GetIntp(P_GetPtrp(backSec, DMU_FLOOR_MATERIAL), DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->origin[VZ] < P_GetDoublep(backSec, DMU_FLOOR_HEIGHT))
                    {
                        if(mo->type == MT_BLOODYSKULL)
                        {
                            mo->mom[MX] = mo->mom[MY] = 0;
                            mo->mom[MZ] = -1;
                        }
                        else
                        {
                            P_MobjRemove(mo, false);
                        }
                        return;
                    }
                }
                // kludge end.

                P_ExplodeMissile(mo);
            }
            else
            {
                mo->mom[MX] = mo->mom[MY] = 0;
            }
        }
    } while(!INRANGE_OF(mom[MX], 0, NOMOM_THRESHOLD) ||
            !INRANGE_OF(mom[MY], 0, NOMOM_THRESHOLD));

    // Slow down.
    Mobj_XYMoveStopping(mo);
}

void P_MobjMoveZ(mobj_t *mo)
{
    coord_t gravity;
    coord_t dist;
    coord_t delta;

    // $democam: cameramen get special z movement
    if(P_CameraZMovement(mo))
        return;

    gravity = XS_Gravity(Mobj_Sector(mo));

    // $voodoodolls: Check for smooth step up unless a voodoo doll.
    if(mo->player && mo->player->plr->mo == mo && mo->origin[VZ] < mo->floorZ)
    {
        mo->player->viewHeight -= mo->floorZ - mo->origin[VZ];
        mo->player->viewHeightDelta =
            (cfg.common.plrViewHeight - mo->player->viewHeight) / 8;
    }

    // Adjust height.
    mo->origin[VZ] += mo->mom[MZ];

    if ((mo->flags2 & MF2_FLY) && mo->onMobj &&
        mo->origin[VZ] > mo->onMobj->origin[VZ] + mo->onMobj->height)
    {
        mo->onMobj = NULL; // We were on a mobj, we are NOT now.
    }

    if((mo->flags & MF_FLOAT) && mo->target && !P_MobjIsCamera(mo->target))
    {
        // Float down towards target if too close.
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            coord_t oldZ = mo->origin[VZ];

            dist = M_ApproxDistance(mo->origin[VX] - mo->target->origin[VX],
                                    mo->origin[VY] - mo->target->origin[VY]);

            delta = (mo->target->origin[VZ] + mo->target->height /2) -
                    (mo->origin[VZ] + mo->height /2);

            if(dist < mo->radius + mo->target->radius &&
               fabs(delta) < mo->height + mo->target->height)
            {
                // Don't go INTO the target.
                delta = 0;
            }

            if(delta < 0 && dist < -(delta * 3))
            {
                mo->origin[VZ] -= FLOATSPEED;
                P_MobjSetSRVOZ(mo, -FLOATSPEED);
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                mo->origin[VZ] += FLOATSPEED;
                P_MobjSetSRVOZ(mo, FLOATSPEED);
            }
            if(delta)
            {
                // Where did we end up?
                if(!P_CheckPosition(mo, mo->origin))
                {
                    // Not a valid position; undo the move.
                    mo->origin[VZ] = oldZ;
                    P_MobjSetSRVOZ(mo, 0);
                }
            }
        }
    }

    if(cfg.allowMonsterFloatOverBlocking && (mo->flags & MF_FLOAT) && !mo->player && !(mo->flags & MF_SKULLFLY))
    {
        if(!P_CheckPosition(mo, mo->origin))
        {
            App_Log(DE2_DEV_MAP_WARNING, "Floating thing %i has gotten stuck!");
            App_Log(DE2_DEV_MAP_MSG, "  onmobj=%i z=%f flz=%f tmfz=%f", mo->thinker.id,
                    mo->onMobj? mo->onMobj->thinker.id : 0, mo->origin[VZ],
                    mo->floorZ, tmFloorZ);

            if(mo->origin[VZ] < tmFloorZ)
            {
                mo->origin[VZ] = mo->floorZ = tmFloorZ;
            }
        }
    }

    // Do some fly-bobbing.
    if (mo->player && mo->player->plr->mo == mo && (mo->flags2 & MF2_FLY) &&
        mo->origin[VZ] > mo->floorZ && !mo->onMobj && (mapTime & 2))
    {
        mo->origin[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
    }

    // Clip movement. Another thing?
    if (mo->onMobj && mo->origin[VZ] <= mo->onMobj->origin[VZ] + mo->onMobj->height)
    {
        if (mo->mom[MZ] < 0)
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down. Decrease viewheight for a moment after
                // hitting the ground (hard), and utter appropriate sound.
                mo->player->viewHeightDelta = mo->mom[MZ] / 8;

                if(mo->player->health > 0)
                    S_StartSound(SFX_PLROOF, mo);
            }
            mo->mom[MZ] = 0;
        }

        if (IS_ZERO(mo->mom[MZ]))
        {
            mo->origin[VZ] = mo->onMobj->origin[VZ] + mo->onMobj->height;
        }

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }

    // The floor.
    if(mo->origin[VZ] <= mo->floorZ)
    {
        // Hit the floor.
        dd_bool             movingDown;

        // Note (id):
        //  somebody left this after the setting mom[MZ] to 0,
        //  kinda useless there.
        //
        // cph - This was the a bug in the linuxdoom-1.10 source which
        //  caused it not to sync Doom 2 v1.9 demos. Someone
        //  added the above comment and moved up the following code. So
        //  demos would desync in close lost soul fights.
        // Note that this only applies to original Doom 1 or Doom2 demos - not
        //  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
        //  gamemission. (Note we assume that Doom1 is always Ult Doom, which
        //  seems to hold for most published demos.)
        //
        //  fraggle - cph got the logic here slightly wrong.  There are three
        //  versions of Doom 1.9:
        //
        //  * The version used in registered doom 1.9 + doom2 - no bounce
        //  * The version used in ultimate doom - has bounce
        //  * The version used in final doom - has bounce
        //
        // So we need to check that this is either retail or commercial
        // (but not doom2)
        int correct_lost_soul_bounce = false;

        if(correct_lost_soul_bounce && (mo->flags & MF_SKULLFLY))
        {
            // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if((movingDown = (mo->mom[MZ] < 0)))
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down. Decrease viewheight for a moment after
                // hitting the ground hard and utter appropriate sound.
                mo->player->viewHeightDelta = mo->mom[MZ] / 8;
//#if __JHERETIC__
                mo->player->jumpTics = 12; // Can't jump in a while.
//#endif
                // Fix DOOM bug - dead players grunting when hitting the ground
                // (e.g., after an archvile attack)
                if(mo->player->health > 0)
                    S_StartSound(SFX_PLROOF, mo);
            }
        }

        mo->origin[VZ] = mo->floorZ;

        if(movingDown)
            P_HitFloor(mo);

        // cph 2001/05/26 -
        // See lost soul bouncing comment above. We need this here for bug
        // compatibility with original Doom2 v1.9 - if a soul is charging and
        // hit by a raising floor this incorrectly reverses its Y momentum.

        if(!correct_lost_soul_bounce && (mo->flags & MF_SKULLFLY))
            mo->mom[MZ] = -mo->mom[MZ];

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
            else if(mo->type == MT_MNTRFX2)
            {
                // Minotaur floor fire can go up steps
                return;
            }
            else
            {
                P_ExplodeMissile(mo);
                return;
            }
        }

        if(movingDown && mo->mom[MZ] < 0)
        {
            mo->mom[MZ] = 0;
        }

        // Set corpses to CRASH state.
        {
            statenum_t state;
            if((state = P_GetState(mo->type, SN_CRASH)) != S_NULL &&
               (mo->flags & MF_CORPSE))
            {
                P_MobjChangeState(mo, state);
                return;
            }
        }
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(IS_ZERO(mo->mom[MZ]))
            mo->mom[MZ] = -(gravity / 8) * 2;
        else
            mo->mom[MZ] -= gravity / 8;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(IS_ZERO(mo->mom[MZ]))
            mo->mom[MZ] = -gravity * 2;
        else
            mo->mom[MZ] -= gravity;
    }

    if(mo->origin[VZ] + mo->height > mo->ceilingZ)
    {
        // hit the ceiling
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->origin[VZ] = mo->ceilingZ - mo->height;

        if(mo->flags & MF_SKULLFLY)
        {                       // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(P_GetIntp(P_GetPtrp(Mobj_Sector(mo), DMU_CEILING_MATERIAL),
                         DMU_FLAGS) & MATF_SKYMASK)
            {
                if(mo->type == MT_BLOODYSKULL)
                {
                    mo->mom[MX] = mo->mom[MY] = 0;
                    mo->mom[MZ] = -1;
                }
                else
                {
                    // Don't explode against sky.
                    P_MobjRemove(mo, false);
                }
                return;
            }

            P_ExplodeMissile(mo);
            return;
        }
    }
}

void P_NightmareRespawn(mobj_t* mobj)
{
    mobj_t*             mo;

    // Something is occupying it's position?
    if(!P_CheckPositionXY(mobj, mobj->spawnSpot.origin[VX],
                          mobj->spawnSpot.origin[VY]))
        return; // No respwan.

    if((mo = P_SpawnMobj(mobj->type, mobj->spawnSpot.origin,
                            mobj->spawnSpot.angle, mobj->spawnSpot.flags)))
    {
        mo->reactionTime = 18;

        // Spawn a teleport fog at old spot because of removal of the body?
        if((mo = P_SpawnMobjXYZ(MT_TFOG, mobj->origin[VX], mobj->origin[VY],
                               TELEFOGHEIGHT, mobj->angle, MSF_Z_FLOOR)))
            S_StartSound(SFX_TELEPT, mo);

        // Spawn a teleport fog at the new spot.
        if((mo = P_SpawnMobjXYZ(MT_TFOG, mobj->spawnSpot.origin[VX],
                               mobj->spawnSpot.origin[VY], TELEFOGHEIGHT,
                               mobj->spawnSpot.angle, MSF_Z_FLOOR)))
            S_StartSound(SFX_TELEPT, mo);
    }

    // Remove the old monster.
    P_MobjRemove(mobj, true);
}

// Fake the zmovement so that we can check if a move is legal
// (from vanilla Heretic)
static void P_FakeZMovement(mobj_t *mo)
{
    coord_t dist  = 0;
    coord_t delta = 0;
    //
    // adjust height
    //
    mo->origin[VZ] += mo->mom[VZ];
    if (mo->flags & MF_FLOAT && mo->target)
    {
        // float down towards target if too close
        if (!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist  = M_ApproxDistance(mo->origin[VX] - mo->target->origin[VX],
                                     mo->origin[VY] - mo->target->origin[VY]);
            delta = (mo->target->origin[VZ] + (mo->height / 2)) - mo->origin[VZ];
            if (delta < 0 && dist < -(delta * 3))
                mo->origin[VZ] -= FLOATSPEED;
            else if (delta > 0 && dist < (delta * 3))
                mo->origin[VZ] += FLOATSPEED;
        }
    }
    if (mo->player && mo->flags2 & MF2_FLY && !(mo->origin[VZ] <= mo->floorZ) && (mapTime & 2))
    {
        mo->origin[VZ] += finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK];
    }

    //
    // clip movement
    //
    if (mo->origin[VZ] <= mo->floorZ)
    {
        // Hit the floor
        mo->origin[VZ] = mo->floorZ;
        if (mo->mom[VZ] < 0)
        {
            mo->mom[VZ] = 0;
        }
        if (mo->flags & MF_SKULLFLY)
        {
            // The skull slammed into something
            mo->mom[VZ] = -mo->mom[VZ];
        }
        if (MOBJINFO[mo->type].states[SN_CRASH] && (mo->flags & MF_CORPSE))
        {
            return;
        }
    }
    else if (mo->flags2 & MF2_LOGRAV)
    {
        coord_t GRAVITY = XS_Gravity(Mobj_Sector(mo));

        if (FEQUAL(mo->mom[VZ], 0))
            mo->mom[VZ] = -(GRAVITY / 8) * 2;
        else
            mo->mom[VZ] -= GRAVITY / 8;
    }
    else if (!(mo->flags & MF_NOGRAVITY))
    {
        coord_t GRAVITY = XS_Gravity(Mobj_Sector(mo));

        if (FEQUAL(mo->mom[VZ], 0))
            mo->mom[VZ] = -GRAVITY * 2;
        else
            mo->mom[VZ] -= GRAVITY;
    }

    if (mo->origin[VZ] + mo->height > mo->ceilingZ)
    {
        // hit the ceiling
        if (mo->mom[VZ] > 0) mo->mom[VZ] = 0;
        mo->origin[VZ] = mo->ceilingZ - mo->height;
        if (mo->flags & MF_SKULLFLY)
        {
            // the skull slammed into something
            mo->mom[VZ] = -mo->mom[VZ];
        }
    }
}

struct checkonmobjz_s
{
    mobj_t *checkThing;
    mobj_t *onMobj;
};

static int PIT_CheckOnmobjZ(mobj_t *thing, void *dataPtr)
{
    struct checkonmobjz_s *data    = dataPtr;
    const mobj_t *         tmthing = data->checkThing;
    coord_t                blockdist;

    if (thing == tmthing)
    {
        // Don't clip against self
        return false;
    }
    if (!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)))
    {
        // Can't hit thing
        return false;
    }
    blockdist = thing->radius + tmthing->radius;
    if (fabs(thing->origin[VX] - tmthing->origin[VX]) >= blockdist ||
        fabs(thing->origin[VY] - tmthing->origin[VY]) >= blockdist)
    {
        // Didn't hit thing
        return false;
    }
    if (tmthing->origin[VZ] > thing->origin[VZ] + thing->height)
    {
        return false;
    }
    else if (tmthing->origin[VZ] + tmthing->height < thing->origin[VZ])
    {
        // under thing
        return false;
    }
    if (thing->flags & MF_SOLID)
    {
        data->onMobj = thing;
    }
    return (thing->flags & MF_SOLID) != 0;
}

// Checks if the new Z position is legal
// (from vanilla Heretic)
static mobj_t *P_CheckOnmobj(mobj_t *thing)
{
#if 0
    int			xl,xh,yl,yh,bx,by;
    subsector_t		*newsubsec;
    fixed_t x;
    fixed_t y;
    mobj_t oldmo;

    x = thing->x;
    y = thing->y;
    tmthing = thing;
    tmflags = thing->flags;
    oldmo = *thing; // save the old mobj before the fake zmovement
    P_FakeZMovement(tmthing);
#endif

    coord_t oldOrigin[3];
    coord_t oldMom[3];
    AABoxd  bounds;

    struct checkonmobjz_s data = {thing, NULL};

    memcpy(oldOrigin, thing->origin, sizeof(oldOrigin));
    memcpy(oldMom, thing->mom, sizeof(oldMom));

    P_FakeZMovement(thing);

//    tmx = x;
//    tmy = y;

//    tmbbox[BOXTOP] = y + tmthing->radius;
//    tmbbox[BOXBOTTOM] = y - tmthing->radius;
//    tmbbox[BOXRIGHT] = x + tmthing->radius;
//    tmbbox[BOXLEFT] = x - tmthing->radius;

//    newsubsec = R_PointInSubsector (x,y);
//    ceilingline = NULL;

////
//// the base floor / ceiling is from the subsector that contains the
//// point.  Any contacted lines the step closer together will adjust them
////
//    tmfloorz = tmdropoffz = newsubsec->sector->floorheight;
//    tmceilingz = newsubsec->sector->ceilingheight;

//    validcount++;
//    numspechit = 0;

    if (!(thing->flags & MF_NOCLIP))
    {
        bounds.minX = thing->origin[VX] - thing->radius;
        bounds.minY = thing->origin[VY] - thing->radius;
        bounds.maxX = thing->origin[VX] + thing->radius;
        bounds.maxY = thing->origin[VY] + thing->radius;

        VALIDCOUNT++;
        Mobj_BoxIterator(&bounds, PIT_CheckOnmobjZ, &data);
    }

    // Restore state.
    memcpy(thing->origin, oldOrigin, sizeof(oldOrigin));
    memcpy(thing->mom, oldMom, sizeof(oldMom));

    return data.onMobj;
}

void P_MobjThinker(void *thinkerPtr)
{
    mobj_t *mobj = thinkerPtr;

    if (IS_CLIENT && !ClMobj_IsValid(mobj)) return; // We should not touch this right now.

    if (mobj->type == MT_BLASTERFX1)
    {
        int     i;
        coord_t frac[3];
        coord_t z;
        dd_bool changexy;

        // Handle movement
        if (NON_ZERO(mobj->mom[MX]) || NON_ZERO(mobj->mom[MY]) || NON_ZERO(mobj->mom[MZ]) ||
            !FEQUAL(mobj->origin[VZ], mobj->floorZ))
        {
            frac[MX] = mobj->mom[MX] / 8;
            frac[MY] = mobj->mom[MY] / 8;
            frac[MZ] = mobj->mom[MZ] / 8;

            changexy = (NON_ZERO(frac[MX]) || NON_ZERO(frac[MY]));
            for (i = 0; i < 8; ++i)
            {
                if (changexy)
                {
                    if (!P_TryMoveXY(mobj,
                                     mobj->origin[VX] + frac[MX],
                                     mobj->origin[VY] + frac[MY],
                                     false,
                                     false))
                    {
                        // Blocked move.
                        P_ExplodeMissile(mobj);
                        return;
                    }
                }

                mobj->origin[VZ] += frac[MZ];
                if (mobj->origin[VZ] <= mobj->floorZ)
                {
                    // Hit the floor.
                    mobj->origin[VZ] = mobj->floorZ;
                    P_HitFloor(mobj);
                    P_ExplodeMissile(mobj);
                    return;
                }

                if (mobj->origin[VZ] + mobj->height > mobj->ceilingZ)
                {
                    // Hit the ceiling.
                    mobj->origin[VZ] = mobj->ceilingZ - mobj->height;
                    P_ExplodeMissile(mobj);
                    return;
                }

                if (changexy && (P_Random() < 64))
                {
                    z = mobj->origin[VZ] - 8;
                    if (z < mobj->floorZ)
                    {
                        z = mobj->floorZ;
                    }

                    P_SpawnMobjXYZ(MT_BLASTERSMOKE,
                                   mobj->origin[VX],
                                   mobj->origin[VY],
                                   z,
                                   P_Random() << 24,
                                   0);
                }
            }
        }

        // Advance the state.
        if (mobj->tics != -1)
        {
            mobj->tics--;
            while (!mobj->tics)
            {
                if (!P_MobjChangeState(mobj, mobj->state->nextState))
                { // Mobj was removed.
                    return;
                }
            }
        }

        return;
    }

    // The first three bits of the selector special byte contain a relative
    // health level.
    P_UpdateHealthBits(mobj);

    // Handle X and Y momentums.
    if (NON_ZERO(mobj->mom[MX]) || NON_ZERO(mobj->mom[MY]) || (mobj->flags & MF_SKULLFLY))
    {
        P_MobjMoveXY(mobj);

        if (mobj->thinker.function == (thinkfunc_t) NOPFUNC)
        {
            return; // Mobj was removed.
        }
    }

    if (mobj->flags2 & MF2_FLOATBOB)
    {
        // Floating item bobbing motion.
        // Keep it on the floor.
        mobj->origin[VZ] = mobj->floorZ;

        // Negative floorclip raises the mobj off the floor.
        mobj->floorClip = -mobj->special1;
        if (mobj->floorClip < -MAX_BOB_OFFSET)
        {
            // We don't want it going through the floor.
            mobj->floorClip = -MAX_BOB_OFFSET;
        }
    }
    else if (!FEQUAL(mobj->origin[VZ], mobj->floorZ) || NON_ZERO(mobj->mom[MZ]))
    {
        coord_t oldZ = mobj->origin[VZ];

        if (mobj->type == MT_POD)
        {
            // Use vanilla behavior for gas pods. The newer routines do not produce the
            // correct behavior when pods interact with each other.
            if ((mobj->onMobj = P_CheckOnmobj(mobj)) == NULL)
            {
                P_MobjMoveZ(mobj);
            }
            else
            {
                // Stop pod's downward momentum when landing on something.
                if (/*mobj->player &&*/ mobj->mom[VZ] < 0)
                {
                    mobj->mom[VZ] = 0;
                }
                // This is exclusive to pods, so the code below is not relevant.
                /*
                if (mobj->player && (onmo->player || onmo->type == MT_POD))
                {
                    mobj->mom[VX] = onmo->mom[VX];
                    mobj->mom[VY] = onmo->mom[VY];
                    if (onmo->origin[VZ] < onmo->floorZ)
                    {
                        mobj->origin[VZ] += onmo->floorZ - onmo->origin[VZ];
                        if (onmo->player)
                        {
                            onmo->player->viewHeight -= onmo->floorZ - onmo->origin[VZ];
                            onmo->player->viewHeightDelta =
                                (VIEWHEIGHT - onmo->player->viewHeight) / 8;
                        }
                        onmo->origin[VZ] = onmo->floorZ;
                    }
                }
                */
            }
        }
        else
        {
            P_MobjMoveZ(mobj);
        }

        if (mobj->thinker.function != (thinkfunc_t) P_MobjThinker) return; // mobj was removed

        /**
         * @todo Instead of this post-move check, we should fix the root cause why
         * the SKULLFLYer is ending up in an invalid position during P_MobjMoveZ().
         * If only the movement validity checks weren't so convoluted... -jk
         */
        if ((mobj->flags & MF_SKULLFLY) && !P_CheckPosition(mobj, mobj->origin))
        {
            // Let's not get stuck.
            if (mobj->origin[VZ] > oldZ && mobj->mom[VZ] > 0) mobj->mom[VZ] = 0;
            if (mobj->origin[VZ] < oldZ && mobj->mom[VZ] < 0) mobj->mom[VZ] = 0;
            mobj->origin[VZ] = oldZ;
        }
    }
    // Non-sentient objects at rest.
    else if (!(NON_ZERO(mobj->mom[MX]) || NON_ZERO(mobj->mom[MY])) && !sentient(mobj) &&
             !mobj->player && !((mobj->flags & MF_CORPSE) && cfg.slidingCorpses))
    {
        /**
         * Objects fall off ledges if they are hanging off slightly push off
         * of ledge if hanging more than halfway off.
         */

        if (mobj->origin[VZ] > mobj->dropOffZ && // Only objects contacting dropoff
            !(mobj->flags & MF_NOGRAVITY) && cfg.fallOff)
        {
            P_ApplyTorque(mobj);
        }
        else
        {
            mobj->intFlags &= ~MIF_FALLING;
            mobj->gear = 0; // Reset torque.
        }
    }

    if (cfg.slidingCorpses)
    {
        if (((mobj->flags & MF_CORPSE)
                 ? mobj->origin[VZ] > mobj->dropOffZ
                 : mobj->origin[VZ] - mobj->dropOffZ > 24) && // Only objects contacting drop off.
            !(mobj->flags & MF_NOGRAVITY))                    // Only objects which fall.
        {
            P_ApplyTorque(mobj); // Apply torque.
        }
        else
        {
            mobj->intFlags &= ~MIF_FALLING;
            mobj->gear = 0; // Reset torque.
        }
    }

    // $vanish: dead monsters disappear after some time.
    if (cfg.corpseTime && (mobj->flags & MF_CORPSE) && mobj->corpseTics != -1)
    {
        if (++mobj->corpseTics < cfg.corpseTime * TICSPERSEC)
        {
            mobj->translucency = 0; // Opaque.
        }
        else if (mobj->corpseTics < cfg.corpseTime * TICSPERSEC + VANISHTICS)
        {
            // Translucent during vanishing.
            mobj->translucency =
                ((mobj->corpseTics - cfg.corpseTime * TICSPERSEC) * 255) / VANISHTICS;
        }
        else
        {
            // Too long; get rid of the corpse.
            mobj->corpseTics = -1;
            return;
        }
    }
    
    P_MobjAngleSRVOTicker(mobj); // "angle-servo"; smooth actor turning.
    
    // Cycle through states, calling action functions at transitions.
    if (mobj->tics != -1)
    {
        mobj->tics--;

        // You can cycle through multiple states in a tic.
        if (!mobj->tics)
        {
            P_MobjClearSRVO(mobj);
            if (!P_MobjChangeState(mobj, mobj->state->nextState)) return; // Freed itself.
        }
    }
    else if (!IS_CLIENT)
    {
        // Check for nightmare respawn.
        if (!(mobj->flags & MF_COUNTKILL)) return;

        if (!gfw_Rule(respawnMonsters)) return;

        mobj->moveCount++;

        if (mobj->moveCount < 12 * 35) return;

        if (mapTime & 31) return;

        if (P_Random() > 4) return;

        P_NightmareRespawn(mobj);
    }
}

/**
 * Spawns a mobj of "type" at the specified position.
 */
mobj_t* P_SpawnMobjXYZ(mobjtype_t type, coord_t x, coord_t y, coord_t z,
    angle_t angle, int spawnFlags)
{
    mobj_t* mo;
    mobjinfo_t* info;
    coord_t space;
    int ddflags = 0;

    if(type < MT_FIRST || type >= Get(DD_NUMMOBJTYPES))
    {
#ifdef _DEBUG
        Con_Error("P_SpawnMobj: Illegal mo type %i.\n", type);
#endif
        return NULL;
    }

    info = &MOBJINFO[type];

    /*
    // Clients only spawn local objects.
    if(!(info->flags & MF_LOCAL) && IS_CLIENT)
        return NULL;
     */

    // Not for deathmatch?
    if(gfw_Rule(deathmatch) && (info->flags & MF_NOTDMATCH))
        return NULL;

    // Check for specific disabled objects.
    switch(type)
    {
    case MT_WSKULLROD:
    case MT_WPHOENIXROD:
    case MT_AMSKRDWIMPY:
    case MT_AMSKRDHEFTY:
    case MT_AMPHRDWIMPY:
    case MT_AMPHRDHEFTY:
    case MT_AMMACEWIMPY:
    case MT_AMMACEHEFTY:
    case MT_ARTISUPERHEAL:
    case MT_ARTITELEPORT:
    case MT_ITEMSHIELD2:
        if(gameMode == heretic_shareware)
        {
            return 0;// Don't place on map.
        }
        break;

    default:
        break;
    }

    // Don't spawn any monsters?
    if(gfw_Rule(noMonsters) && (info->flags & MF_COUNTKILL))
        return 0;

    if(info->flags & MF_SOLID)
        ddflags |= DDMF_SOLID;
    if(info->flags2 & MF2_DONTDRAW)
        ddflags |= DDMF_DONTDRAW;

    mo = Mobj_CreateXYZ(P_MobjThinker, x, y, z, angle, info->radius,
                      info->height, ddflags);
    mo->type = type;
    mo->info = info;
    mo->flags = info->flags;
    mo->flags2 = info->flags2;
    mo->flags3 = info->flags3;
    mo->damage = info->damage;
    mo->health = info->spawnHealth * (IS_NETGAME ? cfg.common.netMobHealthModifier : 1);
    mo->moveDir = DI_NODIR;
    mo->selector = 0;
    P_UpdateHealthBits(mo); // Set the health bits of the selector.

    if(gfw_Rule(skill) != SM_NIGHTMARE)
        mo->reactionTime = info->reactionTime;

    mo->lastLook = P_Random() % MAXPLAYERS;

    // Must link before setting state (ID assigned for the mo).
    Mobj_SetState(mo, P_GetState(mo->type, SN_SPAWN));

    if(mo->type == MT_MACEFX1 || mo->type == MT_MACEFX2 ||
       mo->type == MT_MACEFX3)
        mo->special3 = 1000;

    // Link the mobj into the world.
    P_MobjLink(mo);

    mo->floorZ   = P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT);
    mo->dropOffZ = mo->floorZ;
    mo->ceilingZ = P_GetDoublep(Mobj_Sector(mo), DMU_CEILING_HEIGHT);

    if((spawnFlags & MSF_Z_CEIL) || (info->flags & MF_SPAWNCEILING))
    {
        mo->origin[VZ] = mo->ceilingZ - mo->info->height - z;
    }
    else if((spawnFlags & MSF_Z_RANDOM) || (info->flags2 & MF2_SPAWNFLOAT))
    {
        space = mo->ceilingZ - mo->info->height - mo->floorZ;
        if(space > 48)
        {
            space -= 40;
            mo->origin[VZ] = ((space * P_Random()) / 256) + mo->floorZ + 40;
        }
        else
        {
            mo->origin[VZ] = mo->floorZ;
        }
    }
    else if(spawnFlags & MSF_Z_FLOOR)
    {
        mo->origin[VZ] = mo->floorZ + z;
    }

    if(spawnFlags & MSF_AMBUSH)
        mo->flags |= MF_AMBUSH;

    mo->floorClip = 0;

    if((mo->flags2 & MF2_FLOORCLIP) &&
       FEQUAL(mo->origin[VZ], P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT)))
    {
        const terraintype_t* tt = P_MobjFloorTerrain(mo);

        if(tt->flags & TTF_FLOORCLIP)
        {
            mo->floorClip = 10;
        }
    }

    // Copy spawn attributes to the new mobj.
    mo->spawnSpot.origin[VX] = x;
    mo->spawnSpot.origin[VY] = y;
    mo->spawnSpot.origin[VZ] = z;
    mo->spawnSpot.angle = angle;
    mo->spawnSpot.flags = spawnFlags;

    return mo;
}

mobj_t* P_SpawnMobj(mobjtype_t type, coord_t const pos[3], angle_t angle, int spawnFlags)
{
    return P_SpawnMobjXYZ(type, pos[VX], pos[VY], pos[VZ], angle, spawnFlags);
}

void P_RepositionMace(mobj_t *mo)
{
    mapspot_t const *mapSpot;
    Sector *sector;

    if (gfw_MapInfoFlags() & MIF_SPAWN_ALL_FIREMACES)
    {
        // Randomized Firemace spawning is disabled.
        return;
    }

    DENG_ASSERT(mo && mo->type == MT_WMACE);
    App_Log(DE2_DEV_MAP_MSG, "P_RepositionMace: Repositioning mobj [%p], thinkerId:%i", mo, mo->thinker.id);

    mapSpot = P_ChooseRandomMaceSpot();
    if(!mapSpot)
    {
        App_Log(DE2_DEV_MAP_WARNING, "P_RepositionMace: Failed to choose a map spot, aborting...");
        return;
    }

    P_MobjUnlink(mo);
    {
        mo->origin[VX] = mapSpot->origin[VX];
        mo->origin[VY] = mapSpot->origin[VY];
        sector = Sector_AtPoint_FixedPrecision(mo->origin);

        mo->floorZ = P_GetDoublep(sector, DMU_CEILING_HEIGHT);
        mo->origin[VZ] = mo->floorZ;

        mo->ceilingZ = P_GetDoublep(sector, DMU_CEILING_HEIGHT);
    }
    P_MobjLink(mo);

    App_Log(DE2_DEV_MAP_MSG, "P_RepositionMace: Mobj [%p], thinkerId:%i - now at (%.2f, %.2f, %.2f)",
            mo, mo->thinker.id, mo->origin[VX], mo->origin[VY], mo->origin[VZ]);
}

void P_SpawnBloodSplatter(coord_t x, coord_t y, coord_t z, mobj_t* originator)
{
    mobj_t* mo;

    if((mo = P_SpawnMobjXYZ(MT_BLOODSPLATTER, x, y, z, P_Random() << 24, 0)))
    {
        mo->target = originator;
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
        mo->mom[MZ] = 2;
    }
}

/**
 * @return @c true, if mobj contacted a non-solid floor.
 */
dd_bool P_HitFloor(mobj_t* thing)
{
    mobj_t* mo;
    const terraintype_t* tt;

    if(IS_CLIENT && thing->player)
    {
        // The client notifies the server, which will handle the splash.
        NetCl_FloorHitRequest(thing->player);
        return false;
    }

    if(!FEQUAL(thing->floorZ, P_GetDoublep(Mobj_Sector(thing), DMU_FLOOR_HEIGHT)))
    {
        // Don't splash if landing on the edge above water/lava/etc...
        return false;
    }

    // Things that don't splash go here.
    switch(thing->type)
    {
    case MT_LAVASMOKE:
    case MT_SPLASH:
    case MT_SLUDGECHUNK:
        return false;

    default:
        if(P_MobjIsCamera(thing))
            return false;
        break;
    }

    tt = P_MobjFloorTerrain(thing);
    if(tt->flags & TTF_SPAWN_SPLASHES)
    {
        P_SpawnMobjXYZ(MT_SPLASHBASE, thing->origin[VX], thing->origin[VY],
                       0, thing->angle + ANG180, MSF_Z_FLOOR);

        if((mo = P_SpawnMobjXYZ(MT_SPLASH, thing->origin[VX], thing->origin[VY], 0,
                                thing->angle, MSF_Z_FLOOR)))
        {
            mo->target = thing;
            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MZ] = 2 + FIX2FLT(P_Random() << 8);

            S_StartSound(SFX_GLOOP, mo);
        }

        return true;
    }
    else if(tt->flags & TTF_SPAWN_SMOKE)
    {
        P_SpawnMobjXYZ(MT_LAVASPLASH, thing->origin[VX], thing->origin[VY], 0,
                       thing->angle + ANG180, MSF_Z_FLOOR);

        if((mo = P_SpawnMobjXYZ(MT_LAVASMOKE, thing->origin[VX], thing->origin[VY], 0,
                                P_Random() << 24, MSF_Z_FLOOR)))
        {
            mo->mom[MZ] = 1 + FIX2FLT((P_Random() << 7));

            S_StartSound(SFX_BURN, mo);
        }

        return true;
    }
    else if(tt->flags & TTF_SPAWN_SLUDGE)
    {
        P_SpawnMobjXYZ(MT_SLUDGESPLASH, thing->origin[VX], thing->origin[VY], 0,
                       thing->angle + ANG180, MSF_Z_FLOOR);

        if((mo = P_SpawnMobjXYZ(MT_SLUDGECHUNK, thing->origin[VX], thing->origin[VY], 0,
                                P_Random() << 24, MSF_Z_FLOOR)))
        {
            mo->target = thing;
            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MZ] = 1 + FIX2FLT(P_Random() << 8);
        }
        return true;
    }

    return false;
}

/**
 * @return  @c true, if the missile is at a valid spawn point,
 *          otherwise; explode it and return @false.
 */
dd_bool P_CheckMissileSpawn(mobj_t *mo)
{
    // Move a little forward so an angle can be computed if it immediately
    // explodes
    P_MobjUnlink(mo);
    if(mo->type == MT_BLASTERFX1)
    {
        // Ultra-fast ripper spawning missile.
        mo->origin[VX] += mo->mom[MX] / 8;
        mo->origin[VY] += mo->mom[MY] / 8;
        mo->origin[VZ] += mo->mom[MZ] / 8;
    }
    else
    {
        mo->origin[VX] += mo->mom[MX] / 2;
        mo->origin[VY] += mo->mom[MY] / 2;
        mo->origin[VZ] += mo->mom[MZ] / 2;
    }
    P_MobjLink(mo);

    if(!P_TryMoveXY(mo, mo->origin[VX], mo->origin[VY], false, false))
    {
        P_ExplodeMissile(mo);
        return false;
    }

    return true;
}

mobj_t* P_SpawnMissile(mobjtype_t type, mobj_t* source, mobj_t* dest, dd_bool checkSpawn)
{
    coord_t pos[3];
    mobj_t* th = 0;
    unsigned int an = 0;
    angle_t angle = 0;
    coord_t dist = 0;
    float slope = 0;
    coord_t spawnZOff = 0;
    int spawnFlags = 0;

    memcpy(pos, source->origin, sizeof(pos));

    if(source->player)
    {
        // see which target is to be aimed at
        angle = source->angle;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!cfg.common.noAutoAim)
            if(!lineTarget)
            {
                angle += 1 << 26;
                slope = P_AimLineAttack(source, angle, 16 * 64);
                if(!lineTarget)
                {
                    angle -= 2 << 26;
                    slope = P_AimLineAttack(source, angle, 16 * 64);
                }

                if(!lineTarget)
                {
                    angle = source->angle;
                    slope =
                        tan(LOOKDIR2RAD(source->dPlayer->lookDir)) / 1.2f;
                }
            }

        if(!P_MobjIsCamera(source->player->plr->mo))
            spawnZOff = cfg.common.plrViewHeight - 9 +
                source->player->plr->lookDir / 173;
    }
    else
    {
        // Type specific offset to spawn height z.
        switch(type)
        {
        case MT_MNTRFX1: // Minotaur swing attack missile.
            spawnZOff = 40;
            break;

        case MT_SRCRFX1: // Sorcerer Demon fireball.
            spawnZOff = 48;
            break;

        case MT_KNIGHTAXE: // Knight normal axe.
        case MT_REDAXE: // Knight red power axe.
            spawnZOff = 36;
            break;

        case MT_MNTRFX2:
            spawnZOff = 0;
            break;

        default:
            spawnZOff = 32;
            break;
        }
    }

    if(type == MT_MNTRFX2) // always exactly on the floor.
    {
        pos[VZ] = 0;
        spawnFlags |= MSF_Z_FLOOR;
    }
    else
    {
        pos[VZ] += spawnZOff;
        pos[VZ] -= source->floorClip;
    }

    if(!source->player)
    {
        angle = M_PointToAngle2(pos, dest->origin);
        // Fuzzy player.
        if(dest->flags & MF_SHADOW)
            angle += (P_Random() - P_Random()) << 21; // note << 20 in jDoom
    }

    if(!(th = P_SpawnMobj(type, pos, angle, spawnFlags)))
        return NULL;

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Where it came from.
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    if(source->player)
    {
        th->mom[MZ] = th->info->speed * slope;
    }
    else
    {
        dist = M_ApproxDistance(dest->origin[VX] - pos[VX],
                                dest->origin[VY] - pos[VY]);
        dist /= th->info->speed;
        if(dist < 1)
            dist = 1;
        th->mom[MZ] = (dest->origin[VZ] - source->origin[VZ]) / dist;
    }

    // Make sure the speed is right (in 3D).
    dist = M_ApproxDistance(M_ApproxDistance(th->mom[MX], th->mom[MY]),
                            th->mom[MZ]);
    if(!dist)
        dist = 1;
    dist = th->info->speed / dist;

    th->mom[MX] *= dist;
    th->mom[MY] *= dist;
    th->mom[MZ] *= dist;

//#if __JHERETIC__
    /// @kludge Set this global ptr as we need access to the mobj even if it
    ///         explodes instantly in order to assign values to it.
    missileMobj = th;
    // kludge end.
//#endif

    if(checkSpawn)
        return (P_CheckMissileSpawn(th)? th : NULL);

    return th;
}

mobj_t *Vanilla_P_SpawnMissileAngle(mobj_t *source, mobjtype_t type, angle_t angle, coord_t momZ)
{
    /*
     * NOTE: This function is intended to exactly replicate vanilla Heretic
     * behavior. Do not modify!
     */

    coord_t pos[3] = { source->origin[VX], source->origin[VY], source->origin[VZ] + 32 };
    mobj_t *mo;
    int spawnFlags = 0;

    // Determine missile spawn position.
    switch(type)
    {
    case MT_MNTRFX1: // Minotaur swing attack missile
        pos[VZ] = source->origin[VZ] + 40;
        break;

    case MT_MNTRFX2: // Minotaur floor fire missile
        spawnFlags |= MSF_Z_FLOOR;
        break;

    case MT_SRCRFX1: // Sorcerer Demon fireball
        pos[VZ] = source->origin[VZ] + 48;
        break;

    default:
        break;
    }

    pos[VZ] -= source->floorClip;

    mo = P_SpawnMobj(type, pos, angle, spawnFlags);

    mo->target = source; // Originator
    mo->angle = angle;
    angle >>= ANGLETOFINESHIFT;
    mo->mom[VX] = mo->info->speed * FIX2FLT(finecosine[angle]);
    mo->mom[VY] = mo->info->speed * FIX2FLT(finesine[angle]);
    mo->mom[VZ] = momZ;

    if(mo->info->seeSound)
    {
        S_StartSound(mo->info->seeSound, mo);
    }

    return (P_CheckMissileSpawn(mo)? mo : NULL);
}

mobj_t* P_SpawnMissileAngle(mobjtype_t type, mobj_t* source, angle_t mangle, coord_t momZ)
{
    coord_t pos[3];
    mobj_t* th = 0;
    unsigned int an = 0;
    angle_t angle = 0;
    coord_t dist = 0;
    float slope = 0;
    coord_t spawnZOff = 0;
    int spawnFlags = 0;

    memcpy(pos, source->origin, sizeof(pos));

    angle = mangle;
    if(source->player)
    {
        // Try to find a target.
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!cfg.common.noAutoAim)
            if(!lineTarget)
            {
                angle += 1 << 26;
                slope = P_AimLineAttack(source, angle, 16 * 64);
                if(!lineTarget)
                {
                    angle -= 2 << 26;
                    slope = P_AimLineAttack(source, angle, 16 * 64);
                }

                if(!lineTarget)
                {
                    angle = mangle;
                    slope =
                        tan(LOOKDIR2RAD(source->dPlayer->lookDir)) / 1.2f;
                }
            }

        if(!(source->player->plr->flags & DDPF_CAMERA))
            spawnZOff = cfg.common.plrViewHeight - 9 +
                        (source->player->plr->lookDir) / 173;
    }
    else
    {
        // Type specific offset to spawn height z.
        switch(type)
        {
        case MT_MNTRFX1: // Minotaur swing attack missile.
            spawnZOff = 40;
            break;

        case MT_SRCRFX1: // Sorcerer Demon fireball.
            spawnZOff = 48;
            break;

        case MT_KNIGHTAXE: // Knight normal axe.
        case MT_REDAXE: // Knight red power axe.
            spawnZOff = 36;
            break;

        default:
            spawnZOff = 32;
            break;
        }
    }

    if(type == MT_MNTRFX2) // Always exactly on the floor.
    {
        spawnFlags |= MSF_Z_FLOOR;
    }
    else
    {
        pos[VZ] += spawnZOff;
        pos[VZ] -= source->floorClip;
    }

    if(!(th = P_SpawnMobj(type, pos, angle, spawnFlags)))
        return NULL;

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Where it came from.
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    if(source->player && momZ == -12345)
    {
        th->mom[MZ] = th->info->speed * slope;

        // Make sure the speed is right (in 3D).
        dist = M_ApproxDistance(M_ApproxDistance(th->mom[MX], th->mom[MY]),
                                th->mom[MZ]);
        if(dist < 1)
            dist = 1;
        dist = th->info->speed / dist;

        th->mom[MX] *= dist;
        th->mom[MY] *= dist;
        th->mom[MZ] *= dist;
    }
    else
    {
        th->mom[MZ] = momZ;
    }

//#if __JHERETIC__
    /// @kludge Set this global ptr as we need access to the mobj even if it
    ///         explodes instantly in order to assign values to it.
    missileMobj = th;
    // kludge end.
//#endif

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
}

void C_DECL A_ContMobjSound(mobj_t* actor)
{
    switch(actor->type)
    {
    case MT_KNIGHTAXE:
        S_StartSound(SFX_KGTATK, actor);
        break;

    case MT_MUMMYFX1:
        S_StartSound(SFX_MUMHED, actor);
        break;

    default:
        break;
    }
}
