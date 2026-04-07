/** @file mobj.cpp  Common playsim map object (mobj) functionality.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 * @authors Copyright © 1993-1996 id Software, Inc.
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
#include "mobj.h"

#include <cmath>
#include <doomsday/world/mobjthinkerdata.h>
#include <de/string.h>
#include <de/legacy/mathutil.h>

#include "dmu_lib.h"
#include "g_defs.h"
#include "mapstatereader.h"
#include "mapstatewriter.h"
#include "p_actor.h"
#include "p_map.h"
#include "p_saveg.h"
#include "player.h"

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#define DROPOFFMOMENTUM_THRESHOLD (1.0 / 4)

/// Threshold for stopping walk animation.
#define STANDSPEED              (1.0 / 2) // FIX2FLT(0x8000)

coord_t Mobj_ThrustMulForFriction(coord_t friction)
{
    if(friction <= FRICTION_NORMAL)
        return 1; // Normal friction.

    if(friction > 1)
        return 0; // There's nothing to thrust from!

    // Decrease thrust exponentially when nearing friction == 1.0.
    // {c = -93.31092643, b = 208.0448223, a = -114.7338958}
    return (-114.7338958 * friction * friction + 208.0448223 * friction - 93.31092643);
}

coord_t Mobj_ThrustMul(const mobj_t *mo)
{
    coord_t mul = 1.0;

#if __JHEXEN__
    if(P_MobjFloorTerrain(mo)->flags & TTF_FRICTION_LOW)
    {
        mul /= 2;
    }
#else // !__JHEXEN__
    Sector *sec = Mobj_Sector(mo);

#if __JHERETIC__
    if(P_ToXSector(sec)->special == 15) // Friction_Low
    {
        mul /= 4;
        return mul; // XG friction ignored.
    }
#endif

    // Use a thrust multiplier based on the sector's friction.
    mul = Mobj_ThrustMulForFriction(XS_Friction(sec));

#endif

    return mul;
}

dd_bool Mobj_IsAirborne(const mobj_t *mo)
{
    return ((mo->flags2 & MF2_FLY) && !(mo->origin[VZ] <= mo->floorZ) && !mo->onMobj) != 0;
}

coord_t Mobj_Friction(const mobj_t *mo)
{
    if(Mobj_IsAirborne(mo))
    {
        // Airborne "friction".
        return FRICTION_FLY;
    }

#if __JHERETIC__
    if(P_ToXSector(Mobj_Sector(mo))->special == 15) // Low friction.
    {
        return FRICTION_LOW;
    }
#endif

#if __JHEXEN__
    const terraintype_t *tt = P_MobjFloorTerrain(mo);
    if(tt->flags & TTF_FRICTION_LOW)
    {
        return FRICTION_LOW;
    }
#endif

#if LIBCOMMON_HAVE_XG
    // Use the current sector's friction.
    return XS_Friction(Mobj_Sector(mo));
#else
    return FRICTION_NORMAL;
#endif
}

dd_bool Mobj_IsVoodooDoll(const mobj_t *mo)
{
    if(!mo) return false;
    return (mo->player && mo->player->plr->mo != mo);
}

void Mobj_XYMoveStopping(mobj_t *mo)
{
    DE_ASSERT(mo != 0);

    player_t *player = mo->player;

    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {
        // Debug option for no sliding at all.
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }

    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
    {
        // No friction for missiles.
        return;
    }

    if (mo->origin[VZ] > mo->floorZ && !mo->onMobj && !(mo->flags2 & MF2_FLY))
    {
        // No friction when falling.
        return;
    }

#ifndef __JHEXEN__
    if(cfg.slidingCorpses)
    {
        // $dropoff_fix: Add objects falling off ledges. Does not apply to players!
        if(((mo->flags & MF_CORPSE) || (mo->intFlags & MIF_FALLING)) && !mo->player)
        {
            // Do not stop sliding if halfway off a step with some momentum.
            if(!INRANGE_OF(mo->mom[MX], 0, DROPOFFMOMENTUM_THRESHOLD) ||
               !INRANGE_OF(mo->mom[MY], 0, DROPOFFMOMENTUM_THRESHOLD))
            {
                if(!FEQUAL(mo->floorZ, P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT)))
                    return;
            }
        }
    }
#endif

    bool isVoodooDoll = Mobj_IsVoodooDoll(mo);
    bool belowWalkStop = (INRANGE_OF(mo->mom[MX], 0, WALKSTOP_THRESHOLD) &&
                          INRANGE_OF(mo->mom[MY], 0, WALKSTOP_THRESHOLD));

    bool belowStandSpeed = false;
    bool isMovingPlayer  = false;
    if(player)
    {
        belowStandSpeed = (INRANGE_OF(mo->mom[MX], 0, STANDSPEED) &&
                           INRANGE_OF(mo->mom[MY], 0, STANDSPEED));
        isMovingPlayer = (NON_ZERO(player->plr->forwardMove) ||
                        NON_ZERO(player->plr->sideMove));
    }

    // Stop player walking animation (only real players).
    if(!isVoodooDoll && player && belowStandSpeed && !isMovingPlayer &&
        !IS_NETWORK_SERVER) // Netgame servers use logic elsewhere for player animation.
    {
        // If in a walking frame, stop moving.
        if(P_PlayerInWalkState(player))
        {
            P_MobjChangeState(player->plr->mo, statenum_t(PCLASS_INFO(player->class_)->normalState));
        }
    }

    // Apply friction.
    if(belowWalkStop && !isMovingPlayer)
    {
        // $voodoodolls: Do not zero mom for voodoo dolls!
        if(!isVoodooDoll)
        {
            // Momentum is below the walkstop threshold; stop it completely.
            mo->mom[MX] = mo->mom[MY] = 0;

            // $voodoodolls: Stop view bobbing if this isn't a voodoo doll.
            if(player) player->bob = 0;
        }
    }
    else
    {
        coord_t friction = Mobj_Friction(mo);
        mo->mom[MX] *= friction;
        mo->mom[MY] *= friction;
    }
}

dd_bool Mobj_IsPlayerClMobj(mobj_t *mo)
{
    if(IS_CLIENT)
    {
        for(int i = 0; i < MAXPLAYERS; i++)
        {
            if(ClPlayer_ClMobj(i) == mo)
            {
                return true;
            }
        }
    }
    return false;
}

uint32_t Mobj_PrivateID(const mobj_t *mob)
{
    if (!mob)
    {
        return 0;
    }
    if (const auto *td = THINKER_DATA_MAYBE(mob->thinker, ThinkerData))
    {
        return td->id();
    }
    return 0;
}

mobj_t *Mobj_FindByPrivateID(uint32_t privateId)
{
    if (privateId == 0)
    {
        return nullptr;
    }

    if (auto *thinkerData = ThinkerData::find(de::Id(privateId)))
    {
        if (auto *mobjData = de::maybeAs<MobjThinkerData>(thinkerData))
        {
            return mobjData->mobj();
        }
    }
    return nullptr;
}

dd_bool Mobj_IsPlayer(const mobj_t *mob)
{
    if(!mob) return false;
    return (mob->player != 0);
}

angle_t Mobj_AimAtPoint2(mobj_t *mob, coord_t const point[], dd_bool pointShadowed)
{
    DE_ASSERT(mob);
    return P_AimAtPoint2(mob->origin, point, pointShadowed);
}

angle_t Mobj_AimAtPoint(mobj_t *mob, coord_t const point[])
{
    return Mobj_AimAtPoint2(mob, point, false/* not shadowed*/);
}

