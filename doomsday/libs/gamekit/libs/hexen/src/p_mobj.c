/** @file p_mobj.c  World map object interaction.
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

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include "jhexen.h"
#include "p_mobj.h"

#include <math.h>
#include <string.h>
#include <de/legacy/binangle.h>
#include "d_netcl.h"
#include "dmu_lib.h"
#include "g_common.h"
#include "p_map.h"
#include "player.h"

#define MAX_BOB_OFFSET          8

#define BLAST_RADIUS_DIST       255
#define BLAST_SPEED             20
#define BLAST_FULLSTRENGTH      255
#define HEAL_RADIUS_DIST        255

#define SMALLSPLASHCLIP         (12);

void P_ExplodeMissile(mobj_t *mo)
{
    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }

    switch(mo->type)
    {
    case MT_SORCBALL1:
    case MT_SORCBALL2:
    case MT_SORCBALL3:
        S_StartSound(SFX_SORCERER_BIGBALLEXPLODE, NULL);
        break;

    case MT_SORCFX1:
        S_StartSound(SFX_SORCERER_HEADSCREAM, NULL);
        break;

    default:
        if(mo->info->deathSound)
            S_StartSound(mo->info->deathSound, mo);
        break;
    }
}

void P_FloorBounceMissile(mobj_t* mo)
{
    dd_bool             shouldSplash = P_HitFloor(mo);

    if(shouldSplash)
    {
        switch(mo->type)
        {
        case MT_SORCFX1:
        case MT_SORCBALL1:
        case MT_SORCBALL2:
        case MT_SORCBALL3:
            break;

        default:
            P_MobjRemove(mo, false);
            return;
        }
    }

    switch(mo->type)
    {
    case MT_SORCFX1:
        mo->mom[MZ] = -mo->mom[MZ]; // No energy absorbed.
        break;

    case MT_SGSHARD1:
    case MT_SGSHARD2:
    case MT_SGSHARD3:
    case MT_SGSHARD4:
    case MT_SGSHARD5:
    case MT_SGSHARD6:
    case MT_SGSHARD7:
    case MT_SGSHARD8:
    case MT_SGSHARD9:
    case MT_SGSHARD0:
        mo->mom[MZ] *= -0.3f;
        if(fabs(mo->mom[MZ]) < 1.0f / 2)
        {
            P_MobjChangeState(mo, S_NULL);
            return;
        }
        break;

    default:
        mo->mom[MZ] *= -0.7f;
        break;
    }

    mo->mom[MX] = 2 * mo->mom[MX] / 3;
    mo->mom[MY] = 2 * mo->mom[MY] / 3;
    if(mo->info->seeSound)
    {
        switch(mo->type)
        {
        case MT_SORCBALL1:
        case MT_SORCBALL2:
        case MT_SORCBALL3:
            if(!mo->args[0])
                S_StartSound(mo->info->seeSound, mo);
            break;

        default:
            S_StartSound(mo->info->seeSound, mo);
            break;
        }

        S_StartSound(mo->info->seeSound, mo);
    }
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
 * @return  @c 1, if 'source' needs to turn clockwise, or
 *          @c 0, if 'source' needs to turn counter clockwise.
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
 * The missile tracer field must be mobj_t *target.
 *
 * @return              @c true, if target was tracked.
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
    {
        // Target died.
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

    if(dir) // Turn clockwise.
        actor->angle += delta;
    else // Turn counter clockwise.
        actor->angle -= delta;

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = actor->info->speed * FIX2FLT(finecosine[an]);
    actor->mom[MY] = actor->info->speed * FIX2FLT(finesine[an]);

    if(actor->origin[VZ]  + actor->height  < target->origin[VZ] ||
       target->origin[VZ] + target->height < actor->origin[VZ])
    {
        // Need to seek vertically
        dist = M_ApproxDistance(target->origin[VX] - actor->origin[VX],
                                target->origin[VY] - actor->origin[VY]);
        dist /= actor->info->speed;
        if(dist < 1)
            dist = 1;

        actor->mom[MZ] = (target->origin[VZ] + (target->height /2) -
                          (actor->origin[VZ] + (actor->height /2))) / dist;
    }

    return true;
}

/*
static __inline dd_bool isInWalkState(player_t* pl)
{
    return pl->plr->mo->state - STATES - PCLASS_INFO(pl->class_)->runState < 4;
}
*/

