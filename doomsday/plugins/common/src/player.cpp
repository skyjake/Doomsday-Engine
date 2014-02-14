/** @file player.cpp  Common playsim routines relating to players.
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "player.h"

#include "d_netsv.h"
#include "d_net.h"
#include "dmu_lib.h"
#include "g_common.h"
#include "hu_log.h"
#if __JHERETIC__ || __JHEXEN__
#  include "hu_inventory.h"
#endif
#include "p_actor.h"
#include "p_inventory.h"
#include "p_map.h"
#include "p_saveg.h"
#include "p_start.h"
#include <de/memory.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#define MESSAGETICS             (4 * TICSPERSEC)
#define CAMERA_FRICTION_THRESHOLD (.4f)

struct weaponslotinfo_t
{
    uint num;
    weapontype_t *types;
};
static weaponslotinfo_t weaponSlots[NUM_WEAPON_SLOTS];

static byte slotForWeaponType(weapontype_t type, uint *position = 0)
{
    byte i = 0, found = 0;

    do
    {
        weaponslotinfo_t *slot = &weaponSlots[i];
        uint j = 0;

        while(!found && j < slot->num)
        {
            if(slot->types[j] == type)
            {
                found = i + 1;
                if(position)
                {
                    *position = j;
                }
            }
            else
            {
                j++;
            }
        }

    } while(!found && ++i < NUM_WEAPON_SLOTS);

    return found;
}

static void unlinkWeaponInSlot(byte slotidx, weapontype_t type)
{
    weaponslotinfo_t *slot = &weaponSlots[slotidx - 1];

    uint i;
    for(i = 0; i < slot->num; ++i)
    {
        if(slot->types[i] == type)
            break;
    }
    if(i == slot->num)
        return; // Not linked to this slot.

    memmove(&slot->types[i], &slot->types[i+1], sizeof(weapontype_t) * (slot->num - 1 - i));
    slot->types = (weapontype_t *)M_Realloc(slot->types, sizeof(weapontype_t) * --slot->num);
}

static void linkWeaponInSlot(byte slotidx, weapontype_t type)
{
    weaponslotinfo_t *slot = &weaponSlots[slotidx-1];

    slot->types = (weapontype_t *)M_Realloc(slot->types, sizeof(weapontype_t) * ++slot->num);
    if(slot->num > 1)
    {
        memmove(&slot->types[1], &slot->types[0],
                sizeof(weapontype_t) * (slot->num - 1));
    }

    slot->types[0] = type;
}

void P_InitWeaponSlots()
{
    de::zap(weaponSlots);
}

void P_FreeWeaponSlots()
{
    for(int i = 0; i < NUM_WEAPON_SLOTS; ++i)
    {
        weaponslotinfo_t *slot = &weaponSlots[i];

        M_Free(slot->types);
        slot->types = 0;
        slot->num   = 0;
    }
}

dd_bool P_SetWeaponSlot(weapontype_t type, byte slot)
{
    if(slot > NUM_WEAPON_SLOTS)
        return false;

    // First, remove the weapon (if found).
    byte currentSlot = slotForWeaponType(type);
    if(currentSlot)
    {
        unlinkWeaponInSlot(currentSlot, type);
    }

    if(slot != 0)
    {
        // Add this weapon to the specified slot (head).
        linkWeaponInSlot(slot, type);
    }

    return true;
}

byte P_GetWeaponSlot(weapontype_t type)
{
    if(type >= WT_FIRST && type < NUM_WEAPON_TYPES)
    {
        return slotForWeaponType(type);
    }
    return 0;
}

weapontype_t P_WeaponSlotCycle(weapontype_t type, dd_bool prev)
{
    if(type >= WT_FIRST && type < NUM_WEAPON_TYPES)
    {
        uint position;
        if(byte slotidx = slotForWeaponType(type, &position))
        {
            weaponslotinfo_t *slot = &weaponSlots[slotidx - 1];

            if(slot->num > 1)
            {
                if(prev)
                {
                    if(position == 0)
                        position = slot->num - 1;
                    else
                        position--;
                }
                else
                {
                    if(position == slot->num - 1)
                        position = 0;
                    else
                        position++;
                }

                return slot->types[position];
            }
        }
    }

    return type;
}

int P_IterateWeaponsBySlot(byte slot, dd_bool reverse, int (*callback) (weapontype_t, void *context),
    void *context)
{
    int result = 1;

    if(slot <= NUM_WEAPON_SLOTS)
    {
        uint i = 0;
        weaponslotinfo_t const *sl = &weaponSlots[slot];

        while(i < sl->num &&
             (result = callback(sl->types[reverse ? sl->num - 1 - i : i],
                                context)) != 0)
        {
            i++;
        }
    }

    return result;
}

#if __JHEXEN__
void P_InitPlayerClassInfo()
{
    PCLASS_INFO(PCLASS_FIGHTER)->niceName = GET_TXT(TXT_PLAYERCLASS1);
    PCLASS_INFO(PCLASS_CLERIC)->niceName = GET_TXT(TXT_PLAYERCLASS2);
    PCLASS_INFO(PCLASS_MAGE)->niceName = GET_TXT(TXT_PLAYERCLASS3);
    PCLASS_INFO(PCLASS_PIG)->niceName = GET_TXT(TXT_PLAYERCLASS4);
}
#endif

int P_GetPlayerNum(player_t const *player)
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(player == &players[i])
        {
            return i;
        }
    }
    return 0;
}

int P_GetPlayerCheats(player_t const *player)
{
    if(!player) return 0;

    if(player->plr->flags & DDPF_CAMERA)
    {
        return (player->cheats |
                (CF_GODMODE | cfg.cameraNoClip? CF_NOCLIP : 0));
    }
    return player->cheats;
}

int P_CountPlayersInGame()
{
    int count = 0;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *player = players + i;
        if(player->plr->inGame)
        {
            count += 1;
        }
    }
    return count;
}

dd_bool P_PlayerInWalkState(player_t *pl)
{
    if(!pl->plr->mo) return false;

    /// @todo  Implementation restricts possibilities for modifying behavior solely with state definitions.

#if __JDOOM__
    return pl->plr->mo->state - STATES - PCLASS_INFO(pl->class_)->runState < 4;
#endif

#if __JHERETIC__
    return pl->plr->mo->state - STATES - PCLASS_INFO(pl->class_)->runState < 4;
#endif

#if __JHEXEN__
    return ((unsigned) ((pl->plr->mo->state - STATES) - PCLASS_INFO(pl->class_)->runState) < 4);
#endif

#if __JDOOM64__
    return pl->plr->mo->state - STATES - PCLASS_INFO(pl->class_)->runState < 4;
#endif
}

void P_ShotAmmo(player_t *player)
{
    DENG_ASSERT(player != 0);

    weaponinfo_t *wInfo = &weaponInfo[player->readyWeapon][player->class_];

    if(IS_CLIENT) return; // Server keeps track of this.

    int fireMode = 0;
#if __JHERETIC__
    if(gameRules.deathmatch)
        fireMode = 0; // In deathmatch always use mode zero.
    else
        fireMode = (player->powers[PT_WEAPONLEVEL2]? 1 : 0);
#endif

    for(int i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        if(!wInfo->mode[fireMode].ammoType[i])
            continue; // Weapon does not take this ammo.

        // Don't let it fall below zero.
        player->ammo[i].owned = MAX_OF(0,
            player->ammo[i].owned - wInfo->mode[fireMode].perShot[i]);
    }
    player->update |= PSF_AMMO;
}

weapontype_t P_MaybeChangeWeapon(player_t *player, weapontype_t weapon, ammotype_t ammo, dd_bool force)
{
    if(IS_NETWORK_SERVER)
    {
        // This is done on clientside.
        NetSv_MaybeChangeWeapon(player - players, weapon, ammo, force);
        return WT_NOCHANGE;
    }

    App_Log(DE2_DEV_MAP_XVERBOSE,
            "P_MaybeChangeWeapon: plr %i, weapon %i, ammo %i, force %i",
            (int)(player - players), weapon, ammo, force);

    int pclass = player->class_;

    // Assume weapon power level zero.
    int lvl = 0;
#if __JHERETIC__
    if(player->powers[PT_WEAPONLEVEL2])
        lvl = 1;
#endif

    bool found = false;
    weapontype_t retVal = WT_NOCHANGE;

    if(weapon == WT_NOCHANGE && ammo == AT_NOAMMO) // Out of ammo.
    {
        bool good;

        // Note we have no auto-logical choice for a forced change.
        // Preferences are set by the user.
        for(int i = 0; i < NUM_WEAPON_TYPES && !found; ++i)
        {
            weapontype_t candidate = weapontype_t(cfg.weaponOrder[i]);
            weaponinfo_t *winf     = &weaponInfo[candidate][pclass];

            // Is candidate available in this game mode?
            if(!(winf->mode[lvl].gameModeBits & gameModeBits))
                continue;

            // Does the player actually own this candidate?
            if(!player->weapons[candidate].owned)
                continue;

            // Is there sufficent ammo for the candidate weapon?
            // Check amount for each used ammo type.
            good = true;
            for(int ammotype = 0; ammotype < NUM_AMMO_TYPES && good; ++ammotype)
            {
                if(!winf->mode[lvl].ammoType[ammotype])
                    continue; // Weapon does not take this type of ammo.

#if __JHERETIC__
                // Heretic always uses lvl 0 ammo requirements in deathmatch
                if(gameRules.deathmatch &&
                   player->ammo[ammotype].owned < winf->mode[0].perShot[ammotype])
                {
                    // Not enough ammo of this type. Candidate is NOT good.
                    good = false;
                }
                else
#endif
                if(player->ammo[ammotype].owned < winf->mode[lvl].perShot[ammotype])
                {
                    // Not enough ammo of this type. Candidate is NOT good.
                    good = false;
                }
            }

            if(good)
            {
                // Candidate weapon meets the criteria.
                retVal = candidate;
                found = true;
            }
        }
    }
    else if(weapon != WT_NOCHANGE) // Player was given a NEW weapon.
    {
        // A forced weapon change?
        if(force)
        {
            retVal = weapon;
        }
        // Is the player currently firing?
        else if(!((player->brain.attack) && cfg.noWeaponAutoSwitchIfFiring))
        {
            // Should we change weapon automatically?

            if(cfg.weaponAutoSwitch == 2) // Behavior: Always change
            {
                retVal = weapon;
            }
            else if(cfg.weaponAutoSwitch == 1) // Behavior: Change if better
            {
                // Iterate the weapon order array and see if a weapon change
                // should be made. Preferences are user selectable.
                for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
                {
                    weapontype_t candidate = weapontype_t(cfg.weaponOrder[i]);
                    weaponinfo_t *winf     = &weaponInfo[candidate][pclass];

                    // Is candidate available in this game mode?
                    if(!(winf->mode[lvl].gameModeBits & gameModeBits))
                        continue;

                    if(weapon == candidate)
                    {
                        // weapon has a higher priority than the readyweapon.
                        retVal = weapon;
                    }
                    else if(player->readyWeapon == candidate)
                    {
                        // readyweapon has a higher priority so don't change.
                        break;
                    }
                }
            }
        }
    }
    else if(ammo != AT_NOAMMO) // Player is about to be given some ammo.
    {
        if(force || (!(player->ammo[ammo].owned > 0) && cfg.ammoAutoSwitch != 0))
        {
            // We were down to zero, so select a new weapon.

            // Iterate the weapon order array and see if the player owns a weapon that can be used
            // now they have this ammo.
            // Preferences are user selectable.
            for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
            {
                weapontype_t candidate = weapontype_t(cfg.weaponOrder[i]);
                weaponinfo_t *winf     = &weaponInfo[candidate][pclass];

                // Is candidate available in this game mode?
                if(!(winf->mode[lvl].gameModeBits & gameModeBits))
                    continue;

                // Does the player actually own this candidate?
                if(!player->weapons[candidate].owned)
                    continue;

                // Does the weapon use this type of ammo?
                if(!(winf->mode[lvl].ammoType[ammo]))
                    continue;

                /**
                 * @todo Have we got enough of ALL used ammo types?
                 *
                 * Problem, since the ammo has not been given yet (could be an object that gives
                 * several ammo types eg backpack) we can't test for this with what we know!
                 *
                 * This routine should be called AFTER the new ammo has been given. Somewhat complex
                 * logic to decipher first...
                 */

                if(cfg.ammoAutoSwitch == 2) // Behavior: Always change
                {
                    retVal = candidate;
                    break;
                }
                else if(cfg.ammoAutoSwitch == 1 && player->readyWeapon == candidate)
                {
                    // readyweapon has a higher priority so don't change.
                    break;
                }
            }
        }
    }

    // Don't change to the existing weapon.
    if(retVal == player->readyWeapon)
        retVal = WT_NOCHANGE;

    // Choosen a weapon to change to?
    if(retVal != WT_NOCHANGE)
    {
        App_Log(DE2_DEV_MAP_XVERBOSE, "P_MaybeChangeWeapon: Player %i decided to change to weapon %i",
                (int)(player - players), retVal);

        player->pendingWeapon = retVal;

        if(IS_CLIENT)
        {
            // Tell the server.
            NetCl_PlayerActionRequest(player, GPA_CHANGE_WEAPON, player->pendingWeapon);
        }
    }

    return retVal;
}