angle_t Mobj_AimAtTarget(mobj_t *mob)
{
    DE_ASSERT(mob);
    if(const auto *target = mob->target)
    {
        return Mobj_AimAtPoint2(mob, target->origin, target->flags & MF_SHADOW);
    }
    return mob->angle;
}

dd_bool Mobj_LookForPlayers(mobj_t *mo, dd_bool allAround)
{
    const int playerCount = P_CountPlayersInGame();

    // Nobody to target?
    if(!playerCount) return false;

    const int from = mo->lastLook % MAXPLAYERS;
    const int to   = (from + MAXPLAYERS - 1) % MAXPLAYERS;

    int cand         = from;
    int tries        = 0;
    bool foundTarget = false;
    for(; cand != to; cand = (cand < (MAXPLAYERS - 1)? cand + 1 : 0))
    {
        player_t *player = players + cand;

        // Is player in the game?
        if(!player->plr->inGame) continue;

        mobj_t *plrmo = player->plr->mo;
        if(!plrmo) continue;

        // Do not target camera players.
        if(P_MobjIsCamera(plrmo)) continue;

        // Only look ahead a fixed number of times.
        if(tries++ == 2) break;

        // Do not target dead players.
        if(player->health <= 0) continue;

        // Within sight?
        if(!P_CheckSight(mo, plrmo)) continue;

        if(!allAround)
        {
            angle_t an = M_PointToAngle2(mo->origin, plrmo->origin);
            an -= mo->angle;

            if(an > ANG90 && an < ANG270)
            {
                // If real close, react anyway.
                coord_t dist = M_ApproxDistance(plrmo->origin[VX] - mo->origin[VX],
                                                plrmo->origin[VY] - mo->origin[VY]);
                // Behind us?
                if(dist > MELEERANGE) continue;
            }
        }

#if __JHERETIC__ || __JHEXEN__
        // If player is invisible we may not detect if too far or randomly.
        if(plrmo->flags & MF_SHADOW)
        {
            if((M_ApproxDistance(plrmo->origin[VX] - mo->origin[VX],
                                 plrmo->origin[VY] - mo->origin[VY]) > 2 * MELEERANGE) &&
               M_ApproxDistance(plrmo->mom[MX], plrmo->mom[MY]) < 5)
            {
                // Too far; can't detect.
                continue;
            }

            // Randomly overlook the player regardless.
            if(P_Random() < 225) continue;
        }
#endif

#if __JHEXEN__
        // Minotaurs do not target their master.
        if(mo->type == MT_MINOTAUR && mo->tracer &&
           mo->tracer->player == player) continue;
#endif

        // Found our quarry.
        mo->target = plrmo;
        foundTarget = true;
    }

    // Start looking from here next time.
    mo->lastLook = cand;
    return foundTarget;
}