void P_MobjMoveXY(mobj_t* mo)
{
    static const coord_t windTab[3] = {
        2048.0 / FRACUNIT * 5,
        2048.0 / FRACUNIT * 10,
        2048.0 / FRACUNIT * 25
    };

    coord_t posTry[2], mom[2];
    player_t* player;
    angle_t angle;

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    if(IS_ZERO(mo->mom[MX]) && IS_ZERO(mo->mom[MY]))
    {
        if(mo->flags & MF_SKULLFLY)
        {
            // A flying mobj slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
            P_MobjChangeState(mo, P_GetState(mo->type, SN_SEE));
        }
        return;
    }

    if(mo->flags2 & MF2_WINDTHRUST)
    {
        int special = P_ToXSector(Mobj_Sector(mo))->special;
        switch(special)
        {
        case 40:
        case 41:
        case 42: // Wind_East
            P_ThrustMobj(mo, 0, windTab[special - 40]);
            break;

        case 43:
        case 44:
        case 45: // Wind_North
            P_ThrustMobj(mo, ANG90, windTab[special - 43]);
            break;

        case 46:
        case 47:
        case 48: // Wind_South
            P_ThrustMobj(mo, ANG270, windTab[special - 46]);
            break;

        case 49:
        case 50:
        case 51: // Wind_West
            P_ThrustMobj(mo, ANG180, windTab[special - 49]);
            break;
        }
    }

    mom[MX] = MINMAX_OF(-MAXMOM, mo->mom[MX], MAXMOM);
    mom[MY] = MINMAX_OF(-MAXMOM, mo->mom[MY], MAXMOM);
    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];

    player = mo->player;
    do
    {
        if(mom[VX] > MAXMOMSTEP || mom[VY] > MAXMOMSTEP)
        {
            posTry[VX] = mo->origin[VX] + mom[VX] / 2;
            posTry[VY] = mo->origin[VY] + mom[VY] / 2;
            mom[VX] /= 2;
            mom[VY] /= 2;
        }
        else
        {
            posTry[VX] = mo->origin[VX] + mom[VX];
            posTry[VY] = mo->origin[VY] + mom[VY];
            mom[VX] = mom[VY] = 0;
        }

        if(!P_TryMoveXY(mo, posTry[VX], posTry[VY]))
        {   // Blocked mom.
            if(mo->flags2 & MF2_SLIDE)
            {   // Try to slide along it.
                if(tmBlockingMobj == NULL)
                {   // Slide against wall.
                    P_SlideMove(mo);
                }
                else
                {   // Slide against mobj.
                    if(P_TryMoveXY(mo, mo->origin[VX], posTry[VY]))
                    {
                        mo->mom[MX] = 0;
                    }
                    else if(P_TryMoveXY(mo, posTry[VX], mo->origin[VY]))
                    {
                        mo->mom[MY] = 0;
                    }
                    else
                    {
                        mo->mom[MX] = mo->mom[MY] = 0;
                    }
                }
            }
            else if(mo->flags & MF_MISSILE)
            {
                Sector* backSec;

                if(mo->flags2 & MF2_FLOORBOUNCE)
                {
                    if(tmBlockingMobj)
                    {
                        if((tmBlockingMobj->flags2 & MF2_REFLECTIVE) ||
                           ((!tmBlockingMobj->player) &&
                            (!(tmBlockingMobj->flags & MF_COUNTKILL))))
                        {
                            coord_t speed;

                            angle = M_PointToAngle2(tmBlockingMobj->origin, mo->origin) +
                                ANGLE_1 * ((P_Random() % 16) - 8);

                            speed = M_ApproxDistance(mo->mom[MX], mo->mom[MY]);
                            speed *= 0.75;

                            mo->angle = angle;
                            angle >>= ANGLETOFINESHIFT;
                            mo->mom[MX] = speed * FIX2FLT(finecosine[angle]);
                            mo->mom[MY] = speed * FIX2FLT(finesine[angle]);
                            if(mo->info->seeSound)
                                S_StartSound(mo->info->seeSound, mo);

                            return;
                        }
                        else
                        {
                            // Struck a player/creature
                            P_ExplodeMissile(mo);
                        }
                    }
                    else
                    {
                        // Struck a wall
                        P_BounceWall(mo);
                        switch(mo->type)
                        {
                        case MT_SORCBALL1:
                        case MT_SORCBALL2:
                        case MT_SORCBALL3:
                        case MT_SORCFX1:
                            break;

                        default:
                            if(mo->info->seeSound)
                                S_StartSound(mo->info->seeSound, mo);
                            break;
                        }

                        return;
                    }
                }

                if(tmBlockingMobj && (tmBlockingMobj->flags2 & MF2_REFLECTIVE))
                {
                    angle = M_PointToAngle2(tmBlockingMobj->origin, mo->origin);

                    // Change angle for deflection/reflection
                    switch(tmBlockingMobj->type)
                    {
                    case MT_CENTAUR:
                    case MT_CENTAURLEADER:
                        if(abs((int)(angle - tmBlockingMobj->angle)) >> 24 > 45)
                            goto explode;
                        if(mo->type == MT_HOLY_FX)
                            goto explode;
                        // Drop through to sorcerer full reflection
                    case MT_SORCBOSS:
                        // Deflection
                        if(P_Random() < 128)
                            angle += ANGLE_45;
                        else
                            angle -= ANGLE_45;
                        break;

                    default:
                        // Reflection
                        angle += ANGLE_1 * ((P_Random() % 16) - 8);
                        break;
                    }

                    // Reflect the missile along angle
                    mo->angle = angle;
                    angle >>= ANGLETOFINESHIFT;

                    mo->mom[MX] = (mo->info->speed / 2) * FIX2FLT(finecosine[angle]);
                    mo->mom[MY] = (mo->info->speed / 2) * FIX2FLT(finesine[angle]);

                    if(mo->flags2 & MF2_SEEKERMISSILE)
                    {
                        mo->tracer = mo->target;
                    }
                    mo->target = tmBlockingMobj;

                    return;
                }

explode:
                // Explode a missile

                /// @kludge: Prevent missiles exploding against the sky.
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
                        else if(mo->type == MT_HOLY_FX)
                        {
                            P_ExplodeMissile(mo);
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
                        else if(mo->type == MT_HOLY_FX)
                        {
                            P_ExplodeMissile(mo);
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

    // Friction
    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {   // Debug option for no sliding at all
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }
    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
        return; // No friction for missiles

    if(mo->origin[VZ] > mo->floorZ && !(mo->flags2 & MF2_FLY) && !mo->onMobj)
    {    // No friction when falling
        if(mo->type != MT_BLASTEFFECT)
            return;
    }

    if(mo->flags & MF_CORPSE)
    {
        // Do not stop sliding if halfway off a step with some momentum.
        if(!INRANGE_OF(mo->mom[MX], 0, DROPOFFMOM_THRESHOLD) ||
           !INRANGE_OF(mo->mom[MY], 0, DROPOFFMOM_THRESHOLD))
        {
            if(!FEQUAL(mo->floorZ, P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT)))
                return;
        }
    }

    // Stop player walking animation.
    if((!player || (!(player->plr->forwardMove || player->plr->sideMove))) &&
       INRANGE_OF(mo->mom[MX], 0, WALKSTOP_THRESHOLD) &&
       INRANGE_OF(mo->mom[MY], 0, WALKSTOP_THRESHOLD))
    {   // If in a walking frame, stop moving
        if(player)
        {
            if((unsigned)
               ((player->plr->mo->state - STATES) - PCLASS_INFO(player->class_)->runState) < 4)
            {
                P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->normalState);
            }
        }
        mo->mom[MX] = 0;
        mo->mom[MY] = 0;
    }
    else
    {
        coord_t friction = Mobj_Friction(mo);
        mo->mom[MX] *= friction;
        mo->mom[MY] *= friction;
    }
}

/**
 * @todo Move this to p_inter
 */
void P_MonsterFallingDamage(mobj_t* mo)
{
    int damage;

    /*
     * Note: See Vanilla Hexen sources P_MOBJ.C:658. `damage` is calculated but
     * 10000 is used anyway.
     */

    /*
    coord_t mom;

    mom = (int) fabs(mo->mom[MZ]);
    if(mom > 35)
    {
        // automatic death
        damage = 10000;
    }
    else
    {
        damage = (int) ((mom - 23) * 6);
    }*/

    damage = 10000; // always kill 'em.

    P_DamageMobj(mo, NULL, NULL, damage, false);
}

void P_MobjMoveZ(mobj_t* mo)
{
    coord_t gravity, dist, delta;

    // $democam: cameramen get special z movement
    if(P_CameraZMovement(mo)) return;

    gravity = P_GetGravity();

    // Check for smooth step up.
    if(mo->player && mo->origin[VZ] < mo->floorZ)
    {
        mo->player->viewHeight -= mo->floorZ - mo->origin[VZ];
        mo->player->viewHeightDelta =
            (cfg.common.plrViewHeight - mo->player->viewHeight) / 8;
    }

    // Adjust height.
    mo->origin[VZ] += mo->mom[MZ];
    if((mo->flags & MF_FLOAT) && mo->target && !P_MobjIsCamera(mo->target))
    {
        // Float down towards target if too close.
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = M_ApproxDistance(mo->origin[VX] - mo->target->origin[VX],
                                    mo->origin[VY] - mo->target->origin[VY]);
            delta = (mo->target->origin[VZ] + (mo->height /2)) - mo->origin[VZ];
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
        }
    }

    if(mo->player && (mo->flags2 & MF2_FLY) &&
       !(mo->origin[VZ] <= mo->floorZ) && (mapTime & 2))
    {
        mo->origin[VZ] +=
            FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
    }

    // Clip movement.
    if(mo->origin[VZ] <= mo->floorZ)
    {   // Hit the floor
        statenum_t              state;

        if(mo->flags & MF_MISSILE)
        {
            mo->origin[VZ] = mo->floorZ;
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
            else if(mo->type == MT_HOLY_FX)
            {   // The spirit struck the ground.
                mo->mom[MZ] = 0;
                P_HitFloor(mo);
                return;
            }
            else if(mo->type == MT_MNTRFX2 || mo->type == MT_LIGHTNING_FLOOR)
            {   // Minotaur floor fire can go up steps.
                return;
            }
            else
            {
                P_HitFloor(mo);
                P_ExplodeMissile(mo);
                return;
            }
        }

        if(mo->flags & MF_COUNTKILL)
        {
            // Blasted mobj falling.
            if(mo->mom[MZ] < -23)
            {
                P_MonsterFallingDamage(mo);
            }
        }

        if(mo->origin[VZ] - mo->mom[MZ] > mo->floorZ)
        {   // Spawn splashes, etc.
            P_HitFloor(mo);
        }

        mo->origin[VZ] = mo->floorZ;
        if(mo->mom[MZ] < 0)
        {
            if((mo->flags2 & MF2_ICEDAMAGE) && mo->mom[MZ] < -gravity * 8)
            {
                mo->tics = 1;
                mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
                return;
            }

            if(mo->player)
            {
                mo->player->jumpTics = 7; // delay any jumping for a short time
                if(mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
                {   // Squat down.
                    mo->player->viewHeightDelta = mo->mom[MZ] / 8;
                    if(mo->mom[MZ] < -23)
                    {
                        P_FallingDamage(mo->player);
                        P_NoiseAlert(mo, mo);
                    }
                    else if(mo->mom[MZ] < -gravity * 12 && !mo->player->morphTics)
                    {
                        S_StartSound(SFX_PLAYER_LAND, mo);

                        // Fix DOOM bug - dead players grunting when hitting the ground
                        // (e.g., after an archvile attack)
                        if(mo->player->health > 0)
                            switch(mo->player->class_)
                            {
                            case PCLASS_FIGHTER:
                                S_StartSound(SFX_PLAYER_FIGHTER_GRUNT, mo);
                                break;

                            case PCLASS_CLERIC:
                                S_StartSound(SFX_PLAYER_CLERIC_GRUNT, mo);
                                break;

                            case PCLASS_MAGE:
                                S_StartSound(SFX_PLAYER_MAGE_GRUNT, mo);
                                break;

                            default:
                                break;
                            }
                    }
                    else if(!mo->player->morphTics)
                    {
                        const terraintype_t* tt =
                            P_MobjFloorTerrain(mo);

                        if(!(tt->flags & TTF_NONSOLID))
                            S_StartSound(SFX_PLAYER_LAND, mo);
                    }

                    if(cfg.common.lookSpring)
                        mo->player->centering = true;
                }
            }
            else if(mo->type >= MT_POTTERY1 && mo->type <= MT_POTTERY3)
            {
                P_DamageMobj(mo, NULL, NULL, 25, false);
            }
            else if(mo->flags & MF_COUNTKILL)
            {
                if(mo->mom[MZ] < -23)
                {
                    // Doesn't get here
                }
            }
            mo->mom[MZ] = 0;
        }

        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if((state = P_GetState(mo->type, SN_CRASH)) != S_NULL &&
           (mo->flags & MF_CORPSE) && !(mo->flags2 & MF2_ICEDAMAGE))
        {
            P_MobjChangeState(mo, state);
            return;
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
    {   // hit the ceiling
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->origin[VZ] = mo->ceilingZ - mo->height;
        if(mo->flags2 & MF2_FLOORBOUNCE)
        {
            // Maybe reverse momentum here for ceiling bounce
            // Currently won't happen
            if(mo->info->seeSound)
            {
                S_StartSound(mo->info->seeSound, mo);
            }
            return;
        }

        if(mo->flags & MF_SKULLFLY)
        {   // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(mo->flags & MF_MISSILE)
        {
            if(mo->type == MT_LIGHTNING_CEILING)
                return;

            if(P_GetIntp(P_GetPtrp(Mobj_Sector(mo), DMU_CEILING_MATERIAL),
                         DMU_FLAGS) & MATF_SKYMASK)
            {
                if(mo->type == MT_BLOODYSKULL)
                {
                    mo->mom[MX] = mo->mom[MY] = 0;
                    mo->mom[MZ] = -1;
                }
                else if(mo->type == MT_HOLY_FX)
                {
                    P_ExplodeMissile(mo);
                }
                else
                {
                    P_MobjRemove(mo, false);
                }

                return;
            }

            P_ExplodeMissile(mo);
            return;
        }
    }
}

static void landedOnThing(mobj_t* mo)
{
    if(!mo || !mo->player)
        return; // We are only interested in players.

    mo->player->viewHeightDelta = mo->mom[MZ] / 8;
    if(mo->mom[MZ] < -23)
    {
        P_FallingDamage(mo->player);
        P_NoiseAlert(mo, mo);
    }
    else if(mo->mom[MZ] < -P_GetGravity() * 12 && !mo->player->morphTics)
    {
        S_StartSound(SFX_PLAYER_LAND, mo);
        switch(mo->player->class_)
        {
        case PCLASS_FIGHTER:
            S_StartSound(SFX_PLAYER_FIGHTER_GRUNT, mo);
            break;

        case PCLASS_CLERIC:
            S_StartSound(SFX_PLAYER_CLERIC_GRUNT, mo);
            break;

        case PCLASS_MAGE:
            S_StartSound(SFX_PLAYER_MAGE_GRUNT, mo);
            break;

        default:
            break;
        }
    }
    else if(!mo->player->morphTics)
    {
        S_StartSound(SFX_PLAYER_LAND, mo);
    }

    if(cfg.common.lookSpring) // || demorecording || demoplayback)
        mo->player->centering = true;
}

void P_MobjThinker(void *thinkerPtr)
{
    mobj_t *mobj = thinkerPtr;

    if(IS_CLIENT && !ClMobj_IsValid(mobj))
        return; // We should not touch this right now.

    if(mobj->type == MT_MWAND_MISSILE || mobj->type == MT_CFLAME_MISSILE)
    {
        int i;
        coord_t z, frac[3];
        dd_bool changexy;

        // Handle movement.
        if(NON_ZERO(mobj->mom[MX]) || NON_ZERO(mobj->mom[MY]) || NON_ZERO(mobj->mom[MZ]) ||
           !FEQUAL(mobj->origin[VZ], mobj->floorZ))
        {
            frac[VX] = mobj->mom[MX] / 8;
            frac[VY] = mobj->mom[MY] / 8;
            frac[VZ] = mobj->mom[MZ] / 8;
            changexy = (NON_ZERO(frac[VX]) || NON_ZERO(frac[VY]));

            for(i = 0; i < 8; ++i)
            {
                if(changexy)
                {
                    if(!P_TryMoveXY(mobj, mobj->origin[VX] + frac[VX],
                                          mobj->origin[VY] + frac[VY]))
                    {
                        // Blocked move.
                        P_ExplodeMissile(mobj);
                        return;
                    }
                }

                mobj->origin[VZ] += frac[VZ];
                if(mobj->origin[VZ] <= mobj->floorZ)
                {
                    // Hit the floor.
                    mobj->origin[VZ] = mobj->floorZ;
                    P_HitFloor(mobj);
                    P_ExplodeMissile(mobj);
                    return;
                }

                if(mobj->origin[VZ] + mobj->height > mobj->ceilingZ)
                {
                    // Hit the ceiling.
                    mobj->origin[VZ] = mobj->ceilingZ - mobj->height;
                    P_ExplodeMissile(mobj);
                    return;
                }

                if(changexy)
                {
                    mobj_t *fx;

                    if(mobj->type == MT_MWAND_MISSILE && (P_Random() < 128))
                    {
                        z = mobj->origin[VZ] - 8;
                        if(z < mobj->floorZ)
                        {
                            z = mobj->floorZ;
                        }

                        fx = P_SpawnMobjXYZ(MT_MWANDSMOKE, mobj->origin[VX],
                                            mobj->origin[VY], z, mobj->angle, 0);

                        // Give a small amount of momentum so the movement direction
                        // can be determined.
                        V3d_Copy (fx->mom, mobj->mom);
                        V3d_Scale(fx->mom, 0.0001 / V3d_Length(fx->mom));
                    }
                    else if(!--mobj->special1)
                    {
                        mobj->special1 = 4;
                        z = mobj->origin[VZ] - 12;
                        if(z < mobj->floorZ)
                        {
                            z = mobj->floorZ;
                        }

                        P_SpawnMobjXYZ(MT_CFLAMEFLOOR, mobj->origin[VX],
                                       mobj->origin[VY], z, mobj->angle, 0);
                    }
                }
            }
        }

        // Advance the state.
        if(mobj->tics != -1)
        {
            mobj->tics--;
            while(!mobj->tics)
            {
                if(!P_MobjChangeState(mobj, mobj->state->nextState))
                    return; // Mobj was removed.
            }
        }

        return;
    }

    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mobj);

    // Handle X and Y momentums
    tmBlockingMobj = NULL;
    if(NON_ZERO(mobj->mom[MX]) || NON_ZERO(mobj->mom[MY]) ||
       (mobj->flags & MF_SKULLFLY))
    {
        P_MobjMoveXY(mobj);
        if(mobj->thinker.function == (thinkfunc_t) NOPFUNC)
        {   // Mobj was removed.
            return;
        }
    }
    else if(mobj->flags2 & MF2_BLASTED)
    {   // Reset to not blasted when momentums are gone.
        ResetBlasted(mobj);
    }

    if(mobj->flags2 & MF2_FLOATBOB)
    {
        // Keep it on the floor.
        mobj->origin[VZ] = mobj->floorZ;

        // Negative floorclip raises the mobj off the floor.
        mobj->floorClip = -mobj->special1;
        if(mobj->floorClip < -MAX_BOB_OFFSET)
        {
            // We don't want it going through the floor.
            mobj->floorClip = -MAX_BOB_OFFSET;
        }
    }
    else if(!FEQUAL(mobj->origin[VZ], mobj->floorZ) || NON_ZERO(mobj->mom[MZ]) || tmBlockingMobj)
    {
        // Handle Z momentum and gravity
        if(mobj->flags2 & MF2_PASSMOBJ)
        {
            mobj->onMobj = P_CheckOnMobj(mobj);
            if(!mobj->onMobj)
            {
                P_MobjMoveZ(mobj);
            }
            else
            {
                if(mobj->mom[MZ] < -P_GetGravity() * 8 && !(mobj->flags2 & MF2_FLY))
                {
                    landedOnThing(mobj);
                }

                if(mobj->onMobj->origin[VZ] + mobj->onMobj->height - mobj->origin[VZ] <= 24)
                {
                    if(mobj->player)
                    {
                        mobj->player->viewHeight -=
                            mobj->onMobj->origin[VZ] + mobj->onMobj->height - mobj->origin[VZ];
                        mobj->player->viewHeightDelta =
                            (cfg.common.plrViewHeight - mobj->player->viewHeight) / 8;
                    }

                    mobj->origin[VZ] = mobj->onMobj->origin[VZ] + mobj->onMobj->height;
                    mobj->mom[MZ] = 0;

                    // Adjust floorZ to the top of the contacted mobj.
                    mobj->floorZ = MAX_OF(mobj->floorZ, mobj->onMobj->origin[VZ] + mobj->onMobj->height);
                }
                else
                {   // hit the bottom of the blocking mobj
                    mobj->mom[MZ] = 0;
                }
            }
        }
        else
        {
            P_MobjMoveZ(mobj);
        }

        if(mobj->thinker.function == (thinkfunc_t) NOPFUNC)
        {
            // mobj was removed
            return;
        }
    }
    
    P_MobjAngleSRVOTicker(mobj);
    
    // Cycle through states, calling action functions at transitions.
    if(mobj->tics != -1)
    {
        mobj->tics--;
        // You can cycle through multiple states in a tic.
        while(!mobj->tics)
        {
            P_MobjClearSRVO(mobj);
            if(!P_MobjChangeState(mobj, mobj->state->nextState))
            {   // Mobj was removed.
                return;
            }
        }
    }

    // Ice corpses aren't going anywhere.
    if(mobj->flags & MF_ICECORPSE)
        P_MobjSetSRVO(mobj, 0, 0);
}

mobj_t* P_SpawnMobjXYZ(mobjtype_t type, coord_t x, coord_t y, coord_t z,
    angle_t angle, int spawnFlags)
{
    mobj_t* mo;
    mobjinfo_t* info;
    coord_t space;
    int ddflags = 0;

    if(type == MT_ZLYNCHED_NOHEART)
    {
        type = MT_BLOODPOOL;
        angle = 0;
        spawnFlags |= MSF_Z_FLOOR;
    }

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

    // Don't spawn any monsters?
    if(gfw_Rule(noMonsters) && (info->flags & MF_COUNTKILL))
        return NULL;

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
    // This doesn't appear to actually be used see P_DamageMobj in P_inter.c
    mo->damage = info->damage;
    mo->health = info->spawnHealth * (IS_NETGAME ? cfg.common.netMobHealthModifier : 1);
    mo->moveDir = DI_NODIR;
    mo->selector = 0;
    P_UpdateHealthBits(mo); // Set the health bits of the selector.

    if(gfw_Rule(skill) != SM_NIGHTMARE)
    {
        mo->reactionTime = info->reactionTime;
    }
    mo->lastLook = P_Random() % MAXPLAYERS;

    Mobj_SetState(mo, P_GetState(mo->type, SN_SPAWN));

    // Link the mobj into the world.
    P_MobjLink(mo);

    mo->floorZ   = P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT);
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
    {
        mo->flags |= MF_AMBUSH;
    }

    mo->floorClip = 0;

    if((mo->flags2 & MF2_FLOORCLIP) &&
       FEQUAL(mo->origin[VZ], P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT)))
    {
        terraintype_t const *tt = P_MobjFloorTerrain(mo);
        if(tt->flags & TTF_FLOORCLIP)
            mo->floorClip = 10;
    }

    if(spawnFlags & MTF_DORMANT)
    {
        mo->flags2 |= MF2_DORMANT;
        if(mo->type == MT_ICEGUY)
            P_MobjChangeState(mo, S_ICEGUY_DORMANT);
        mo->tics = -1;
    }

    return mo;
}

mobj_t *P_SpawnMobj(mobjtype_t type, coord_t const pos[3], angle_t angle, int spawnFlags)
{
    return P_SpawnMobjXYZ(type, pos[VX], pos[VY], pos[VZ], angle, spawnFlags);
}

void P_SpawnBloodSplatter(coord_t x, coord_t y, coord_t z, mobj_t* originator)
{
    mobj_t* mo;
    if((mo = P_SpawnMobjXYZ(MT_BLOODSPLATTER, x, y, z, P_Random() << 24, 0)))
    {
        mo->target = originator;
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MZ] = 3;
    }
}

void P_SpawnBloodSplatter2(coord_t x, coord_t y, coord_t z, mobj_t* originator)
{
    mobj_t* mo;
    if((mo = P_SpawnMobjXYZ(MT_AXEBLOOD,
                           x + FIX2FLT((P_Random() - 128) << 11),
                           y + FIX2FLT((P_Random() - 128) << 11),
                           z, P_Random() << 24, 0)))
    {
        mo->target = originator;
    }
}

dd_bool P_HitFloor(mobj_t *thing)
{
    mobj_t *mo;
    int smallsplash = false;
    terraintype_t const *tt;

    if(!thing->info)
        return false;

    if(IS_CLIENT && thing->player)
    {
        // The client notifies the server, which will handle the splash.
        NetCl_FloorHitRequest(thing->player);
        return false;
    }

    if(!FEQUAL(thing->floorZ, P_GetDoublep(Mobj_Sector(thing), DMU_FLOOR_HEIGHT)))
    {
        // Don't splash if landing on the edge above water/lava/etc....
        return false;
    }

    // Things that don't splash go here
    switch(thing->type)
    {
    case MT_LEAF1:
    case MT_LEAF2:
    case MT_SPLASH:
    case MT_SLUDGECHUNK:
    case MT_FOGPATCHS:
    case MT_FOGPATCHM:
    case MT_FOGPATCHL:
        return false;

    default:
        if(P_MobjIsCamera(thing))
            return false;
        break;
    }

    // Small splash for small masses.
    if(thing->info->mass < 10)
        smallsplash = true;

    tt = P_MobjFloorTerrain(thing);
    if(tt->flags & TTF_SPAWN_SPLASHES)
    {
        if(smallsplash)
        {
            if((mo = P_SpawnMobjXYZ(MT_SPLASHBASE, thing->origin[VX],
                                   thing->origin[VY], 0, thing->angle + ANG180,
                                   MSF_Z_FLOOR)))
            {

                mo->floorClip += SMALLSPLASHCLIP;
                S_StartSound(SFX_AMBIENT10, mo); // small drip
            }
        }
        else
        {
            if((mo = P_SpawnMobjXYZ(MT_SPLASH, thing->origin[VX], thing->origin[VY], 0,
                                  P_Random() << 24, MSF_Z_FLOOR)))
            {
                mo->target = thing;
                mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
                mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
                mo->mom[MZ] = 2 + FIX2FLT(P_Random() << 8);

                mo = P_SpawnMobjXYZ(MT_SPLASHBASE, thing->origin[VX], thing->origin[VY],
                                   0, thing->angle + ANG180, MSF_Z_FLOOR);
                S_StartSound(SFX_WATER_SPLASH, mo);
            }

            if(thing->player)
                P_NoiseAlert(thing, thing);
        }

        return true;
    }
    else if(tt->flags & TTF_SPAWN_SMOKE)
    {
        if(smallsplash)
        {
            if((mo = P_SpawnMobjXYZ(MT_LAVASPLASH, thing->origin[VX], thing->origin[VY],
                                   0, P_Random() << 24, MSF_Z_FLOOR)))
                mo->floorClip += SMALLSPLASHCLIP;
        }
        else
        {
            if((mo = P_SpawnMobjXYZ(MT_LAVASMOKE, thing->origin[VX], thing->origin[VY],
                                   0, P_Random() << 24, MSF_Z_FLOOR)))
            {
                mo->mom[MZ] = 1 + FIX2FLT(P_Random() << 7);
                mo = P_SpawnMobjXYZ(MT_LAVASPLASH, thing->origin[VX], thing->origin[VY],
                                   0, P_Random() << 24, MSF_Z_FLOOR);
            }

            if(thing->player)
                P_NoiseAlert(thing, thing);
        }

        S_StartSound(SFX_LAVA_SIZZLE, mo);
        if(thing->player && mapTime & 31)
        {
            P_DamageMobj(thing, P_LavaInflictor(), NULL, 5, false);
        }
        return true;
    }
    else if(tt->flags & TTF_SPAWN_SLUDGE)
    {
        mo = NULL;

        if(smallsplash)
        {
            if((mo = P_SpawnMobjXYZ(MT_SLUDGESPLASH, thing->origin[VX],
                                   thing->origin[VY], 0, P_Random() << 24,
                                   MSF_Z_FLOOR)))
            {
                mo->floorClip += SMALLSPLASHCLIP;
            }
        }
        else
        {
            if((mo = P_SpawnMobjXYZ(MT_SLUDGECHUNK, thing->origin[VX],
                                   thing->origin[VY], 0, P_Random() << 24,
                                   MSF_Z_FLOOR)))
            {
                mo->target = thing;
                mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
                mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
                mo->mom[MZ] = 1 + FIX2FLT(P_Random() << 8);
            }

            mo = P_SpawnMobjXYZ(MT_SLUDGESPLASH, thing->origin[VX],
                               thing->origin[VY], 0, P_Random() << 24,
                               MSF_Z_FLOOR);

            if(thing->player)
                P_NoiseAlert(thing, thing);
        }

        S_StartSound(SFX_SLUDGE_GLOOP, mo);
        return true;
    }

    return false;
}

void ResetBlasted(mobj_t* mo)
{
    mo->flags2 &= ~MF2_BLASTED;
    if(!(mo->flags & MF_ICECORPSE))
    {
        mo->flags2 &= ~MF2_SLIDE;
    }
}

void P_BlastMobj(mobj_t* source, mobj_t* victim, float strength)
{
    uint an;
    angle_t angle;
    mobj_t* mo;
    coord_t pos[3];

    angle = M_PointToAngle2(source->origin, victim->origin);
    an = angle >> ANGLETOFINESHIFT;
    if(strength < BLAST_FULLSTRENGTH)
    {
        victim->mom[MX] = strength * FIX2FLT(finecosine[an]);
        victim->mom[MY] = strength * FIX2FLT(finesine[an]);
        if(victim->player)
        {
            // Players handled automatically.
        }
        else
        {
            victim->flags2 |= MF2_SLIDE;
            victim->flags2 |= MF2_BLASTED;
        }
    }
    else // Full strength.
    {
        if(victim->flags & MF_MISSILE)
        {
            switch(victim->type)
            {
            case MT_SORCBALL1: // Don't blast sorcerer balls.
            case MT_SORCBALL2:
            case MT_SORCBALL3:
                return;
                break;

            case MT_MSTAFF_FX2: // Reflect to originator.
                victim->tracer = victim->target;
                victim->target = source;
                break;

            default:
                break;
            }
        }

        if(victim->type == MT_HOLY_FX)
        {
            if(victim->tracer == source)
            {
                victim->tracer = victim->target;
                victim->target = source;
            }
        }
        victim->mom[MX] = BLAST_SPEED * FIX2FLT(finecosine[an]);
        victim->mom[MY] = BLAST_SPEED * FIX2FLT(finesine[an]);

        // Spawn blast puff.
        angle = M_PointToAngle2(victim->origin, source->origin);
        an = angle >> ANGLETOFINESHIFT;

        pos[VX] = victim->origin[VX];
        pos[VY] = victim->origin[VY];
        pos[VZ] = victim->origin[VZ] - victim->floorClip + victim->height / 2;

        pos[VX] += (victim->radius + FIX2FLT(FRACUNIT)) * FIX2FLT(finecosine[an]);
        pos[VY] += (victim->radius + FIX2FLT(FRACUNIT)) * FIX2FLT(finesine[an]);

        if((mo = P_SpawnMobj(MT_BLASTEFFECT, pos, angle, 0)))
        {
            mo->mom[MX] = victim->mom[MX];
            mo->mom[MY] = victim->mom[MY];
        }

        if((victim->flags & MF_MISSILE))
        {
            victim->mom[MZ] = 8;
            if(mo)
                mo->mom[MZ] = victim->mom[MZ];
        }
        else
        {
            victim->mom[MZ] = 1000 / victim->info->mass;
        }

        if(victim->player)
        {
            // Players handled automatically
        }
        else
        {
            victim->flags2 |= MF2_SLIDE;
            victim->flags2 |= MF2_BLASTED;
        }
    }
}

typedef struct {
    coord_t maxDistance;
    mobj_t* source;
} radiusblastparams_t;

static int radiusBlast(thinker_t* th, void* context)
{
    radiusblastparams_t* params = (radiusblastparams_t*) context;
    mobj_t* mo = (mobj_t *) th;
    coord_t dist;

    if(mo == params->source || (mo->flags2 & MF2_BOSS) || (mo->flags3 & MF3_NOBLAST))
    {
        // Unaffected.
        return false; // Continue iteration.
    }

    if(mo->type == MT_POISONCLOUD || // poison cloud.
       mo->type == MT_HOLY_FX || // holy fx.
       (mo->flags & MF_ICECORPSE)) // frozen corpse.
    {
        // Let these special cases go.
    }
    else if((mo->flags & MF_COUNTKILL) && mo->health <= 0)
    {
        return false; // Continue iteration.
    }
    else if(!(mo->flags & MF_COUNTKILL) && !mo->player &&
            !(mo->flags & MF_MISSILE))
    {
        // Must be monster, player, or missile.
        return false; // Continue iteration.
    }

    // Is this mobj dormant?
    if(mo->flags2 & MF2_DORMANT)
        return false; // Continue iteration.

    // Is this an underground Wraith?
    if(mo->type == MT_WRAITHB && (mo->flags2 & MF2_DONTDRAW))
        return false; // Continue iteration.

    if(mo->type == MT_SPLASHBASE || mo->type == MT_SPLASH)
        return false; // Continue iteration.

    if(mo->type == MT_SERPENT || mo->type == MT_SERPENTLEADER)
        return false; // Continue iteration.

    // Within range?
    dist = M_ApproxDistance(params->source->origin[VX] - mo->origin[VX],
                            params->source->origin[VY] - mo->origin[VY]);
    if(dist <= params->maxDistance)
    {
        P_BlastMobj(params->source, mo, BLAST_FULLSTRENGTH);
    }

    return false; // Continue iteration.
}

/**
 * Blast all mobjs away.
 */
void P_BlastRadius(player_t* pl)
{
    mobj_t* pmo = pl->plr->mo;
    radiusblastparams_t params;

    S_StartSound(SFX_INVITEM_BLAST, pmo);
    P_NoiseAlert(pmo, pmo);

    params.source = pmo;
    params.maxDistance = BLAST_RADIUS_DIST;
    Thinker_Iterate(P_MobjThinker, radiusBlast, &params);
}

typedef struct {
    coord_t origin[2];
    coord_t maxDistance;
    dd_bool effective;
} radiusgiveparams_t;

static int radiusGiveArmor(thinker_t* th, void* context)
{
    radiusgiveparams_t* params = (radiusgiveparams_t*) context;
    mobj_t* mo = (mobj_t*) th;
    coord_t dist;

    if(!mo->player || mo->health <= 0)
        return false; // Continue iteration.

    // Within range?
    dist = M_ApproxDistance(params->origin[VX] - mo->origin[VX],
                            params->origin[VY] - mo->origin[VY]);
    if(dist <= params->maxDistance)
    {
        if((P_GiveArmorAlt(mo->player, ARMOR_ARMOR,  1)) ||
           (P_GiveArmorAlt(mo->player, ARMOR_SHIELD, 1)) ||
           (P_GiveArmorAlt(mo->player, ARMOR_HELMET, 1)) ||
           (P_GiveArmorAlt(mo->player, ARMOR_AMULET, 1)))
        {
            params->effective = true;
            S_StartSound(SFX_MYSTICINCANT, mo);
        }
    }

    return false; // Continue iteration.
}

static int radiusGiveBody(thinker_t* th, void* context)
{
    radiusgiveparams_t* params = (radiusgiveparams_t*) context;
    mobj_t* mo = (mobj_t*) th;
    coord_t dist;

    if(!mo->player || mo->health <= 0)
        return false; // Continue iteration.

    // Within range?
    dist = M_ApproxDistance(params->origin[VX] - mo->origin[VX],
                            params->origin[VY] - mo->origin[VY]);
    if(dist <= params->maxDistance)
    {
        int amount = 50 + (P_Random() % 50);

        if(P_GiveHealth(mo->player, amount))
        {
            params->effective = true;
            S_StartSound(SFX_MYSTICINCANT, mo);
        }
    }

    return false; // Continue iteration.
}

static int radiusGiveMana(thinker_t* th, void* context)
{
    radiusgiveparams_t* params = (radiusgiveparams_t*) context;
    mobj_t* mo = (mobj_t *) th;
    coord_t dist;

    if(!mo->player || mo->health <= 0)
        return false; // Continue iteration.

    // Within range?
    dist = M_ApproxDistance(params->origin[VX] - mo->origin[VX],
                            params->origin[VY] - mo->origin[VY]);
    if(dist <= params->maxDistance)
    {
        int amount = 50 + (P_Random() % 50);

        if(P_GiveAmmo(mo->player, AT_BLUEMANA, amount) ||
           P_GiveAmmo(mo->player, AT_GREENMANA, amount))
        {
            params->effective = true;
            S_StartSound(SFX_MYSTICINCANT, mo);
        }
    }

    return false; // Continue iteration.
}

/**
 * Do class specific effect for everyone in radius
 */
dd_bool P_HealRadius(player_t* player)
{
    mobj_t*             pmo = player->plr->mo;
    radiusgiveparams_t  params;

    params.effective = false;
    params.origin[VX] = pmo->origin[VX];
    params.origin[VY] = pmo->origin[VY];
    params.maxDistance = HEAL_RADIUS_DIST;

    switch(player->class_)
    {
    case PCLASS_FIGHTER:
        Thinker_Iterate(P_MobjThinker, radiusGiveArmor, &params);
        break;

    case PCLASS_CLERIC:
        Thinker_Iterate(P_MobjThinker, radiusGiveBody, &params);
        break;

    case PCLASS_MAGE:
        Thinker_Iterate(P_MobjThinker, radiusGiveMana, &params);
        break;

    default:
        break;
    }

    return params.effective;
}

/**
 * @return  @c true, if the missile is at a valid spawn point,
 *          otherwise explodes it and return @c false.
 */
dd_bool P_CheckMissileSpawn(mobj_t *mo)
{
    // Move a little forward so an angle can be computed if it
    // immediately explodes
    P_MobjUnlink(mo);
    mo->origin[VX] += mo->mom[MX] / 2;
    mo->origin[VY] += mo->mom[MY] / 2;
    mo->origin[VZ] += mo->mom[MZ] / 2;
    P_MobjLink(mo);

    if(!P_TryMoveXY(mo, mo->origin[VX], mo->origin[VY]))
    {
        P_ExplodeMissile(mo);
        return false;
    }

    return true;
}

mobj_t* P_SpawnMissile(mobjtype_t type, mobj_t* source, mobj_t* dest)
{
    uint an;
    coord_t z, dist, origdist;
    mobj_t* th;
    angle_t angle;
    float aim;

    // Destination is required for the missile; if missing, can't spawn.
    if(!dest) return NULL;

    switch(type)
    {
    case MT_MNTRFX1: // Minotaur swing attack missile
        z = source->origin[VZ] + 40;
        break;

    case MT_MNTRFX2: // Minotaur floor fire missile
        z = source->floorZ;
        break;

    case MT_CENTAUR_FX:
        z = source->origin[VZ] + 45;
        break;

    case MT_ICEGUY_FX:
        z = source->origin[VZ] + 40;
        break;

    case MT_HOLY_MISSILE:
        z = source->origin[VZ] + 40;
        break;

    default:
        z = source->origin[VZ] + 32;
        break;
    }
    z -= source->floorClip;

    angle = M_PointToAngle2(source->origin, dest->origin);
    if(dest->flags & MF_SHADOW)
    {
        // Invisible target
        angle += (P_Random() - P_Random()) << 21;
    }

    if(!(th = P_SpawnMobjXYZ(type, source->origin[VX], source->origin[VY], z, angle, 0)))
        return NULL;

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Originator
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    origdist = M_ApproxDistance(dest->origin[VX] - source->origin[VX],
                                dest->origin[VY] - source->origin[VY]);
    dist = origdist / th->info->speed;
    if(dist < 1)
        dist = 1;
    th->mom[MZ] = (dest->origin[VZ] - source->origin[VZ]) / dist;

    // Use a more three-dimensional method.
    aim =
        BANG2RAD(bamsAtan2
                 ((int) (dest->origin[VZ] - source->origin[VZ]), (int) origdist));

    th->mom[MX] *= cos(aim);
    th->mom[MY] *= cos(aim);
    th->mom[MZ] = sin(aim) * th->info->speed;

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
}

mobj_t *P_SpawnMissileAngle(mobjtype_t type, mobj_t *source, angle_t angle, coord_t momz)
{
    unsigned int an;
    coord_t pos[3], spawnZOff = 0;
    mobj_t* mo;

    memcpy(pos, source->origin, sizeof(pos));

    switch(type)
    {
    case MT_MNTRFX1: // Minotaur swing attack missile
        spawnZOff = 40;
        break;

    case MT_ICEGUY_FX2: // Secondary Projectiles of the Ice Guy
        spawnZOff = 3;
        break;

    case MT_MSTAFF_FX2:
        spawnZOff = 40;
        break;

    default:
        if(source->player)
        {
            if(!P_MobjIsCamera(source->player->plr->mo))
                spawnZOff = cfg.common.plrViewHeight - 9 +
                    source->player->plr->lookDir / 173;
        }
        else
        {
            spawnZOff = 32;
        }
        break;
    }

    if(type == MT_MNTRFX2) // Minotaur floor fire missile
    {
        mo = P_SpawnMobjXYZ(type, pos[VX], pos[VY], 0, angle, MSF_Z_FLOOR);
    }
    else
    {
        pos[VZ] += spawnZOff;
        pos[VZ] -= source->floorClip;
        mo = P_SpawnMobj(type, pos, angle, 0);
    }

    if(mo)
    {
        if(mo->info->seeSound)
            S_StartSound(mo->info->seeSound, mo);

        mo->target = source; // Originator
        an = angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
        mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);
        mo->mom[MZ] = momz;

        return (P_CheckMissileSpawn(mo) ? mo : NULL);
    }

    return NULL;
}