dd_bool P_CheckAmmo(player_t *plr)
{
    DENG_ASSERT(plr != 0);

    weaponinfo_t *wInfo = &weaponInfo[plr->readyWeapon][plr->class_];

    int fireMode = 0;
#if __JHERETIC__
    // If deathmatch always use firemode two ammo requirements.
    if(plr->powers[PT_WEAPONLEVEL2] && !gameRules.deathmatch)
    {
        fireMode = 1;
    }
#endif

#if __JHEXEN__
    //// @todo Kludge: Work around the multiple firing modes problems.
    //// We need to split the weapon firing routines and implement them as
    //// new fire modes.
    if(plr->class_ == PCLASS_FIGHTER && plr->readyWeapon != WT_FOURTH)
    {
        return true;
    }
    // < KLUDGE
#endif

    // Check we have enough of ALL ammo types used by this weapon.
    bool good = true;
    for(int i = 0; i < NUM_AMMO_TYPES && good; ++i)
    {
        if(!wInfo->mode[fireMode].ammoType[i])
            continue; // Weapon does not take this type of ammo.

        // Minimal amount for one shot varies.
        if(plr->ammo[i].owned < wInfo->mode[fireMode].perShot[i])
        {
            // Insufficent ammo to fire this weapon.
            good = false;
        }
    }

    if(good) return true;

    // Out of ammo, pick a weapon to change to.
    P_MaybeChangeWeapon(plr, WT_NOCHANGE, AT_NOAMMO, false);

    // Now set appropriate weapon overlay.
    if(plr->pendingWeapon != WT_NOCHANGE)
    {
        P_SetPsprite(plr, ps_weapon, statenum_t(wInfo->mode[fireMode].states[WSN_DOWN]));
    }

    return false;
}