/**
 * Determines if it is allowed to execute the action function of @a mo.
 * @return @c true, if allowed.
 */
static bool shouldCallAction(mobj_t *mobj)
{
    if (IS_CLIENT)
    {
        if (ClMobj_LocalActionsEnabled(mobj)) return true;
    }
    if (!(mobj->ddFlags & DDMF_REMOTE) ||  // only for local mobjs
        (mobj->flags3 & MF3_CLIENTACTION)) // action functions allowed?
    {
        return true;
    }
    return false;
}

static bool changeMobjState(mobj_t *mobj, statenum_t stateNum, bool doCallAction)
{
    DE_ASSERT(mobj != 0);

    // Skip zero-tic states -- call their action but then advance to the next.
    do
    {
        if (stateNum == S_NULL)
        {
            mobj->state = (state_t *) S_NULL;
            P_MobjRemove(mobj, false);
            return false;
        }

        Mobj_SetState(mobj, stateNum);
        mobj->turnTime = false; // $visangle-facetarget

        state_t *st = &STATES[stateNum];

        // Call the action function?
        if (doCallAction && st->action)
        {
            if (shouldCallAction(mobj))
            {
                // Custom parameters in the action function are passed to libdoomsday this way.
                P_SetCurrentActionState(stateNum);

                void (*action)(mobj_t *);
                action = de::function_cast<void (*)(mobj_t *)>(st->action);
                action(mobj);
            }
        }

        stateNum = statenum_t(st->nextState);
    } while (!mobj->tics);

    // Return false if an action function removed the mobj.
    return mobj->thinker.function != (thinkfunc_t) NOPFUNC;
}

dd_bool P_MobjChangeState(mobj_t *mobj, statenum_t stateNum)
{
    return changeMobjState(mobj, stateNum, true /*call action functions*/);
}

dd_bool P_MobjChangeStateNoAction(mobj_t *mobj, statenum_t stateNum)
{
    return changeMobjState(mobj, stateNum, false /*don't call action functions*/);
}

#if __JHEXEN__
# define MOBJ_SAVEVERSION 8
#elif __JHERETIC__
# define MOBJ_SAVEVERSION 10
#else
# define MOBJ_SAVEVERSION 10
#endif

void mobj_s::write(MapStateWriter *msw) const
{
    Writer1 *writer = msw->writer();
    const mobj_t *mo = this;

    // Version.
    // JHEXEN
    // 2: Added the 'translucency' byte.
    // 3: Added byte 'vistarget'
    // 4: Added long 'tracer'
    // 4: Added long 'lastenemy'
    // 5: Added flags3
    // 6: Floor material removed.
    //
    // JDOOM || JHERETIC || JDOOM64
    // 4: Added byte 'translucency'
    // 5: Added byte 'vistarget'
    // 5: Added tracer in jDoom
    // 5: Added dropoff fix in jHeretic
    // 5: Added long 'floorclip'
    // 6: Added proper respawn data
    // 6: Added flags 2 in jDoom
    // 6: Added damage
    // 7: Added generator in jHeretic
    // 7: Added flags3
    //
    // JDOOM
    // 9: Revised mapspot flag interpretation
    //
    // JHERETIC
    // 8: Added special3
    // 9: Revised mapspot flag interpretation
    //
    // JHEXEN
    // 7: Removed superfluous info ptr
    // 8: Added 'onMobj'
    Writer_WriteByte(writer, MOBJ_SAVEVERSION);

#if !__JHEXEN__
    // A version 2 features: archive number and target.
    Writer_WriteInt16(writer, msw->serialIdFor(mo));
    Writer_WriteInt16(writer, msw->serialIdFor(mo->target));

# if __JDOOM__ || __JDOOM64__
    // Ver 5 features: Save tracer (fixes Archvile, Revenant bug)
    Writer_WriteInt16(writer, msw->serialIdFor(mo->tracer));
# endif
#endif

    Writer_WriteInt16(writer, msw->serialIdFor(mo->onMobj));

    // Info for drawing: position.
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VX]));
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VY]));
    Writer_WriteInt32(writer, FLT2FIX(mo->origin[VZ]));

    //More drawing info: to determine current sprite.
    Writer_WriteInt32(writer, mo->angle); // Orientation.
    Writer_WriteInt32(writer, mo->sprite); // Used to find patch_t and flip value.
    Writer_WriteInt32(writer, mo->frame);

#if !__JHEXEN__
    // The closest interval over all contacted Sectors.
    Writer_WriteInt32(writer, FLT2FIX(mo->floorZ));
    Writer_WriteInt32(writer, FLT2FIX(mo->ceilingZ));
