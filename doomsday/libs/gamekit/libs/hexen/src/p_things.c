/** @file p_things.c
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
#include "p_things.h"

#include "g_common.h"
#include "mobj.h"
#include "p_map.h"

static dd_bool ActivateThing(mobj_t *mobj);
static dd_bool DeactivateThing(mobj_t *mobj);

mobjtype_t TranslateThingType[] = {
    MT_MAPSPOT,                 // T_NONE
    MT_CENTAUR,                 // T_CENTAUR
    MT_CENTAURLEADER,           // T_CENTAURLEADER
    MT_DEMON,                   // T_DEMON
    MT_ETTIN,                   // T_ETTIN
    MT_FIREDEMON,               // T_FIREGARGOYLE
    MT_SERPENT,                 // T_WATERLURKER
    MT_SERPENTLEADER,           // T_WATERLURKERLEADER
    MT_WRAITH,                  // T_WRAITH
    MT_WRAITHB,                 // T_WRAITHBURIED
    MT_FIREBALL1,               // T_FIREBALL1
    MT_MANA1,                   // T_MANA1
    MT_MANA2,                   // T_MANA2
    MT_SPEEDBOOTS,              // T_ITEMBOOTS
    MT_ARTIEGG,                 // T_ITEMEGG
    MT_ARTIFLY,                 // T_ITEMFLIGHT
    MT_SUMMONMAULATOR,          // T_ITEMSUMMON
    MT_TELEPORTOTHER,           // T_ITEMTPORTOTHER
    MT_ARTITELEPORT,            // T_ITEMTELEPORT
    MT_BISHOP,                  // T_BISHOP
    MT_ICEGUY,                  // T_ICEGOLEM
    MT_BRIDGE,                  // T_BRIDGE
    MT_BOOSTARMOR,              // T_DRAGONSKINBRACERS
    MT_HEALINGBOTTLE,           // T_ITEMHEALTHPOTION
    MT_HEALTHFLASK,             // T_ITEMHEALTHFLASK
    MT_ARTISUPERHEAL,           // T_ITEMHEALTHFULL
    MT_BOOSTMANA,               // T_ITEMBOOSTMANA
    MT_FW_AXE,                  // T_FIGHTERAXE
    MT_FW_HAMMER,               // T_FIGHTERHAMMER
    MT_FW_SWORD1,               // T_FIGHTERSWORD1
    MT_FW_SWORD2,               // T_FIGHTERSWORD2
    MT_FW_SWORD3,               // T_FIGHTERSWORD3
    MT_CW_SERPSTAFF,            // T_CLERICSTAFF
    MT_CW_HOLY1,                // T_CLERICHOLY1
    MT_CW_HOLY2,                // T_CLERICHOLY2
    MT_CW_HOLY3,                // T_CLERICHOLY3
    MT_MW_CONE,                 // T_MAGESHARDS
    MT_MW_STAFF1,               // T_MAGESTAFF1
    MT_MW_STAFF2,               // T_MAGESTAFF2
    MT_MW_STAFF3,               // T_MAGESTAFF3
    MT_EGGFX,                   // T_MORPHBLAST
    MT_ROCK1,                   // T_ROCK1
    MT_ROCK2,                   // T_ROCK2
    MT_ROCK3,                   // T_ROCK3
    MT_DIRT1,                   // T_DIRT1
    MT_DIRT2,                   // T_DIRT2
    MT_DIRT3,                   // T_DIRT3
    MT_DIRT4,                   // T_DIRT4
    MT_DIRT5,                   // T_DIRT5
    MT_DIRT6,                   // T_DIRT6
    MT_ARROW,                   // T_ARROW
    MT_DART,                    // T_DART
    MT_POISONDART,              // T_POISONDART
    MT_RIPPERBALL,              // T_RIPPERBALL
    MT_SGSHARD1,                // T_STAINEDGLASS1
    MT_SGSHARD2,                // T_STAINEDGLASS2
    MT_SGSHARD3,                // T_STAINEDGLASS3
    MT_SGSHARD4,                // T_STAINEDGLASS4
    MT_SGSHARD5,                // T_STAINEDGLASS5
    MT_SGSHARD6,                // T_STAINEDGLASS6
    MT_SGSHARD7,                // T_STAINEDGLASS7
    MT_SGSHARD8,                // T_STAINEDGLASS8
    MT_SGSHARD9,                // T_STAINEDGLASS9
    MT_SGSHARD0,                // T_STAINEDGLASS0
    MT_PROJECTILE_BLADE,        // T_BLADE
    MT_ICESHARD,                // T_ICESHARD
    MT_FLAME_SMALL,             // T_FLAME_SMALL
    MT_FLAME_LARGE,             // T_FLAME_LARGE
    MT_ARMOR_1,                 // T_MESHARMOR
    MT_ARMOR_2,                 // T_FALCONSHIELD
    MT_ARMOR_3,                 // T_PLATINUMHELM
    MT_ARMOR_4,                 // T_AMULETOFWARDING
    MT_ARTIPOISONBAG,           // T_ITEMFLECHETTE
    MT_ARTITORCH,               // T_ITEMTORCH
    MT_BLASTRADIUS,             // T_ITEMREPULSION
    MT_MANA3,                   // T_MANA3
    MT_ARTIPUZZSKULL,           // T_PUZZSKULL
    MT_ARTIPUZZGEMBIG,          // T_PUZZGEMBIG
    MT_ARTIPUZZGEMRED,          // T_PUZZGEMRED
    MT_ARTIPUZZGEMGREEN1,       // T_PUZZGEMGREEN1
    MT_ARTIPUZZGEMGREEN2,       // T_PUZZGEMGREEN2
    MT_ARTIPUZZGEMBLUE1,        // T_PUZZGEMBLUE1
    MT_ARTIPUZZGEMBLUE2,        // T_PUZZGEMBLUE2
    MT_ARTIPUZZBOOK1,           // T_PUZZBOOK1
    MT_ARTIPUZZBOOK2,           // T_PUZZBOOK2
    MT_KEY1,                    // T_METALKEY
    MT_KEY2,                    // T_SMALLMETALKEY
    MT_KEY3,                    // T_AXEKEY
    MT_KEY4,                    // T_FIREKEY
    MT_KEY5,                    // T_GREENKEY
    MT_KEY6,                    // T_MACEKEY
    MT_KEY7,                    // T_SILVERKEY
    MT_KEY8,                    // T_RUSTYKEY
    MT_KEY9,                    // T_HORNKEY
    MT_KEYA,                    // T_SERPENTKEY
    MT_WATER_DRIP,              // T_WATERDRIP
    MT_FLAME_SMALL_TEMP,        // T_TEMPSMALLFLAME
    MT_FLAME_SMALL,             // T_PERMSMALLFLAME
    MT_FLAME_LARGE_TEMP,        // T_TEMPLARGEFLAME
    MT_FLAME_LARGE,             // T_PERMLARGEFLAME
    MT_DEMON_MASH,              // T_DEMON_MASH
    MT_DEMON2_MASH,             // T_DEMON2_MASH
    MT_ETTIN_MASH,              // T_ETTIN_MASH
    MT_CENTAUR_MASH,            // T_CENTAUR_MASH
    MT_THRUSTFLOOR_UP,          // T_THRUSTSPIKEUP
    MT_THRUSTFLOOR_DOWN,        // T_THRUSTSPIKEDOWN
    MT_WRAITHFX4,               // T_FLESH_DRIP1
    MT_WRAITHFX5,               // T_FLESH_DRIP2
    MT_WRAITHFX2                // T_SPARK_DRIP
};

dd_bool EV_ThingProjectile(byte* args, dd_bool gravity)
{
    uint an;
    int tid, searcher;
    angle_t angle;
    coord_t speed, vspeed;
    mobjtype_t moType;
    mobj_t* mobj, *newMobj;
    dd_bool success;

    success = false;
    searcher = -1;
    tid = args[0];
    moType = TranslateThingType[args[1]];
    if(gfw_Rule(noMonsters) && (MOBJINFO[moType].flags & MF_COUNTKILL))
    {
        // Don't spawn monsters if -nomonsters
        return false;
    }

    angle = (int) args[2] << 24;
    an = angle >> ANGLETOFINESHIFT;
    speed = FIX2FLT((int) args[3] << 13);
    vspeed = FIX2FLT((int) args[4] << 13);
    while((mobj = P_FindMobjFromTID(tid, &searcher)) != NULL)
    {
        if((newMobj = P_SpawnMobj(moType, mobj->origin, angle, 0)))
        {
            if(newMobj->info->seeSound)
                S_StartSound(newMobj->info->seeSound, newMobj);

            newMobj->target = mobj; // Originator
            newMobj->mom[MX] = speed * FIX2FLT(finecosine[an]);
            newMobj->mom[MY] = speed * FIX2FLT(finesine[an]);
            newMobj->mom[MZ] = vspeed;
            newMobj->flags2 |= MF2_DROPPED; // Don't respawn
            if(gravity == true)
            {
                newMobj->flags &= ~MF_NOGRAVITY;
                newMobj->flags2 |= MF2_LOGRAV;
            }

            if(P_CheckMissileSpawn(newMobj) == true)
            {
                success = true;
            }
        }
    }

    return success;
}

dd_bool EV_ThingSpawn(byte *args, dd_bool fog)
{
    int tid, searcher;
    angle_t angle;
    mobj_t *mobj, *newMobj, *fogMobj;
    mobjtype_t moType;
    dd_bool success;

    success = false;
    searcher = -1;
    tid = args[0];
    moType = TranslateThingType[args[1]];
    if(gfw_Rule(noMonsters) && (MOBJINFO[moType].flags & MF_COUNTKILL))
    {
        // Don't spawn monsters if -nomonsters
        return false;
    }

    angle = (int) args[2] << 24;
    while((mobj = P_FindMobjFromTID(tid, &searcher)) != NULL)
    {
        //z = mobj->origin[VZ];

        if((newMobj = P_SpawnMobj(moType, mobj->origin, angle, 0)))
        {
            if(P_TestMobjLocation(newMobj) == false)
            {   // Didn't fit
                P_MobjRemove(newMobj, true);
            }
            else
            {
                if(fog)
                {
                    if((fogMobj = P_SpawnMobjXYZ(MT_TFOG, mobj->origin[VX], mobj->origin[VY],
                                                          mobj->origin[VZ] + TELEFOGHEIGHT,
                                                          angle + ANG180, 0)))
                        S_StartSound(SFX_TELEPORT, fogMobj);
                }

                newMobj->flags2 |= MF2_DROPPED; // Don't respawn
                if(newMobj->flags2 & MF2_FLOATBOB)
                {
                    newMobj->special1 = FLT2FIX(newMobj->origin[VZ] - newMobj->floorZ);
                }

                success = true;
            }
        }
    }

    return success;
}

dd_bool EV_ThingActivate(int tid)
{
    mobj_t     *mobj;
    int         searcher;
    dd_bool     success;

    success = false;
    searcher = -1;
    while((mobj = P_FindMobjFromTID(tid, &searcher)) != NULL)
    {
        if(ActivateThing(mobj) == true)
        {
            success = true;
        }
    }
    return success;
}

dd_bool EV_ThingDeactivate(int tid)
{
    mobj_t      *mobj;
    int         searcher;
    dd_bool     success;

    success = false;
    searcher = -1;
    while((mobj = P_FindMobjFromTID(tid, &searcher)) != NULL)
    {
        if(DeactivateThing(mobj) == true)
        {
            success = true;
        }
    }
    return success;
}

dd_bool EV_ThingRemove(int tid)
{
    mobj_t      *mobj;
    int         searcher;
    dd_bool     success;

    success = false;
    searcher = -1;
    while((mobj = P_FindMobjFromTID(tid, &searcher)) != NULL)
    {
        if(mobj->type == MT_BRIDGE)
        {
            A_BridgeRemove(mobj);
            return true;
        }
        P_MobjRemove(mobj, false);
        success = true;
    }
    return success;
}

dd_bool EV_ThingDestroy(int tid)
{
    mobj_t      *mobj;
    int         searcher;
    dd_bool     success;

    success = false;
    searcher = -1;
    while((mobj = P_FindMobjFromTID(tid, &searcher)) != NULL)
    {
        if(mobj->flags & MF_SHOOTABLE)
        {
            P_DamageMobj(mobj, NULL, NULL, 10000, false);
            success = true;
        }
    }
    return success;
}

static dd_bool ActivateThing(mobj_t *mobj)
{
    if(mobj->flags & MF_COUNTKILL)
    {                           // Monster
        if(mobj->flags2 & MF2_DORMANT)
        {
            mobj->flags2 &= ~MF2_DORMANT;
            mobj->tics = 1;
            return true;
        }
        return false;
    }

    switch(mobj->type)
    {
    case MT_ZTWINEDTORCH:
    case MT_ZTWINEDTORCH_UNLIT:
        P_MobjChangeState(mobj, S_ZTWINEDTORCH_1);
        S_StartSound(SFX_IGNITE, mobj);
        break;

    case MT_ZWALLTORCH:
    case MT_ZWALLTORCH_UNLIT:
        P_MobjChangeState(mobj, S_ZWALLTORCH1);
        S_StartSound(SFX_IGNITE, mobj);
        break;

    case MT_ZGEMPEDESTAL:
        P_MobjChangeState(mobj, S_ZGEMPEDESTAL2);
        break;

    case MT_ZWINGEDSTATUENOSKULL:
        P_MobjChangeState(mobj, S_ZWINGEDSTATUENOSKULL2);
        break;

    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
        if(mobj->args[0] == 0)
        {
            S_StartSound(SFX_THRUSTSPIKE_LOWER, mobj);
            mobj->flags2 &= ~MF2_DONTDRAW;
            if(mobj->args[1])
                P_MobjChangeState(mobj, S_BTHRUSTRAISE1);
            else
                P_MobjChangeState(mobj, S_THRUSTRAISE1);
        }
        break;

    case MT_ZFIREBULL:
    case MT_ZFIREBULL_UNLIT:
        P_MobjChangeState(mobj, S_ZFIREBULL_BIRTH);
        S_StartSound(SFX_IGNITE, mobj);
        break;

    case MT_ZBELL:
        if(mobj->health > 0)
        {
            P_DamageMobj(mobj, NULL, NULL, 10, false); // 'ring' the bell
        }
        break;

    case MT_ZCAULDRON:
    case MT_ZCAULDRON_UNLIT:
        P_MobjChangeState(mobj, S_ZCAULDRON1);
        S_StartSound(SFX_IGNITE, mobj);
        break;

    case MT_FLAME_SMALL:
        S_StartSound(SFX_IGNITE, mobj);
        P_MobjChangeState(mobj, S_FLAME_SMALL1);
        break;

    case MT_FLAME_LARGE:
        S_StartSound(SFX_IGNITE, mobj);
        P_MobjChangeState(mobj, S_FLAME_LARGE1);
        break;

    case MT_BAT_SPAWNER:
        P_MobjChangeState(mobj, S_SPAWNBATS1);
        break;

    default:
        return false;
        break;
    }
    return true;
}

static dd_bool DeactivateThing(mobj_t *mobj)
{
    if(mobj->flags & MF_COUNTKILL)
    {                           // Monster
        if(!(mobj->flags2 & MF2_DORMANT))
        {
            mobj->flags2 |= MF2_DORMANT;
            mobj->tics = -1;
            return true;
        }
        return false;
    }

    switch(mobj->type)
    {
    case MT_ZTWINEDTORCH:
    case MT_ZTWINEDTORCH_UNLIT:
        P_MobjChangeState(mobj, S_ZTWINEDTORCH_UNLIT);
        break;

    case MT_ZWALLTORCH:
    case MT_ZWALLTORCH_UNLIT:
        P_MobjChangeState(mobj, S_ZWALLTORCH_U);
        break;

    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
        if(mobj->args[0] == 1)
        {
            S_StartSound(SFX_THRUSTSPIKE_RAISE, mobj);
            if(mobj->args[1])
                P_MobjChangeState(mobj, S_BTHRUSTLOWER);
            else
                P_MobjChangeState(mobj, S_THRUSTLOWER);
        }
        break;

    case MT_ZFIREBULL:
    case MT_ZFIREBULL_UNLIT:
        P_MobjChangeState(mobj, S_ZFIREBULL_DEATH);
        break;

    case MT_ZCAULDRON:
    case MT_ZCAULDRON_UNLIT:
        P_MobjChangeState(mobj, S_ZCAULDRON_U);
        break;

    case MT_FLAME_SMALL:
        P_MobjChangeState(mobj, S_FLAME_SDORM1);
        break;

    case MT_FLAME_LARGE:
        P_MobjChangeState(mobj, S_FLAME_LDORM1);
        break;

    case MT_BAT_SPAWNER:
        P_MobjChangeState(mobj, S_SPAWNBATS_OFF);
        break;

    default:
        return false;
        break;
    }
    return true;
}