weapontype_t P_PlayerFindWeapon(player_t *player, dd_bool prev)
{
    DENG_ASSERT(player != 0);

#if __JDOOM__
    static weapontype_t wp_list[] = {
        WT_FIRST, WT_SECOND, WT_THIRD, WT_NINETH, WT_FOURTH,
        WT_FIFTH, WT_SIXTH, WT_SEVENTH, WT_EIGHTH
    };

#elif __JDOOM64__
    static weapontype_t wp_list[] = {
        WT_FIRST, WT_SECOND, WT_THIRD, WT_NINETH, WT_FOURTH,
        WT_FIFTH, WT_SIXTH, WT_SEVENTH, WT_EIGHTH, WT_TENTH
    };
#elif __JHERETIC__
    static weapontype_t wp_list[] = {
        WT_FIRST, WT_SECOND, WT_THIRD, WT_FOURTH, WT_FIFTH,
        WT_SIXTH, WT_SEVENTH, WT_EIGHTH
    };

#elif __JHEXEN__ || __JSTRIFE__
    static weapontype_t wp_list[] = {
        WT_FIRST, WT_SECOND, WT_THIRD, WT_FOURTH
    };
#endif

    int lvl = 0;
#if __JHERETIC__
    lvl = (player->powers[PT_WEAPONLEVEL2]? 1 : 0);
#endif

    // Are we using weapon order preferences for next/previous?
    weapontype_t *list;
    if(cfg.weaponNextMode)
    {
        list = (weapontype_t *) cfg.weaponOrder;
        prev = !prev; // Invert order.
    }
    else
    {
        list = wp_list;
    }


    // Find the current position in the weapon list.
    int i = 0;
    weapontype_t w = WT_FIRST;
    for(; i < NUM_WEAPON_TYPES; ++i)
    {
        w = list[i];
        if(!cfg.weaponCycleSequential || player->pendingWeapon == WT_NOCHANGE)
        {
            if(w == player->readyWeapon)
                break;
        }
        else if(w == player->pendingWeapon)
        {
            break;
        }
    }

    // Locate the next or previous weapon owned by the player.
    weapontype_t initial = w;
    for(;;)
    {
        // Move the iterator.
        if(prev)
            i--;
        else
            i++;

        if(i < 0)
            i = NUM_WEAPON_TYPES - 1;
        else if(i > NUM_WEAPON_TYPES - 1)
            i = 0;

        w = list[i];

        // Have we circled around?
        if(w == initial)
            break;

        // Available in this game mode? And a valid weapon?
        if((weaponInfo[w][player->class_].mode[lvl].
            gameModeBits & gameModeBits) && player->weapons[w].owned)
            break;
    }

    return w;
}