#endif

    // For movement checking.
    Writer_WriteInt32(writer, FLT2FIX(mo->radius));
    Writer_WriteInt32(writer, FLT2FIX(mo->height));

    // Momentums, used to update position.
    Writer_WriteInt32(writer, FLT2FIX(mo->mom[MX]));
    Writer_WriteInt32(writer, FLT2FIX(mo->mom[MY]));
    Writer_WriteInt32(writer, FLT2FIX(mo->mom[MZ]));

    // If == VALIDCOUNT, already checked.
    Writer_WriteInt32(writer, mo->valid);

    Writer_WriteInt32(writer, mo->type);
    Writer_WriteInt32(writer, mo->tics); // State tic counter.
    Writer_WriteInt32(writer, int(mo->state - STATES) /*PTR2INT(mo->state)*/);

#if __JHEXEN__
    Writer_WriteInt32(writer, mo->damage);
#endif

    Writer_WriteInt32(writer, mo->flags);
#if __JHEXEN__
    Writer_WriteInt32(writer, mo->flags2);
    Writer_WriteInt32(writer, mo->flags3);

    if(mo->type == MT_KORAX)
        Writer_WriteInt32(writer, 0); // Searching index.
    else
        Writer_WriteInt32(writer, mo->special1);

    switch(mo->type)
    {
    case MT_LIGHTNING_FLOOR:
    case MT_LIGHTNING_ZAP:
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(mo->flags & MF_CORPSE)
            Writer_WriteInt32(writer, 0);
        else
            Writer_WriteInt32(writer, msw->serialIdFor(INT2PTR(mobj_t, mo->special2)));
        break;

    default:
        Writer_WriteInt32(writer, mo->special2);
        break;
    }
#endif
    Writer_WriteInt32(writer, mo->health);

    // Movement direction, movement generation (zig-zagging).
    Writer_WriteInt32(writer, mo->moveDir); // 0-7
    Writer_WriteInt32(writer, mo->moveCount); // When 0, select a new dir.

#if __JHEXEN__
    if(mo->flags & MF_CORPSE)
        Writer_WriteInt32(writer, 0);
    else
        Writer_WriteInt32(writer, (int) msw->serialIdFor(mo->target));
#endif

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    Writer_WriteInt32(writer, mo->reactionTime);

    // If >0, the target will be chased no matter what (even if shot).
    Writer_WriteInt32(writer, mo->threshold);

    // Additional info record for player avatars only (only valid if type is MT_PLAYER).
    {
        const int playerNum = (mo->player? int(mo->player - players + 1) : 0);
        Writer_WriteInt32(writer, playerNum /*PTR2INT(mo->player)*/);
    }

    // Player number last looked for.
    Writer_WriteInt32(writer, mo->lastLook);

#if !__JHEXEN__
    // For nightmare/multiplayer respawn.
    Writer_WriteInt32(writer, FLT2FIX(mo->spawnSpot.origin[VX]));
    Writer_WriteInt32(writer, FLT2FIX(mo->spawnSpot.origin[VY]));
    Writer_WriteInt32(writer, FLT2FIX(mo->spawnSpot.origin[VZ]));
    Writer_WriteInt32(writer, mo->spawnSpot.angle);
    Writer_WriteInt32(writer, mo->spawnSpot.flags);

    Writer_WriteInt32(writer, mo->intFlags); // $dropoff_fix: internal flags.
    Writer_WriteInt32(writer, FLT2FIX(mo->dropOffZ)); // $dropoff_fix
    Writer_WriteInt32(writer, mo->gear); // Used in torque simulation.

    Writer_WriteInt32(writer, mo->damage);
    Writer_WriteInt32(writer, mo->flags2);
    Writer_WriteInt32(writer, mo->flags3);
# ifdef __JHERETIC__
    Writer_WriteInt32(writer, mo->special1);
    Writer_WriteInt32(writer, mo->special2);
    Writer_WriteInt32(writer, mo->special3);
# endif

    Writer_WriteByte(writer,  mo->translucency);
    Writer_WriteByte(writer,  (byte)(mo->visTarget + 1));
#endif

    Writer_WriteInt32(writer, FLT2FIX(mo->floorClip));
#if __JHEXEN__
    Writer_WriteInt32(writer, msw->serialIdFor(mo));
    Writer_WriteInt32(writer, mo->tid);
    Writer_WriteInt32(writer, mo->special);
    Writer_Write(writer,      mo->args, sizeof(mo->args));
    Writer_WriteByte(writer,  mo->translucency);
    Writer_WriteByte(writer,  (byte)(mo->visTarget + 1));

    switch(mo->type)
    {
    case MT_BISH_FX:
    case MT_HOLY_FX:
    case MT_DRAGON:
    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
    case MT_MINOTAUR:
    case MT_SORCFX1:
    case MT_MSTAFF_FX2:
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(mo->flags & MF_CORPSE)
            Writer_WriteInt32(writer, 0);
        else
            Writer_WriteInt32(writer, msw->serialIdFor(mo->tracer));
        break;

    default:
        DE_ASSERT(mo->tracer == NULL); /// @todo Tracer won't be saved correctly?
        Writer_WriteInt32(writer, PTR2INT(mo->tracer));
        break;
    }

    Writer_WriteInt32(writer, 0); //PTR2INT(mo->lastEnemy));