mobj_t* P_SpawnMissileAngleSpeed(mobjtype_t type, mobj_t* source, angle_t angle,
    coord_t momz, float speed)
{
    unsigned int an;
    coord_t z;
    mobj_t* mo;

    z = source->origin[VZ];
    z -= source->floorClip;
    mo = P_SpawnMobjXYZ(type, source->origin[VX], source->origin[VY], z, angle, 0);

    if(mo)
    {
        mo->target = source; // Originator
        an = angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = speed * FIX2FLT(finecosine[an]);
        mo->mom[MY] = speed * FIX2FLT(finesine[an]);
        mo->mom[MZ] = momz;

        return (P_CheckMissileSpawn(mo) ? mo : NULL);
    }

    return NULL;
}

/**
 * Tries to aim at a nearby monster.
 */
mobj_t *P_SpawnPlayerMissile(mobjtype_t type, mobj_t *source)
{
    uint an;
    angle_t angle;
    coord_t pos[3];
    float fangle = LOOKDIR2RAD(source->player->plr->lookDir);
    float movfac = 1, slope;
    dd_bool dontAim = cfg.common.noAutoAim;
    int spawnFlags = 0;
    mobj_t *missile;

    // Try to find a target
    angle = source->angle;
    slope = P_AimLineAttack(source, angle, 16 * 64);
    if(!lineTarget || dontAim)
    {
        angle += 1 << 26;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!lineTarget)
        {
            angle -= 2 << 26;
            slope = P_AimLineAttack(source, angle, 16 * 64);
        }

        if(!lineTarget || dontAim)
        {
            angle = source->angle;

            slope = sin(fangle) / 1.2;
            movfac = cos(fangle);
        }
    }

    memcpy(pos, source->origin, sizeof(pos));

    if(type == MT_LIGHTNING_FLOOR)
    {
        pos[VZ] = 0;
        slope = 0;
        spawnFlags |= MSF_Z_FLOOR;
    }
    else if(type == MT_LIGHTNING_CEILING)
    {
        pos[VZ] = 0;
        slope = 0;
        spawnFlags |= MSF_Z_CEIL;
    }
    else
    {
        if(!P_MobjIsCamera(source->player->plr->mo))
            pos[VZ] += cfg.common.plrViewHeight - 9 +
                (source->player->plr->lookDir / 173);
        pos[VZ] -= source->floorClip;
    }

    if(!(missile = P_SpawnMobj(type, pos, angle, spawnFlags)))
        return NULL;

    missile->target = source;
    an = angle >> ANGLETOFINESHIFT;
    missile->mom[MX] =
        movfac * missile->info->speed * FIX2FLT(finecosine[an]);
    missile->mom[MY] =
        movfac * missile->info->speed * FIX2FLT(finesine[an]);
    missile->mom[MZ] = missile->info->speed * slope;

    P_MobjUnlink(missile);
    if(missile->type == MT_MWAND_MISSILE ||
       missile->type == MT_CFLAME_MISSILE)
    {
        // Ultra-fast ripper spawning missile.
        missile->origin[VX] += missile->mom[MX] / 8;
        missile->origin[VY] += missile->mom[MY] / 8;
        missile->origin[VZ] += missile->mom[MZ] / 8;
    }
    else
    {
        // Normal missile.
        missile->origin[VX] += missile->mom[MX] / 2;
        missile->origin[VY] += missile->mom[MY] / 2;
        missile->origin[VZ] += missile->mom[MZ] / 2;
    }
    P_MobjLink(missile);

    if(!P_TryMoveXY(missile, missile->origin[VX], missile->origin[VY]))
    {
        // Exploded immediately
        P_ExplodeMissile(missile);
        return NULL;
    }

    return missile;
}