#if __JHEXEN__
void P_PlayerChangeClass(player_t *player, playerclass_t newClass)
{
    DENG_ASSERT(player != 0);

    if(newClass < PCLASS_FIRST || newClass >= NUM_PLAYER_CLASSES)
        return;

    // Don't change if morphed.
    if(player->morphTics) return;
    if(!PCLASS_INFO(newClass)->userSelectable) return;

    player->class_ = newClass;
    cfg.playerClass[player - players] = newClass;
    P_ClassForPlayerWhenRespawning(player - players, true /*clear change request*/);

    // Take away armor.
    for(int i = 0; i < NUMARMOR; ++i)
    {
        player->armorPoints[i] = 0;
    }
    player->update |= PSF_ARMOR_POINTS;

    P_PostMorphWeapon(player, WT_FIRST);

    if(player->plr->mo)
    {
        // Respawn the player and destroy the old mobj.
        mobj_t *oldMo = player->plr->mo;

        P_SpawnPlayer(player - players, newClass, oldMo->origin[VX],
                      oldMo->origin[VY], oldMo->origin[VZ], oldMo->angle, 0,
                      P_MobjIsCamera(oldMo), true);
        P_MobjRemove(oldMo, true);
    }
}
#endif

void P_SetMessage(player_t *pl, int flags, char const *msg)
{
    DENG_ASSERT(pl != 0);

    if(!msg || !msg[0]) return;

    ST_LogPost(pl - players, flags, msg);

    if(pl == &players[CONSOLEPLAYER])
    {
        App_Log(DE2_LOG_MAP | (cfg.echoMsg? DE2_LOG_NOTE : DE2_LOG_VERBOSE), "%s\n", msg);
    }

    // Servers are responsible for sending these messages to the clients.
    NetSv_SendMessage(pl - players, msg);
}

#if __JHEXEN__
void P_SetYellowMessage(player_t *pl, int flags, char const *msg)
{
#define YELLOW_FMT      "{r=1;g=0.7;b=0.3;}"
#define YELLOW_FMT_LEN  18

    if(!msg || !msg[0]) return;

    size_t len = strlen(msg);

    AutoStr *buf = AutoStr_NewStd();
    Str_Reserve(buf, YELLOW_FMT_LEN + len+1);
    Str_Set(buf, YELLOW_FMT);
    Str_Appendf(buf, "%s", msg);

    ST_LogPost(pl - players, flags, Str_Text(buf));

    if(pl == &players[CONSOLEPLAYER])
    {
        App_Log(DE2_LOG_MAP | (cfg.echoMsg? DE2_LOG_NOTE : DE2_LOG_VERBOSE), "%s\n", msg);
    }

    // Servers are responsible for sending these messages to the clients.
    /// @todo We shouldn't need to send the format string along with every
    /// important game message. Instead flag a bit in the packet and then
    /// reconstruct on the other end.
    NetSv_SendMessage(pl - players, Str_Text(buf));

#undef YELLOW_FMT_LEN
#undef YELLOW_FMT
}
#endif

void P_Thrust3D(player_t *player, angle_t angle, float lookdir, coord_t forwardMove, coord_t sideMove)
{
    angle_t pitch = LOOKDIR2DEG(lookdir) / 360 * ANGLE_MAX;
    angle_t sideangle = angle - ANG90;
    mobj_t *mo = player->plr->mo;
    coord_t zmul, mom[3];

    angle >>= ANGLETOFINESHIFT;
    pitch >>= ANGLETOFINESHIFT;
    mom[MX] = forwardMove * FIX2FLT(finecosine[angle]);
    mom[MY] = forwardMove * FIX2FLT(finesine[angle]);
    mom[MZ] = forwardMove * FIX2FLT(finesine[pitch]);

    zmul = FIX2FLT(finecosine[pitch]);
    mom[MX] *= zmul;
    mom[MY] *= zmul;

    sideangle >>= ANGLETOFINESHIFT;
    mom[MX] += sideMove * FIX2FLT(finecosine[sideangle]);
    mom[MY] += sideMove * FIX2FLT(finesine[sideangle]);

    mo->mom[MX] += mom[MX];
    mo->mom[MY] += mom[MY];
    mo->mom[MZ] += mom[MZ];
}

int P_CameraXYMovement(mobj_t *mo)
{
    if(!P_MobjIsCamera(mo))
        return false;

#if __JDOOM__ || __JDOOM64__
    if(mo->flags & MF_NOCLIP ||
       // This is a very rough check! Sometimes you get stuck in things.
       P_CheckPositionXYZ(mo, mo->origin[VX] + mo->mom[MX], mo->origin[VY] + mo->mom[MY], mo->origin[VZ]))
    {
#endif

        P_MobjUnlink(mo);
        mo->origin[VX] += mo->mom[MX];
        mo->origin[VY] += mo->mom[MY];
        P_MobjLink(mo);
        P_CheckPositionXY(mo, mo->origin[VX], mo->origin[VY]);
        mo->floorZ = tmFloorZ;
        mo->ceilingZ = tmCeilingZ;

#if __JDOOM__ || __JDOOM64__
    }
#endif

    // Friction.
    if(!INRANGE_OF(mo->player->brain.forwardMove, 0, CAMERA_FRICTION_THRESHOLD) ||
       !INRANGE_OF(mo->player->brain.sideMove, 0, CAMERA_FRICTION_THRESHOLD) ||
       !INRANGE_OF(mo->player->brain.upMove, 0, CAMERA_FRICTION_THRESHOLD))
    {
        // While moving; normal friction applies.
        mo->mom[MX] *= FRICTION_NORMAL;
        mo->mom[MY] *= FRICTION_NORMAL;
    }
    else
    {
        // Else lose momentum, quickly!.
        mo->mom[MX] *= FRICTION_HIGH;
        mo->mom[MY] *= FRICTION_HIGH;
    }

    return true;
}

int P_CameraZMovement(mobj_t *mo)
{
    if(!P_MobjIsCamera(mo))
        return false;

    mo->origin[VZ] += mo->mom[MZ];

    // Friction.
    if(!INRANGE_OF(mo->player->brain.forwardMove, 0, CAMERA_FRICTION_THRESHOLD) ||
       !INRANGE_OF(mo->player->brain.sideMove, 0, CAMERA_FRICTION_THRESHOLD) ||
       !INRANGE_OF(mo->player->brain.upMove, 0, CAMERA_FRICTION_THRESHOLD))
    {
        // While moving; normal friction applies.
        mo->mom[MZ] *= FRICTION_NORMAL;
    }
    else
    {
        // Lose momentum quickly!.
        mo->mom[MZ] *= FRICTION_HIGH;
    }

    return true;
}