#elif __JHERETIC__
    Writer_WriteInt16(writer, msw->serialIdFor(mo->generator));
#endif
}

int mobj_s::read(MapStateReader *msr)
{
#define FF_FULLBRIGHT 0x8000 ///< Used to be a flag in thing->frame.
#define FF_FRAMEMASK  0x7fff

    Reader1 *reader = msr->reader();

    int ver = Reader_ReadByte(reader);

#if !__JHEXEN__
    if(ver >= 2) // Version 2 has mobj archive numbers.
    {
        msr->addMobjToThingArchive(this, Reader_ReadInt16(reader));
    }
#endif

#if !__JHEXEN__
    target = 0;
    if(ver >= 2)
    {
        target   = INT2PTR(mobj_t, Reader_ReadInt16(reader));
    }
#endif

#if __JDOOM__ || __JDOOM64__
    tracer = 0;
    if(ver >= 5)
    {
        tracer   = INT2PTR(mobj_t, Reader_ReadInt16(reader));
    }
#endif

    onMobj = 0;
#if __JHEXEN__
    if(ver >= 8)
#else
    if(ver >= 5)
#endif
    {
        onMobj   = INT2PTR(mobj_t, Reader_ReadInt16(reader));
    }

    origin[VX]   = FIX2FLT(Reader_ReadInt32(reader));
    origin[VY]   = FIX2FLT(Reader_ReadInt32(reader));
    origin[VZ]   = FIX2FLT(Reader_ReadInt32(reader));
    angle        = Reader_ReadInt32(reader);
    sprite       = Reader_ReadInt32(reader);

    frame        = Reader_ReadInt32(reader); // might be ORed with FF_FULLBRIGHT
    if(frame & FF_FULLBRIGHT)
        frame &= FF_FRAMEMASK; // not used anymore.

#if __JHEXEN__
    if(ver < 6)
    {
        /*floorflat =*/ Reader_ReadInt32(reader);
    }
#else
    floorZ       = FIX2FLT(Reader_ReadInt32(reader));
    ceilingZ     = FIX2FLT(Reader_ReadInt32(reader));
#endif

    radius       = FIX2FLT(Reader_ReadInt32(reader));
    height       = FIX2FLT(Reader_ReadInt32(reader));
    mom[MX]      = FIX2FLT(Reader_ReadInt32(reader));
    mom[MY]      = FIX2FLT(Reader_ReadInt32(reader));
    mom[MZ]      = FIX2FLT(Reader_ReadInt32(reader));
    valid        = Reader_ReadInt32(reader);
    type         = Reader_ReadInt32(reader);

#if __JHEXEN__
    if(ver < 7)
    {
        /*info   = (mobjinfo_t *)*/ Reader_ReadInt32(reader);
    }
#endif
    info = &MOBJINFO[type];

    if(info->flags2 & MF2_FLOATBOB)
        mom[MZ] = 0;

    if(info->flags & MF_SOLID)
        ddFlags |= DDMF_SOLID;
    if(info->flags2 & MF2_DONTDRAW)
        ddFlags |= DDMF_DONTDRAW;

    tics         = Reader_ReadInt32(reader);
    state        = INT2PTR(state_t, Reader_ReadInt32(reader));
#if __JHEXEN__
    damage       = Reader_ReadInt32(reader);
#endif
    flags        = Reader_ReadInt32(reader);
#if __JHEXEN__
    flags2       = Reader_ReadInt32(reader);
    if(ver >= 5)
    {
        flags3   = Reader_ReadInt32(reader);
    }
    special1     = Reader_ReadInt32(reader);
    special2     = Reader_ReadInt32(reader);
#endif
    health       = Reader_ReadInt32(reader);

#if __JHERETIC__
    if(ver < 8)
    {
        // Fix a bunch of kludges in the original Heretic.
        switch(type)
        {
        case MT_MACEFX1:
        case MT_MACEFX2:
        case MT_MACEFX3:
        case MT_HORNRODFX2:
        case MT_HEADFX3:
        case MT_WHIRLWIND:
        case MT_TELEGLITTER:
        case MT_TELEGLITTER2:
            special3 = health;
            if(type == MT_HORNRODFX2 && special3 > 16)
                special3 = 16;
            health = MOBJINFO[type].spawnHealth;
            break;

        default:
            break;
        }
    }
#endif

    moveDir      = Reader_ReadInt32(reader);
    moveCount    = Reader_ReadInt32(reader);
#if __JHEXEN__
    target       = INT2PTR(mobj_t, Reader_ReadInt32(reader));
#endif
    reactionTime = Reader_ReadInt32(reader);
    threshold    = Reader_ReadInt32(reader);
    player       = INT2PTR(player_t, Reader_ReadInt32(reader));
    lastLook     = Reader_ReadInt32(reader);

#if __JHEXEN__
    floorClip    = FIX2FLT(Reader_ReadInt32(reader));
    msr->addMobjToThingArchive(this, Reader_ReadInt32(reader));
    tid          = Reader_ReadInt32(reader);
#else
    // For nightmare respawn.
    if(ver >= 6)
    {
        spawnSpot.origin[VX] = FIX2FLT(Reader_ReadInt32(reader));
        spawnSpot.origin[VY] = FIX2FLT(Reader_ReadInt32(reader));
        spawnSpot.origin[VZ] = FIX2FLT(Reader_ReadInt32(reader));
        spawnSpot.angle      = Reader_ReadInt32(reader);
        if(ver < 10)
        {
            /*spawnSpot.type =*/ Reader_ReadInt32(reader);
        }
        spawnSpot.flags      = Reader_ReadInt32(reader);
    }
    else
    {
        spawnSpot.origin[VX] = (float) Reader_ReadInt16(reader);
        spawnSpot.origin[VY] = (float) Reader_ReadInt16(reader);
        spawnSpot.origin[VZ] = 0; // Initialize with "something".
        spawnSpot.angle      = (angle_t) (ANG45 * (Reader_ReadInt16(reader) / 45));
        /*spawnSpot.type       = (int)*/ Reader_ReadInt16(reader);
        spawnSpot.flags      = (int) Reader_ReadInt16(reader);
    }

# if __JDOOM__ || __JDOOM64__
    if(ver >= 3)
# elif __JHERETIC__
    if(ver >= 5)
# endif
    {
        intFlags = Reader_ReadInt32(reader);
        dropOffZ = FIX2FLT(Reader_ReadInt32(reader));
        gear     = Reader_ReadInt32(reader);
    }

# if __JDOOM__ || __JDOOM64__
    if(ver >= 6)
    {
        damage = Reader_ReadInt32(reader);
        flags2 = Reader_ReadInt32(reader);
    }
    else // flags2 will be applied from the defs.
    {
        damage = DDMAXINT; // Use the value set in mo->info->damage
    }

# elif __JHERETIC__
    damage = Reader_ReadInt32(reader);
    flags2 = Reader_ReadInt32(reader);
# endif

    if(ver >= 7)
    {
        flags3 = Reader_ReadInt32(reader);
    } // Else flags3 will be applied from the defs.
#endif

#if __JHEXEN__
    special = Reader_ReadInt32(reader);
    Reader_Read(reader, args, 1 * 5);
#elif __JHERETIC__
    special1 = Reader_ReadInt32(reader);
    special2 = Reader_ReadInt32(reader);
    if(ver >= 8)
    {
        special3 = Reader_ReadInt32(reader);
    }
#endif

#if __JHEXEN__
    if(ver >= 2)
#else
    if(ver >= 4)
#endif
    {
        translucency = Reader_ReadByte(reader);
    }

#if __JHEXEN__
    if(ver >= 3)
#else
    if(ver >= 5)
#endif
    {
        visTarget = (short) (Reader_ReadByte(reader)) -1;
    }

#if __JHEXEN__
    if(ver >= 4)
    {
        // This value has not been mangled properly.
        /*tracer = */INT2PTR(mobj_t, Reader_ReadInt32(reader));
        tracer = nullptr;
    }

    if(ver >= 4)
    {
        // This value has not been mangled properly.
        /*lastEnemy =*/ INT2PTR(mobj_t, Reader_ReadInt32(reader));
        lastEnemy = nullptr;
    }
#else
    if(ver >= 5)
    {
        floorClip = FIX2FLT(Reader_ReadInt32(reader));
    }
#endif

#if __JHERETIC__
    if(ver >= 7)
    {
        generator = INT2PTR(mobj_t, Reader_ReadInt16(reader));
    }
    else
    {
        generator = 0;
    }
#endif

    /*
     * Restore! (unmangle)
     */
    info = &MOBJINFO[type];

    Mobj_SetState(this, PTR2INT(state));
#if __JHEXEN__
    if(flags2 & MF2_DORMANT)
    {
        tics = -1;
    }
#endif

    if(player)
    {
        // The player number translation table is used to find out the
        // *current* (actual) player number of the referenced player.
        player = msr->player(PTR2INT(player));
#if __JHEXEN__
        if(!player)
        {
            // This saved player does not exist in the current game!
            // Destroy this mobj.
            Mobj_Destroy(this);
            return false;
        }
#endif
        dPlayer = player->plr;

        dPlayer->mo      = this;
        //dPlayer->clAngle = angle; /* $unifiedangles */
        dPlayer->lookDir = 0; /* $unifiedangles */
    }

    visAngle = angle >> 16;

#if !__JHEXEN__
    if(dPlayer && !dPlayer->inGame)
    {
        dPlayer->mo = 0;
        Mobj_Destroy(this);
        return false;
    }
#endif

#if !__JDOOM64__
    // Do we need to update this mobj's flag values?
    if(ver < MOBJ_SAVEVERSION)
    {
        SV_TranslateLegacyMobjFlags(this, ver);
    }
#endif

    P_MobjLink(this);
    floorZ   = P_GetDoublep(Mobj_Sector(this), DMU_FLOOR_HEIGHT);
    ceilingZ = P_GetDoublep(Mobj_Sector(this), DMU_CEILING_HEIGHT);

    return false;

#undef FF_FRAMEMASK
#undef FF_FULLBRIGHT
}