mobj_t* P_SPMAngle(mobjtype_t type, mobj_t* source, angle_t origAngle)
{
    uint an;
    angle_t angle;
    mobj_t* th;
    coord_t pos[3];
    float fangle = LOOKDIR2RAD(source->player->plr->lookDir);
    float slope, movfac = 1;
    dd_bool dontAim = cfg.common.noAutoAim;

    // See which target is to be aimed at.
    angle = origAngle;
    slope = P_AimLineAttack(source, angle, 16 * 64);
    if(!lineTarget || dontAim)
    {
        angle += 1 << 26;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!lineTarget)
        {
            angle -= 2 << 26;
            slope = P_AimLineAttack(source, angle, 16 * 64);
        }

        if(!lineTarget || dontAim)
        {
            angle = origAngle;

            slope = sin(fangle) / 1.2;
            movfac = cos(fangle);
        }
    }

    memcpy(pos, source->origin, sizeof(pos));
    if(!P_MobjIsCamera(source->player->plr->mo))
        pos[VZ] += cfg.common.plrViewHeight - 9 +
            (source->player->plr->lookDir / 173);
    pos[VZ] -= source->floorClip;

    if((th = P_SpawnMobj(type, pos, angle, 0)))
    {
        th->target = source;
        an = angle >> ANGLETOFINESHIFT;
        th->mom[MX] = movfac * th->info->speed * FIX2FLT(finecosine[an]);
        th->mom[MY] = movfac * th->info->speed * FIX2FLT(finesine[an]);
        th->mom[MZ] = th->info->speed * slope;

        if(P_CheckMissileSpawn(th))
            return th;
    }

    return NULL;
}