void P_PlayerThinkCamera(player_t *player)
{
    DENG_ASSERT(player != 0);

    mobj_t *mo = player->plr->mo;
    if(!mo) return;

    // If this player is not a camera, get out of here.
    if(!(player->plr->flags & DDPF_CAMERA))
    {
        if(player->playerState == PST_LIVE)
        {
            mo->flags |= (MF_SOLID | MF_SHOOTABLE | MF_PICKUP);
        }
        return;
    }

    mo->flags &= ~(MF_SOLID | MF_SHOOTABLE | MF_PICKUP);

    // How about viewlock?
    if(player->viewLock)
    {
        mobj_t *target = players->viewLock;

        if(!target->player || !target->player->plr->inGame)
        {
            player->viewLock = 0;
            return;
        }

        int full = player->lockFull;

        //player->plr->flags |= DDPF_FIXANGLES;
        /* $unifiedangles */
        mo->angle = M_PointToAngle2(mo->origin, target->origin);
        //player->plr->clAngle = mo->angle;
        player->plr->flags |= DDPF_INTERYAW;

        if(full)
        {
            coord_t dist = M_ApproxDistance(mo->origin[VX] - target->origin[VX],
                                            mo->origin[VY] - target->origin[VY]);
            angle_t angle = M_PointXYToAngle2(0, 0,
                                              target->origin[VZ] + (target->height / 2) - mo->origin[VZ],
                                              dist);

            player->plr->lookDir = -(angle / (float) ANGLE_MAX * 360.0f - 90);
            if(player->plr->lookDir > 180)
                player->plr->lookDir -= 360;

            player->plr->lookDir *= 110.0f / 85.0f;

            if(player->plr->lookDir > 110)
                player->plr->lookDir = 110;
            if(player->plr->lookDir < -110)
                player->plr->lookDir = -110;

            player->plr->flags |= DDPF_INTERPITCH;
        }
    }
}