mobj_t *Mobj_ExplodeIfObstructed(mobj_t *mob)
{
    return P_CheckMissileSpawn(mob)? mob : nullptr;
}

mobj_t *P_LaunchMissile(mobj_t *missile, angle_t angle, coord_t const targetPos[],
    coord_t const sourcePos[], coord_t extraMomZ)
{
    DE_ASSERT(targetPos);
    if(missile)
    {
        DE_ASSERT(missile->info);

        // Play the launch sound.
        if(missile->info->seeSound)
        {
            S_StartSound(missile->info->seeSound, missile);
        }

        if(!sourcePos)
        {
            sourcePos = missile->origin;
        }

        // Determine speed.
        /// @todo Should optionally calculate this in true 3D.
        coord_t dist;
        uint an = angle >> ANGLETOFINESHIFT;
        missile->mom[0] = missile->info->speed * FIX2FLT(finecosine[an]);
        missile->mom[1] = missile->info->speed * FIX2FLT(finesine  [an]);

        dist = M_ApproxDistance(targetPos[0] - sourcePos[0], targetPos[1] - sourcePos[1]);
        dist /= missile->info->speed;
        if(dist < 1) dist = 1;

        missile->mom[2] = (targetPos[2] - sourcePos[2] + extraMomZ) / dist;
    }
    return Mobj_ExplodeIfObstructed(missile);
}