mobj_t* P_SPMAngleXYZ(mobjtype_t type, coord_t x, coord_t y, coord_t z,
    mobj_t* source, angle_t origAngle)
{
    uint an;
    mobj_t* th;
    angle_t angle;
    float slope, movfac = 1;
    float fangle = LOOKDIR2RAD(source->player->plr->lookDir);
    dd_bool dontAim = cfg.common.noAutoAim;

    // See which target is to be aimed at.
    angle = origAngle;
    slope = P_AimLineAttack(source, angle, 16 * 64);
    if(!lineTarget || dontAim)
    {
        angle += 1 << 26;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!lineTarget)
        {
            angle -= 2 << 26;
            slope = P_AimLineAttack(source, angle, 16 * 64);
        }

        if(!lineTarget || dontAim)
        {
            angle = origAngle;
            slope = sin(fangle) / 1.2;
            movfac = cos(fangle);
        }
    }

    if(!P_MobjIsCamera(source->player->plr->mo))
        z += cfg.common.plrViewHeight - 9 + (source->player->plr->lookDir / 173);
    z -= source->floorClip;

    if((th = P_SpawnMobjXYZ(type, x, y, z, angle, 0)))
    {
        th->target = source;
        an = angle >> ANGLETOFINESHIFT;
        th->mom[MX] = movfac * th->info->speed * FIX2FLT(finecosine[an]);
        th->mom[MY] = movfac * th->info->speed * FIX2FLT(finesine[an]);
        th->mom[MZ] = th->info->speed * slope;

        if(P_CheckMissileSpawn(th))
            return th;
    }

    return NULL;
}