D_CMD(SetCamera)
{
    int p = atoi(argv[1]);
    if(p < 0 || p >= MAXPLAYERS)
    {
        App_Log(DE2_LOG_SCR| DE2_LOG_ERROR, "Invalid console number %i\n", p);
        return false;
    }

    player_t *player = &players[p];

    player->plr->flags ^= DDPF_CAMERA;
    if(player->plr->inGame)
    {
        if(player->plr->flags & DDPF_CAMERA)
        {
            // Is now a camera.
            if(player->plr->mo)
            {
                player->plr->mo->origin[VZ] += player->viewHeight;
            }
        }
        else
        {
            // Is now a "real" player.
            if(player->plr->mo)
            {
                player->plr->mo->origin[VZ] -= player->viewHeight;
            }
        }
    }

    return true;
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
int P_PlayerGiveArmorBonus(player_t *plr, int points)
#else // __JHEXEN__
int P_PlayerGiveArmorBonus(player_t *plr, armortype_t type, int points)
#endif
{
    if(!points) return 0;

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    int *current = &plr->armorPoints;
#else // __JHEXEN__
    int *current = &plr->armorPoints[type];
#endif

    int oldPoints = *current;
    int delta;
    if(points > 0)
    {
        delta = points; /// @todo No upper limit?
    }
    else
    {
        if(*current + points < 0)
            delta = -(*current);
        else
            delta = points;
    }

    *current += delta;
    if(*current != oldPoints)
    {
        plr->update |= PSF_ARMOR_POINTS;
    }

    return delta;
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void P_PlayerSetArmorType(player_t *plr, int type)
{
    int oldType = plr->armorType;

    plr->armorType = type;
    if(plr->armorType != oldType)
    {
        plr->update |= PSF_ARMOR_TYPE;
    }
}
#endif

D_CMD(SetViewMode)
{
    if(argc > 2) return false;

    int pl = CONSOLEPLAYER;
    if(argc == 2)
    {
        pl = atoi(argv[1]);
    }
    if(pl < 0 || pl >= MAXPLAYERS)
        return false;

    if(!(players[pl].plr->flags & DDPF_CHASECAM))
    {
        players[pl].plr->flags |= DDPF_CHASECAM;
    }
    else
    {
        players[pl].plr->flags &= ~DDPF_CHASECAM;
    }

    return true;
}

D_CMD(SetViewLock)
{
    int pl = CONSOLEPLAYER, lock;

    if(!stricmp(argv[0], "lockmode"))
    {
        lock = atoi(argv[1]);
        if(lock)
            players[pl].lockFull = true;
        else
            players[pl].lockFull = false;

        return true;
    }
    if(argc < 2)
        return false;

    if(argc >= 3)
        pl = atoi(argv[2]); // Console number.

    lock = atoi(argv[1]);

    if(!(lock == pl || lock < 0 || lock >= MAXPLAYERS))
    {
        if(players[lock].plr->inGame && players[lock].plr->mo)
        {
            players[pl].viewLock = players[lock].plr->mo;
            return true;
        }
    }

    players[pl].viewLock = 0;

    return false;
}

D_CMD(MakeLocal)
{
    if(G_GameState() != GS_MAP)
    {
        App_Log(DE2_LOG_ERROR | DE2_LOG_MAP, "You must be in a game to create a local player.\n");
        return false;
    }

    int p = atoi(argv[1]);
    if(p < 0 || p >= MAXPLAYERS)
    {
        App_Log(DE2_SCR_ERROR, "Invalid console number %i.\n", p);
        return false;
    }

    player_t *plr = &players[p];
    if(plr->plr->inGame)
    {
        App_Log(DE2_LOG_ERROR | DE2_LOG_MAP, "Player %i is already in the game.\n", p);
        return false;
    }

    plr->playerState = PST_REBORN;
    plr->plr->inGame = true;

    char buf[20];
    sprintf(buf, "conlocp %i", p);
    DD_Execute(false, buf);

    P_DealPlayerStarts(0);

    return true;
}

D_CMD(PrintPlayerCoords)
{
    mobj_t *mo;

    if(G_GameState() != GS_MAP)
        return false;

    if(!(mo = players[CONSOLEPLAYER].plr->mo))
        return false;

    App_Log(DE2_LOG_MAP, "Console %i: X=%g Y=%g Z=%g\n", CONSOLEPLAYER,
                         mo->origin[VX], mo->origin[VY], mo->origin[VZ]);

    return true;
}

D_CMD(CycleSpy)
{
    //// @todo The engine should do this.
    App_Log(DE2_LOG_MAP | DE2_LOG_ERROR, "Spying not allowed.\n");
#if 0
    if(G_GameState() == GS_MAP && !deathmatch)
    {
        // Cycle the display player.
        do
        {
            Set(DD_DISPLAYPLAYER, DISPLAYPLAYER + 1);
            if(DISPLAYPLAYER == MAXPLAYERS)
            {
                Set(DD_DISPLAYPLAYER, 0);
            }
        } while(!players[DISPLAYPLAYER].plr->inGame &&
                DISPLAYPLAYER != CONSOLEPLAYER);
    }
#endif
    return true;
}

D_CMD(SpawnMobj)
{
    if(argc != 5 && argc != 6)
    {
        App_Log(DE2_SCR_NOTE, "Usage: %s (type) (x) (y) (z) (angle)\n", argv[0]);
        App_Log(DE2_LOG_SCR, "Type must be a defined Thing ID or Name.\n");
        App_Log(DE2_LOG_SCR, "Z is an offset from the floor, 'floor', 'ceil' or 'random'.\n");
        App_Log(DE2_LOG_SCR, "Angle (0..360) is optional.\n");
        return true;
    }

    if(IS_CLIENT)
    {
        App_Log(DE2_SCR_ERROR, "%s can't be used by clients\n", argv[0]);
        return false;
    }

    // First try to find the thing by ID.
    mobjtype_t type;
    if((type = mobjtype_t(Def_Get(DD_DEF_MOBJ, argv[1], 0))) < 0)
    {
        // Try to find it by name instead.
        if((type = mobjtype_t(Def_Get(DD_DEF_MOBJ_BY_NAME, argv[1], 0))) < 0)
        {
            App_Log(DE2_LOG_RES | DE2_LOG_ERROR, "Undefined thing type %s\n", argv[1]);
            return false;
        }
    }

    // The coordinates.
    coord_t pos[3];
    pos[VX] = strtod(argv[2], 0);
    pos[VY] = strtod(argv[3], 0);
    pos[VZ] = 0;

    int spawnFlags = 0;
    if(!stricmp(argv[4], "ceil"))
    {
        spawnFlags |= MSF_Z_CEIL;
    }
    else if(!stricmp(argv[4], "random"))
    {
        spawnFlags |= MSF_Z_RANDOM;
    }
    else
    {
        spawnFlags |= MSF_Z_FLOOR;
        if(stricmp(argv[4], "floor"))
        {
            pos[VZ] = strtod(argv[4], 0);
        }
    }

    angle_t angle = 0;
    if(argc == 6)
    {
        angle = ((int) (strtod(argv[5], 0) / 360 * FRACUNIT)) << 16;
    }

    if(mobj_t *mo = P_SpawnMobj(type, pos, angle, spawnFlags))
    {
#if __JDOOM64__
        // jd64 > kaiser - another cheesy hack!!!
        if(mo->type == MT_DART)
        {
            S_StartSound(SFX_SKESWG, mo); // We got darts! spawn skeswg sound!
        }
        else
        {
            S_StartSound(SFX_ITMBK, mo); // If not dart, then spawn itmbk sound
            mo->translucency = 255;
            mo->spawnFadeTics = 0;
            mo->intFlags |= MIF_FADE;
        }
    // << d64tc
#endif
    }

    return true;
}

angle_t Player_ViewYawAngle(int playerNum)
{
    if(playerNum < 0 || playerNum >= MAXPLAYERS)
    {
        return 0;
    }

    ddplayer_t *plr = players[playerNum].plr;
    angle_t ang = plr->mo->angle + (int) (ANGLE_MAX * -G_GetLookOffset(playerNum));

    if(Get(DD_USING_HEAD_TRACKING))
    {
        // The actual head yaw angle will be used for rendering.
        ang -= plr->appliedBodyYaw;
    }

    return ang;
}

void player_s::write(Writer *writer, playerheader_t &plrHdr) const
{
#if __JDOOM64__ || __JHERETIC__ || __JHEXEN__
    int const plrnum = P_GetPlayerNum(this);
#endif

    player_t temp, *p = &temp;
    ddplayer_t ddtemp, *dp = &ddtemp;

    // Make a copy of the player.
    std::memcpy(p, this, sizeof(temp));
    std::memcpy(dp, plr, sizeof(ddtemp));
    temp.plr = &ddtemp;

    // Convert the psprite states.
    for(int i = 0; i < plrHdr.numPSprites; ++i)
    {
        pspdef_t *pspDef = &temp.pSprites[i];

        if(pspDef->state)
        {
            pspDef->state = (state_t *) (pspDef->state - STATES);
        }
    }

    // Version number. Increase when you make changes to the player data
    // segment format.
    Writer_WriteByte(writer, 6);

#if __JHEXEN__
    // Class.
    Writer_WriteByte(writer, cfg.playerClass[plrnum]);
#endif

    Writer_WriteInt32(writer, p->playerState);
#if __JHEXEN__
    Writer_WriteInt32(writer, p->class_);    // 2nd class...?
#endif
    Writer_WriteInt32(writer, FLT2FIX(p->viewZ));
    Writer_WriteInt32(writer, FLT2FIX(p->viewHeight));
    Writer_WriteInt32(writer, FLT2FIX(p->viewHeightDelta));
#if !__JHEXEN__
    Writer_WriteFloat(writer, dp->lookDir);
#endif
    Writer_WriteInt32(writer, FLT2FIX(p->bob));
#if __JHEXEN__
    Writer_WriteInt32(writer, p->flyHeight);
    Writer_WriteFloat(writer, dp->lookDir);
    Writer_WriteInt32(writer, p->centering);
#endif
    Writer_WriteInt32(writer, p->health);

#if __JHEXEN__
    for(int i = 0; i < plrHdr.numArmorTypes; ++i)
    {
        Writer_WriteInt32(writer, p->armorPoints[i]);
    }
#else
    Writer_WriteInt32(writer, p->armorPoints);
    Writer_WriteInt32(writer, p->armorType);
#endif

#if __JDOOM64__ || __JHEXEN__
    for(int i = 0; i < plrHdr.numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRST + i);

        Writer_WriteInt32(writer, type);
        Writer_WriteInt32(writer, P_InventoryCount(plrnum, type));
    }
    Writer_WriteInt32(writer, P_InventoryReadyItem(plrnum));
#endif

    for(int i = 0; i < plrHdr.numPowers; ++i)
    {
        Writer_WriteInt32(writer, p->powers[i]);
    }

#if __JHEXEN__
    Writer_WriteInt32(writer, p->keys);
#else
    for(int i = 0; i < plrHdr.numKeys; ++i)
    {
        Writer_WriteInt32(writer, p->keys[i]);
    }
#endif

#if __JHEXEN__
    Writer_WriteInt32(writer, p->pieces);
#else
    Writer_WriteInt32(writer, p->backpack);
#endif

    for(int i = 0; i < plrHdr.numFrags; ++i)
    {
        Writer_WriteInt32(writer, p->frags[i]);
    }

    Writer_WriteInt32(writer, p->readyWeapon);
    Writer_WriteInt32(writer, p->pendingWeapon);

    for(int i = 0; i < plrHdr.numWeapons; ++i)
    {
        Writer_WriteInt32(writer, p->weapons[i].owned);
    }

    for(int i = 0; i < plrHdr.numAmmoTypes; ++i)
    {
        Writer_WriteInt32(writer, p->ammo[i].owned);
#if !__JHEXEN__
        Writer_WriteInt32(writer, p->ammo[i].max);
#endif
    }

    Writer_WriteInt32(writer, p->attackDown);
    Writer_WriteInt32(writer, p->useDown);

    Writer_WriteInt32(writer, p->cheats);

    Writer_WriteInt32(writer, p->refire);

    Writer_WriteInt32(writer, p->killCount);
    Writer_WriteInt32(writer, p->itemCount);
    Writer_WriteInt32(writer, p->secretCount);

    Writer_WriteInt32(writer, p->damageCount);
    Writer_WriteInt32(writer, p->bonusCount);
#if __JHEXEN__
    Writer_WriteInt32(writer, p->poisonCount);
#endif

    Writer_WriteInt32(writer, dp->extraLight);
    Writer_WriteInt32(writer, dp->fixedColorMap);
    Writer_WriteInt32(writer, p->colorMap);

    for(int i = 0; i < plrHdr.numPSprites; ++i)
    {
        pspdef_t *psp = &p->pSprites[i];

        Writer_WriteInt32(writer, PTR2INT(psp->state));
        Writer_WriteInt32(writer, psp->tics);
        Writer_WriteInt32(writer, FLT2FIX(psp->pos[VX]));
        Writer_WriteInt32(writer, FLT2FIX(psp->pos[VY]));
    }

#if !__JHEXEN__
    Writer_WriteInt32(writer, p->didSecret);

    // Added in ver 2 with __JDOOM__
    Writer_WriteInt32(writer, p->flyHeight);
#endif

#if __JHERETIC__
    for(int i = 0; i < plrHdr.numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRST + i);

        Writer_WriteInt32(writer, type);
        Writer_WriteInt32(writer, P_InventoryCount(plrnum, type));
    }
    Writer_WriteInt32(writer, P_InventoryReadyItem(plrnum));
    Writer_WriteInt32(writer, p->chickenPeck);
#endif

#if __JHERETIC__ || __JHEXEN__
    Writer_WriteInt32(writer, p->morphTics);
#endif

    Writer_WriteInt32(writer, p->airCounter);

#if __JHEXEN__
    Writer_WriteInt32(writer, p->jumpTics);
    Writer_WriteInt32(writer, p->worldTimer);
#elif __JHERETIC__
    Writer_WriteInt32(writer, p->flameCount);

    // Added in ver 2
    Writer_WriteByte(writer, p->class_);
#endif
}