mobj_t *Mobj_LaunchMissileAtAngle2(mobj_t *mob, mobj_t *missile, angle_t angle,
    coord_t const targetPos[], coord_t const sourcePos[], coord_t extraMomZ)
{
    DE_ASSERT(mob);

    if(missile)
    {
        // Remember the source (i.e., us) for tracking kills, etc...
        missile->target = mob;
    }

    return P_LaunchMissile(missile, angle, targetPos, sourcePos, extraMomZ);
}

mobj_t *Mobj_LaunchMissileAtAngle(mobj_t *mob, mobj_t *missile, angle_t angle,
    coord_t const targetPos[], coord_t const sourcePos[])
{
    return Mobj_LaunchMissileAtAngle2(mob, missile, angle, targetPos, sourcePos,
                                      0/*no extra z-momentum*/);
}

mobj_t *Mobj_LaunchMissile2(mobj_t *mob, mobj_t *missile, coord_t const targetPos[],
    coord_t const sourcePos[], coord_t extraMomZ)
{
    DE_ASSERT(mob);
    return Mobj_LaunchMissileAtAngle2(mob, missile, missile? missile->angle : mob->angle,
                                      targetPos, sourcePos, extraMomZ);
}

mobj_t *Mobj_LaunchMissile(mobj_t *mob, mobj_t *missile, coord_t const targetPos[],
    coord_t const sourcePos[])
{
    return Mobj_LaunchMissile2(mob, missile, targetPos, sourcePos, 0/*no extra z-momentum*/);
}

void Mobj_InflictDamage(mobj_t *mob, const mobj_t *inflictor, int damage)
{
    DE_ASSERT(mob);

    // Do the damage.
    mob->health -= damage;

    // Notify the engine.
    THINKER_DATA(mob->thinker, MobjThinkerData).damageReceived(damage, inflictor);
}

de::String Mobj_StateAsInfo(const mobj_t *mob)
{
    using de::String;

    String str;

    str += "Mobj 0x";
    str += String::asText(Mobj_PrivateID(mob), 16);
    str += " {\n  target = 0x";
    str += String::asText(Mobj_PrivateID(mob->target), 16);
    str += "\n  onMobj = 0x";
    str += String::asText(Mobj_PrivateID(mob->onMobj), 16);
    str += "\n  tracer = 0x";
    str += String::asText(Mobj_PrivateID(mob->tracer), 16);

    #if defined (__JHERETIC__)
    {
        str += "\n  generator = 0x";
        str += String::asText(Mobj_PrivateID(mob->generator), 16);
    }
    #endif

    #if defined (__JHEXEN__)
    {
        str += "\n  lastEnemy = 0x";
        str += String::asText(Mobj_PrivateID(mob->lastEnemy), 16);
    }
    #endif

    str += "\n}\n";

    return str;
}

void Mobj_RestoreObjectState(mobj_t *mob, const de::Info::BlockElement &state)
{
    static de::String const KEY_TARGET     ("target");
    static de::String const KEY_ON_MOBJ    ("onMobj");
    static de::String const KEY_TRACER     ("tracer");
    static de::String const KEY_GENERATOR  ("generator");
    static de::String const KEY_LAST_ENEMY ("lastEnemy");

    bool ok;
    uint32_t pid;

    pid = state.keyValue(KEY_TARGET).text.toUInt32(&ok, 0);
    if (ok)
    {
        mob->target = Mobj_FindByPrivateID(pid);
        //qDebug() << "mobj" << mob->thinker.id << "target" << pid << mob->target;
    }

    pid = state.keyValue(KEY_ON_MOBJ).text.toUInt32(&ok, 0);
    if (ok)
    {
        mob->onMobj = Mobj_FindByPrivateID(pid);
        //qDebug() << "mobj" << mob->thinker.id << "onMobj" << pid << mob->onMobj;
    }

    pid = state.keyValue(KEY_TRACER).text.toUInt32(&ok, 0);
    if (ok)
    {
        mob->tracer = Mobj_FindByPrivateID(pid);
        //qDebug() << "mobj" << mob->thinker.id << "tracer" << pid << mob->tracer;
    }

    #if defined (__JHERETIC__)
    {
        pid = state.keyValue(KEY_GENERATOR).text.toUInt32(&ok, 0);
        if (ok)
        {
            mob->generator = Mobj_FindByPrivateID(pid);
            //qDebug() << "mobj" << mob->thinker.id << "generator" << pid << mob->generator;
        }
    }
    #endif

    #if defined (__JHEXEN__)
    {
        pid = state.keyValue(KEY_LAST_ENEMY).text.toUInt32(&ok, 0);
        if (ok)
        {
            mob->lastEnemy = Mobj_FindByPrivateID(pid);
            //qDebug() << "mobj" << mob->thinker.id << "lastEnemy" << pid << mob->lastEnemy;
        }
    }
    #endif
}

void Mobj_RunScriptOnDeath(mobj_t *mob, mobj_t *killer)
{
    using namespace de;

    if (IS_NETWORK_CLIENT)
    {
        // Clients don't do this, only the server will.
        return;
    }

    // Check Thing definition for an onDeath script.
    const auto &thingDef = DED_Definitions()->things[mob->type];
    if (const String onDeathSrc = thingDef.gets(DE_STR("onDeath")))
    {
        LOG_AS("Mobj_RunScriptOnDeath");

        const auto &self = THINKER_NS(mob->thinker);

        Record ns;
        ns.add(DE_STR("self")).set(new RecordValue(self));
        Variable &killerVar = ns.add(DE_STR("killer"));
        if (killer)
        {
            killerVar.set(new RecordValue(THINKER_NS(killer->thinker)));
        }
        else
        {
            killerVar.set(new NoneValue);
        }
        Process proc(&ns);
        Script script(onDeathSrc);
        proc.run(script);
        proc.execute();
    }
}

dd_bool Mobj_RunScriptOnTouch(mobj_t *mob, mobj_t *special, enum mobjtouchresult_e *result)
{
    using namespace de;

    if (IS_NETWORK_CLIENT)
    {
        // Clients don't do this, only the server will.
        return false;
    }

    // Check Thing definition for an onTouch script.
    const auto &thingDef = DED_Definitions()->things[special->type];
    if (const String onTouchSrc = thingDef.gets(DE_STR("onTouch")))
    {
        LOG_AS("Mobj_RunScriptOnTouch");

        const auto &self = THINKER_NS(special->thinker);

        // We will make it seem like the special thing has a script function
        // called "onTouch", even though we are only now parsing the script from
        // the DEDs. In the future, there might be an actual "onTouch" function in
        // some Thing class that gets called instead (in this case, the Thing
        // definition would be created in a script and not via DED).
        //
        // The function will perform any actions required for the touch interaction.
        // "self" points to the special thing, "toucher" is given as argument to
        // the function. The function returns a code that tells what to do with the
        // special item afterwards: "keep", "dormant", "hide", "destroy".

        // We are interested in the return value, so make it a function that can be called.
        String src = "def onTouch(toucher)\n";
        src += onTouchSrc;
        src += "\nend";

        Record ns;
        Script script(src);
        Process proc(&ns);
        proc.run(script);
        proc.execute();

        // Function is ready, call it now with the mobjs.
        ns.add(DE_STR("self")).set(new RecordValue(self));
        std::unique_ptr<Value> resultValue(
            Process::scriptCall(Process::TakeResult,
                                ns,
                                DE_STR("onTouch"),
                                &THINKER_DATA(mob->thinker, ThinkerData)));
        if (result)
        {
            *result = MTR_KEEP;
            if (resultValue && !is<NoneValue>(resultValue.get()))
            {
                const String val = resultValue->asText();
                if      (val == "keep")    *result = MTR_KEEP;
                else if (val == "dormant") *result = MTR_MAKE_DORMANT;
                else if (val == "hide")    *result = MTR_HIDE;
                else if (val == "destroy") *result = MTR_DESTROY;
                else
                {
                    LOG_SCR_ERROR("Invalid return value from function: \"%s\"")
                        << resultValue->asText();
                }
            }
        }
        return true;
    }
    return false;
}