void player_s::read(Reader *reader, playerheader_t &plrHdr)
{
    int const plrnum = P_GetPlayerNum(this);

    byte ver = Reader_ReadByte(reader);

#if __JHEXEN__
    cfg.playerClass[plrnum] = playerclass_t(Reader_ReadByte(reader));
#endif

    ddplayer_t *dp = plr;

#if __JHEXEN__
    de::zapPtr(this); // Force everything NULL,
    plr = dp;   // but restore the ddplayer pointer.
#endif

    playerState     = playerstate_t(Reader_ReadInt32(reader));
#if __JHEXEN__
    class_          = playerclass_t(Reader_ReadInt32(reader)); // 2nd class?? (ask Raven...)
#endif

    viewZ           = FIX2FLT(Reader_ReadInt32(reader));
    viewHeight      = FIX2FLT(Reader_ReadInt32(reader));
    viewHeightDelta = FIX2FLT(Reader_ReadInt32(reader));
#if !__JHEXEN__
    dp->lookDir       = Reader_ReadFloat(reader);
#endif
    bob             = FIX2FLT(Reader_ReadInt32(reader));
#if __JHEXEN__
    flyHeight       = Reader_ReadInt32(reader);

    dp->lookDir        = Reader_ReadFloat(reader);

    centering       = Reader_ReadInt32(reader);
#endif
    health          = Reader_ReadInt32(reader);

#if __JHEXEN__
    for(int i = 0; i < plrHdr.numArmorTypes; ++i)
    {
        armorPoints[i] = Reader_ReadInt32(reader);
    }
#else
    armorPoints     = Reader_ReadInt32(reader);
    armorType       = Reader_ReadInt32(reader);
#endif

#if __JDOOM64__ || __JHEXEN__
    P_InventoryEmpty(plrnum);
    for(int i = 0; i < plrHdr.numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(Reader_ReadInt32(reader));
        int count = Reader_ReadInt32(reader);

        for(int k = 0; k < count; ++k)
        {
            P_InventoryGive(plrnum, type, true);
        }
    }

    P_InventorySetReadyItem(plrnum, inventoryitemtype_t(Reader_ReadInt32(reader)));
# if __JHEXEN__
    Hu_InventorySelect(plrnum, P_InventoryReadyItem(plrnum));
    if(ver < 5)
    {
        /*artifactCount   =*/ Reader_ReadInt32(reader);
    }
    if(ver < 6)
    {
        /*inventorySlotNum =*/ Reader_ReadInt32(reader);
    }
# endif
#endif

    for(int i = 0; i < plrHdr.numPowers; ++i)
    {
        powers[i] = Reader_ReadInt32(reader);
    }
    if(powers[PT_ALLMAP])
    {
        ST_RevealAutomap(plrnum, true);
    }

#if __JHEXEN__
    keys = Reader_ReadInt32(reader);
#else
    for(int i = 0; i < plrHdr.numKeys; ++i)
    {
        keys[i] = Reader_ReadInt32(reader);
    }
#endif

#if __JHEXEN__
    pieces   = Reader_ReadInt32(reader);
#else
    backpack = Reader_ReadInt32(reader);
#endif

    for(int i = 0; i < plrHdr.numFrags; ++i)
    {
        frags[i] = Reader_ReadInt32(reader);
    }

    readyWeapon = weapontype_t(Reader_ReadInt32(reader));
#if __JHEXEN__
    if(ver < 5)
        pendingWeapon = WT_NOCHANGE;
    else
#endif
        pendingWeapon = weapontype_t(Reader_ReadInt32(reader));

    for(int i = 0; i < plrHdr.numWeapons; ++i)
    {
        weapons[i].owned = (Reader_ReadInt32(reader)? true : false);
    }

    for(int i = 0; i < plrHdr.numAmmoTypes; ++i)
    {
        ammo[i].owned = Reader_ReadInt32(reader);

#if !__JHEXEN__
        ammo[i].max = Reader_ReadInt32(reader);
#endif
    }

    attackDown  = Reader_ReadInt32(reader);
    useDown     = Reader_ReadInt32(reader);
    cheats      = Reader_ReadInt32(reader);
    refire      = Reader_ReadInt32(reader);
    killCount   = Reader_ReadInt32(reader);
    itemCount   = Reader_ReadInt32(reader);
    secretCount = Reader_ReadInt32(reader);

#if __JHEXEN__
    if(ver <= 1)
    {
        /*messageTics     =*/ Reader_ReadInt32(reader);
        /*ultimateMessage =*/ Reader_ReadInt32(reader);
        /*yellowMessage   =*/ Reader_ReadInt32(reader);
    }
#endif

    damageCount = Reader_ReadInt32(reader);
    bonusCount  = Reader_ReadInt32(reader);
#if __JHEXEN__
    poisonCount = Reader_ReadInt32(reader);
#endif

    dp->extraLight    = Reader_ReadInt32(reader);
    dp->fixedColorMap = Reader_ReadInt32(reader);

    colorMap    = Reader_ReadInt32(reader);

    for(int i = 0; i < plrHdr.numPSprites; ++i)
    {
        pspdef_t *psp = &pSprites[i];

        psp->state   = INT2PTR(state_t, Reader_ReadInt32(reader));
        psp->tics    = Reader_ReadInt32(reader);
        psp->pos[VX] = FIX2FLT(Reader_ReadInt32(reader));
        psp->pos[VY] = FIX2FLT(Reader_ReadInt32(reader));
    }

#if !__JHEXEN__
    didSecret = Reader_ReadInt32(reader);

# if __JDOOM__ || __JDOOM64__
    if(ver == 2)
    {
        /*messageTics =*/ Reader_ReadInt32(reader);
    }

    if(ver >= 2)
    {
        flyHeight = Reader_ReadInt32(reader);
    }

# elif __JHERETIC__
    if(ver < 3)
    {
        /*messageTics =*/ Reader_ReadInt32(reader);
    }

    flyHeight = Reader_ReadInt32(reader);

    P_InventoryEmpty(plrnum);
    for(int i = 0; i < plrHdr.numInvItemTypes; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(Reader_ReadInt32(reader));
        int count = Reader_ReadInt32(reader);

        for(int k = 0; k < count; ++k)
        {
            P_InventoryGive(plrnum, type, true);
        }
    }

    P_InventorySetReadyItem(plrnum, (inventoryitemtype_t) Reader_ReadInt32(reader));
    Hu_InventorySelect(plrnum, P_InventoryReadyItem(plrnum));
    if(ver < 5)
    {
        Reader_ReadInt32(reader); // Current inventory item count?
    }
    if(ver < 6)
    {
        /*inventorySlotNum =*/ Reader_ReadInt32(reader);
    }

    chickenPeck = Reader_ReadInt32(reader);
# endif
#endif

#if __JHERETIC__ || __JHEXEN__
    morphTics = Reader_ReadInt32(reader);
#endif

    if(ver >= 2)
    {
        airCounter = Reader_ReadInt32(reader);
    }

#if __JHEXEN__
    jumpTics   = Reader_ReadInt32(reader);
    worldTimer = Reader_ReadInt32(reader);
#elif __JHERETIC__
    flameCount = Reader_ReadInt32(reader);

    if(ver >= 2)
    {
        class_ = playerclass_t(Reader_ReadByte(reader));
    }
#endif

#if !__JHEXEN__
    // Will be set when unarc thinker.
    plr->mo = 0;
    attacker = 0;
#endif

    // Demangle it.
    for(int i = 0; i < plrHdr.numPSprites; ++i)
    {
        if(pSprites[i].state)
        {
            pSprites[i].state = &STATES[PTR2INT(pSprites[i].state)];
        }
    }

    // Mark the player for fixpos and fixangles.
    dp->flags |= DDPF_FIXORIGIN | DDPF_FIXANGLES | DDPF_FIXMOM;
    update |= PSF_REBORN;
}
