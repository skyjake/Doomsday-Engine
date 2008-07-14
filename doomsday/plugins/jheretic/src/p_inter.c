/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/**
 * p_inter.c: Handling interactions (i.e., collisions).
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "am_map.h"
#include "d_net.h"
#include "dmu_lib.h"
#include "p_player.h"
#include "p_inventory.h"
#include "p_tick.h"

// MACROS ------------------------------------------------------------------

#define BONUSADD            (6)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int maxAmmo[NUM_AMMO_TYPES] = {
    100, // gold wand
    50, // crossbow
    200, // blaster
    200, // skull rod
    20, // phoenix rod
    150 // mace
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int getWeaponAmmo[NUM_WEAPON_TYPES] = {
    0, // staff
    25, // gold wand
    10, // crossbow
    30, // blaster
    50, // skull rod
    2, // phoenix rod
    50, // mace
    0 // gauntlets
};

// CODE --------------------------------------------------------------------

/**
 * Returns true if the player accepted the ammo, false if it was
 * refused (player has maxammo[ammo]).
 */
boolean P_GiveAmmo(player_t *player, ammotype_t ammo, int num)
{
    if(ammo == AT_NOAMMO)
        return false;

    if(ammo < 0 || ammo > NUM_AMMO_TYPES)
        Con_Error("P_GiveAmmo: bad type %i", ammo);

    if(!(player->ammo[ammo].owned < player->ammo[ammo].max))
        return false;

    if(gameSkill == SM_BABY || gameSkill == SM_NIGHTMARE)
    {   // Extra ammo in baby mode and nightmare mode.
        num += num / 1;
    }

    // We are about to receive some more ammo. Does the player want to
    // change weapon automatically?
    P_MaybeChangeWeapon(player, WT_NOCHANGE, ammo, false);

    if(player->ammo[ammo].owned + num > player->ammo[ammo].max)
        player->ammo[ammo].owned = player->ammo[ammo].max;
    else
        player->ammo[ammo].owned += num;
    player->update |= PSF_AMMO;

    // Maybe unhide the HUD?
    if(player == &players[CONSOLEPLAYER])
        ST_HUDUnHide(HUE_ON_PICKUP_AMMO);

    return true;
}

/**
 * @return              @c true, if the weapon or its ammo was accepted.
 */
boolean P_GiveWeapon(player_t *player, weapontype_t weapon)
{
    int                 i;
    int                 lvl = (player->powers[PT_WEAPONLEVEL2]? 1 : 0);
    boolean             gaveAmmo = false;
    boolean             gaveWeapon = false;

    if(IS_NETGAME && !deathmatch)
    {
        // Leave placed weapons forever on net games.
        if(player->weapons[weapon].owned)
            return false;

        player->bonusCount += BONUSADD;
        player->weapons[weapon].owned = true;
        player->update |= PSF_OWNED_WEAPONS;

        // Give some of each of the ammo types used by this weapon.
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            if(!weaponInfo[weapon][player->class].mode[lvl].ammoType[i])
                continue;   // Weapon does not take this type of ammo.

            if(P_GiveAmmo(player, i,getWeaponAmmo[weapon]))
                gaveAmmo = true; // At least ONE type of ammo was given.
        }

        // Should we change weapon automatically?
        P_MaybeChangeWeapon(player, weapon, AT_NOAMMO, false);

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER])
            ST_HUDUnHide(HUE_ON_PICKUP_WEAPON);

        S_ConsoleSound(SFX_WPNUP, NULL, player - players);
        return false;
    }
    else
    {
        // Give some of each of the ammo types used by this weapon.
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            if(!weaponInfo[weapon][player->class].mode[lvl].ammoType[i])
                continue;   // Weapon does not take this type of ammo.

            if(P_GiveAmmo(player, i, getWeaponAmmo[weapon]))
                gaveAmmo = true; // At least ONE type of ammo was given.
        }

        if(player->weapons[weapon].owned)
            gaveWeapon = false;
        else
        {
            gaveWeapon = true;
            player->weapons[weapon].owned = true;
            player->update |= PSF_OWNED_WEAPONS;

            // Should we change weapon automatically?
            P_MaybeChangeWeapon(player, weapon, AT_NOAMMO, false);
        }

        // Maybe unhide the HUD?
        if(gaveWeapon && player == &players[CONSOLEPLAYER])
            ST_HUDUnHide(HUE_ON_PICKUP_WEAPON);

        return (gaveWeapon || gaveAmmo);
    }
}

/**
 * @return              @c false, if the body isn't needed at all.
 */
boolean P_GiveBody(player_t *player, int num)
{
    int                 max;

    max = MAXHEALTH;
    if(player->morphTics)
    {
        max = MAXCHICKENHEALTH;
    }

    if(player->health >= max)
    {
        return false;
    }

    player->health += num;
    if(player->health > max)
    {
        player->health = max;
    }

    player->update |= PSF_HEALTH;
    player->plr->mo->health = player->health;

    // Maybe unhide the HUD?
    if(player == &players[CONSOLEPLAYER])
        ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);

    return true;
}

/**
 * @return              @c false, if the armor is worse than the
 *                      current armor.
 */
boolean P_GiveArmor(player_t *player, int armortype)
{
    int                 hits;

    hits = armortype * 100;
    if(player->armorPoints >= hits)
        return false;

    player->armorType = armortype;
    player->armorPoints = hits;
    player->update |= PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;

    // Maybe unhide the HUD?
    if(player == &players[CONSOLEPLAYER])
        ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);

    return true;
}

void P_GiveKey(player_t *player, keytype_t key)
{
    if(player->keys[key])
        return;

    if(player == &players[CONSOLEPLAYER])
    {
        playerKeys |= 1 << key;
    }

    player->bonusCount = BONUSADD;
    player->keys[key] = true;
    player->update |= PSF_KEYS;

    // Maybe unhide the HUD?
    if(player == &players[CONSOLEPLAYER])
        ST_HUDUnHide(HUE_ON_PICKUP_KEY);
}

/**
 * @return              @c true, if power accepted.
 */
boolean P_GivePower(player_t *player, powertype_t power)
{
    mobj_t             *plrmo = player->plr->mo;
    boolean             retval = false;

    player->update |= PSF_POWERS;
    switch(power)
    {
    case PT_INVULNERABILITY:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = INVULNTICS;
            retval = true;
        }
        break;

    case PT_WEAPONLEVEL2:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = WPNLEV2TICS;
            retval = true;
        }
        break;

    case PT_INVISIBILITY:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = INVISTICS;
            plrmo->flags |= MF_SHADOW;
            retval = true;
        }
        break;

    case PT_FLIGHT:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = FLIGHTTICS;
            plrmo->flags2 |= MF2_FLY;
            plrmo->flags |= MF_NOGRAVITY;
            if(plrmo->pos[VZ] <= plrmo->floorZ)
            {
                player->flyHeight = 10; // Thrust the player in the air a bit.
                player->plr->flags |= DDPF_FIXMOM;
            }
            retval = true;
        }
        break;

    case PT_INFRARED:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = INFRATICS;
            retval = true;
        }
        break;

    default:
        if(!player->powers[power])
        {
            player->powers[power] = 1;
            retval = true;
        }
        break;
    }

    if(retval)
    {
        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER])
            ST_HUDUnHide(HUE_ON_PICKUP_POWER);
    }

    return retval;
}

/**
 * Removes the MF_SPECIAL flag, and initiates the artifact pickup
 * animation.
 */
void P_SetDormantArtifact(mobj_t *arti)
{
    arti->flags &= ~MF_SPECIAL;
    if(deathmatch && (arti->type != MT_ARTIINVULNERABILITY) &&
       (arti->type != MT_ARTIINVISIBILITY))
    {
        P_MobjChangeState(arti, S_DORMANTARTI1);
    }
    else
    {   // Don't respawn.
        P_MobjChangeState(arti, S_DEADARTI1);
    }

    S_StartSound(SFX_ARTIUP, arti);
}

void C_DECL A_RestoreArtifact(mobj_t *arti)
{
    arti->flags |= MF_SPECIAL;
    P_MobjChangeState(arti, arti->info->spawnState);
    S_StartSound(SFX_RESPAWN, arti);
}

void P_HideSpecialThing(mobj_t *thing)
{
    thing->flags &= ~MF_SPECIAL;
    thing->flags2 |= MF2_DONTDRAW;
    P_MobjChangeState(thing, S_HIDESPECIAL1);
}

/**
 * Make a special thing visible again.
 */
void C_DECL A_RestoreSpecialThing1(mobj_t *thing)
{
    if(thing->type == MT_WMACE)
    {   // Do random mace placement.
        P_RepositionMace(thing);
    }

    thing->flags2 &= ~MF2_DONTDRAW;
    S_StartSound(SFX_RESPAWN, thing);
}

void C_DECL A_RestoreSpecialThing2(mobj_t *thing)
{
    thing->flags |= MF_SPECIAL;
    P_MobjChangeState(thing, thing->info->spawnState);
}

void P_TouchSpecialMobj(mobj_t *special, mobj_t *toucher)
{
    int                 i;
    player_t           *player;
    float               delta;
    int                 sound;
    boolean             respawn;

    delta = special->pos[VZ] - toucher->pos[VZ];
    if(delta > toucher->height || delta < -32)
    {
        return; // Out of reach.
    }

    if(toucher->health <= 0)
    {
        return; // Toucher is dead.
    }

    sound = SFX_ITEMUP;
    player = toucher->player;
    if(player == NULL)
        return;

    respawn = true;
    switch(special->sprite)
    {
    // Items
    case SPR_PTN1: // Item_HealingPotion
        if(!P_GiveBody(player, 10))
            return;

        P_SetMessage(player, TXT_ITEMHEALTH, false);
        break;

    case SPR_SHLD: // Item_Shield1
        if(!P_GiveArmor(player, 1))
            return;

        P_SetMessage(player, TXT_ITEMSHIELD1, false);
        break;

    case SPR_SHD2: // Item_Shield2
        if(!P_GiveArmor(player, 2))
            return;

        P_SetMessage(player, TXT_ITEMSHIELD2, false);
        break;

    case SPR_BAGH: // Item_BagOfHolding
        if(!player->backpack)
        {
            for(i = 0; i < NUM_AMMO_TYPES; ++i)
            {
                player->ammo[i].max *= 2;
            }
            player->backpack = true;
        }

        P_GiveAmmo(player, AT_CRYSTAL, AMMO_GWND_WIMPY);
        P_GiveAmmo(player, AT_ORB, AMMO_BLSR_WIMPY);
        P_GiveAmmo(player, AT_ARROW, AMMO_CBOW_WIMPY);
        P_GiveAmmo(player, AT_RUNE, AMMO_SKRD_WIMPY);
        P_GiveAmmo(player, AT_FIREORB, AMMO_PHRD_WIMPY);
        P_SetMessage(player, TXT_ITEMBAGOFHOLDING, false);
        break;

    case SPR_SPMP: // Item_SuperMap.
        if(!P_GivePower(player, PT_ALLMAP))
            return;

        P_SetMessage(player, TXT_ITEMSUPERMAP, false);
        break;

     // Keys
    case SPR_BKYY: // Key_Blue
        if(!player->keys[KT_BLUE])
        {
            P_SetMessage(player, TXT_GOTBLUEKEY, false);
        }

        P_GiveKey(player, KT_BLUE);
        sound = SFX_KEYUP;
        if(!IS_NETGAME)
        {
            break;
        }
        return;

    case SPR_CKYY: // Key_Yellow
        if(!player->keys[KT_YELLOW])
        {
            P_SetMessage(player, TXT_GOTYELLOWKEY, false);
        }

        sound = SFX_KEYUP;
        P_GiveKey(player, KT_YELLOW);
        if(!IS_NETGAME)
        {
            break;
        }
        return;

    case SPR_AKYY: // Key_Green
        if(!player->keys[KT_GREEN])
        {
            P_SetMessage(player, TXT_GOTGREENKEY, false);
        }

        sound = SFX_KEYUP;
        P_GiveKey(player, KT_GREEN);
        if(!IS_NETGAME)
        {
            break;
        }
        return;

    // Artifacts
    case SPR_PTN2: // Arti_HealingPotion
        if(P_GiveArtifact(player, arti_health, special))
        {
            P_SetMessage(player, TXT_ARTIHEALTH, false);
            P_SetDormantArtifact(special);
        }
        return;

    case SPR_SOAR: // Arti_Fly.
        if(P_GiveArtifact(player, arti_fly, special))
        {
            P_SetMessage(player, TXT_ARTIFLY, false);
            P_SetDormantArtifact(special);
        }
        return;

    case SPR_INVU: // Arti_Invulnerability.
        if(P_GiveArtifact(player, arti_invulnerability, special))
        {
            P_SetMessage(player, TXT_ARTIINVULNERABILITY, false);
            P_SetDormantArtifact(special);
        }
        return;

    case SPR_PWBK: // Arti_TomeOfPower.
        if(P_GiveArtifact(player, arti_tomeofpower, special))
        {
            P_SetMessage(player, TXT_ARTITOMEOFPOWER, false);
            P_SetDormantArtifact(special);
        }
        return;

    case SPR_INVS: // Arti_Invisibility.
        if(P_GiveArtifact(player, arti_invisibility, special))
        {
            P_SetMessage(player, TXT_ARTIINVISIBILITY, false);
            P_SetDormantArtifact(special);
        }
        return;

    case SPR_EGGC: // Arti_Egg.
        if(P_GiveArtifact(player, arti_egg, special))
        {
            P_SetMessage(player, TXT_ARTIEGG, false);
            P_SetDormantArtifact(special);
        }
        return;

    case SPR_SPHL: // Arti_SuperHealth.
        if(P_GiveArtifact(player, arti_superhealth, special))
        {
            P_SetMessage(player, TXT_ARTISUPERHEALTH, false);
            P_SetDormantArtifact(special);
        }
        return;

    case SPR_TRCH: // Arti_Torch.
        if(P_GiveArtifact(player, arti_torch, special))
        {
            P_SetMessage(player, TXT_ARTITORCH, false);
            P_SetDormantArtifact(special);
        }
        return;

    case SPR_FBMB: // Arti_FireBomb.
        if(P_GiveArtifact(player, arti_firebomb, special))
        {
            P_SetMessage(player, TXT_ARTIFIREBOMB, false);
            P_SetDormantArtifact(special);
        }
        return;

    case SPR_ATLP: // Arti_Teleport.
        if(P_GiveArtifact(player, arti_teleport, special))
        {
            P_SetMessage(player, TXT_ARTITELEPORT, false);
            P_SetDormantArtifact(special);
        }
        return;

    // Ammo
    case SPR_AMG1: // Ammo_GoldWandWimpy.
        if(!P_GiveAmmo(player, AT_CRYSTAL, special->health))
            return;

        P_SetMessage(player, TXT_AMMOGOLDWAND1, false);
        break;

    case SPR_AMG2: // Ammo_GoldWandHefty.
        if(!P_GiveAmmo(player, AT_CRYSTAL, special->health))
            return;

        P_SetMessage(player, TXT_AMMOGOLDWAND2, false);
        break;

    case SPR_AMM1: // Ammo_MaceWimpy.
        if(!P_GiveAmmo(player, AT_MSPHERE, special->health))
            return;

        P_SetMessage(player, TXT_AMMOMACE1, false);
        break;

    case SPR_AMM2: // Ammo_MaceHefty.
        if(!P_GiveAmmo(player, AT_MSPHERE, special->health))
            return;

        P_SetMessage(player, TXT_AMMOMACE2, false);
        break;

    case SPR_AMC1: // Ammo_CrossbowWimpy.
        if(!P_GiveAmmo(player, AT_ARROW, special->health))
            return;

        P_SetMessage(player, TXT_AMMOCROSSBOW1, false);
        break;

    case SPR_AMC2: // Ammo_CrossbowHefty.
        if(!P_GiveAmmo(player, AT_ARROW, special->health))
            return;

        P_SetMessage(player, TXT_AMMOCROSSBOW2, false);
        break;

    case SPR_AMB1: // Ammo_BlasterWimpy.
        if(!P_GiveAmmo(player, AT_ORB, special->health))
            return;

        P_SetMessage(player, TXT_AMMOBLASTER1, false);
        break;

    case SPR_AMB2: // Ammo_BlasterHefty.
        if(!P_GiveAmmo(player, AT_ORB, special->health))
            return;

        P_SetMessage(player, TXT_AMMOBLASTER2, false);
        break;

    case SPR_AMS1: // Ammo_SkullRodWimpy.
        if(!P_GiveAmmo(player, AT_RUNE, special->health))
            return;

        P_SetMessage(player, TXT_AMMOSKULLROD1, false);
        break;

    case SPR_AMS2: // Ammo_SkullRodHefty.
        if(!P_GiveAmmo(player, AT_RUNE, special->health))
            return;

        P_SetMessage(player, TXT_AMMOSKULLROD2, false);
        break;

    case SPR_AMP1: // Ammo_PhoenixRodWimpy.
        if(!P_GiveAmmo(player, AT_FIREORB, special->health))
            return;

        P_SetMessage(player, TXT_AMMOPHOENIXROD1, false);
        break;

    case SPR_AMP2: // Ammo_PhoenixRodHefty.
        if(!P_GiveAmmo(player, AT_FIREORB, special->health))
            return;

        P_SetMessage(player, TXT_AMMOPHOENIXROD2, false);
        break;

    // Weapons
    case SPR_WMCE: // Weapon_Mace.
        if(!P_GiveWeapon(player, WT_SEVENTH))
            return;

        P_SetMessage(player, TXT_WPNMACE, false);
        sound = SFX_WPNUP;
        break;

    case SPR_WBOW: // Weapon_Crossbow.
        if(!P_GiveWeapon(player, WT_THIRD))
            return;

        P_SetMessage(player, TXT_WPNCROSSBOW, false);
        sound = SFX_WPNUP;
        break;

    case SPR_WBLS: // Weapon_Blaster.
        if(!P_GiveWeapon(player, WT_FOURTH))
            return;

        P_SetMessage(player, TXT_WPNBLASTER, false);
        sound = SFX_WPNUP;
        break;

    case SPR_WSKL: // Weapon_SkullRod.
        if(!P_GiveWeapon(player, WT_FIFTH))
            return;

        P_SetMessage(player, TXT_WPNSKULLROD, false);
        sound = SFX_WPNUP;
        break;

    case SPR_WPHX: // Weapon_PhoenixRod.
        if(!P_GiveWeapon(player, WT_SIXTH))
            return;

        P_SetMessage(player, TXT_WPNPHOENIXROD, false);
        sound = SFX_WPNUP;
        break;

    case SPR_WGNT: // Weapon_Gauntlets.
        if(!P_GiveWeapon(player, WT_EIGHTH))
            return;

        P_SetMessage(player, TXT_WPNGAUNTLETS, false);
        sound = SFX_WPNUP;
        break;

    default:
        Con_Error("P_SpecialThing: Unknown gettable thing");
    }

    if(special->flags & MF_COUNTITEM)
    {
        player->itemCount++;
    }

    if(deathmatch && respawn && !(special->flags & MF_DROPPED))
    {
        P_HideSpecialThing(special);
    }
    else
    {
        P_MobjRemove(special, false);
    }

    player->bonusCount += BONUSADD;

    if(player == &players[CONSOLEPLAYER])
        ST_doPaletteStuff();

    S_ConsoleSound(sound, NULL, player - players);
}

void P_KillMobj(mobj_t *source, mobj_t *target)
{
    if(!target) // Nothing to kill.
        return;

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_NOGRAVITY);
    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->flags2 &= ~MF2_PASSMOBJ;
    target->corpseTics = 0;
    target->height /= 2*2;
    if(source && source->player)
    {
        if(target->flags & MF_COUNTKILL)
        {   // Count for intermission.
            source->player->killCount++;
        }

        if(target->player)
        {   // Frag stuff.
            source->player->update |= PSF_FRAGS;
            if(target == source)
            {   // Self-frag.
                target->player->frags[target->player - players]--;
                NetSv_FragsForAll(target->player);
            }
            else
            {
                source->player->frags[target->player - players]++;
                NetSv_FragsForAll(source->player);

                if(source->player->morphTics)
                {   // Make a super chicken.
                    P_GivePower(source->player, PT_WEAPONLEVEL2);
                }
            }
        }
    }
    else if(!IS_NETGAME && (target->flags & MF_COUNTKILL))
    {   // Count all monster deaths.
        players[0].killCount++;
    }

    if(target->player)
    {
        if(!source)
        {   // Self-frag.
            target->player->frags[target->player - players]--;
            NetSv_FragsForAll(target->player);
        }

        target->flags &= ~MF_SOLID;
        target->flags2 &= ~MF2_FLY;
        target->player->powers[PT_FLIGHT] = 0;
        target->player->powers[PT_WEAPONLEVEL2] = 0;
        target->player->playerState = PST_DEAD;
        target->player->plr->flags |= DDPF_DEAD;
        target->player->update |= PSF_STATE;
        P_DropWeapon(target->player);

        if(target->flags2 & MF2_FIREDAMAGE)
        {   // Player flame death.
            P_MobjChangeState(target, S_PLAY_FDTH1);
            return;
        }

        // Don't die with the automap open.
        AM_Stop(target->player - players);
    }

    if(target->health < -(target->info->spawnHealth / 2) &&
       target->info->xDeathState)
    {   // Extreme death.
        P_MobjChangeState(target, target->info->xDeathState);
    }
    else
    {   // Normal death.
        P_MobjChangeState(target, target->info->deathState);
    }

    target->tics -= P_Random() & 3;
}

void P_MinotaurSlam(mobj_t *source, mobj_t *target)
{
    angle_t             angle;
    uint                an;
    float               thrust;

    angle = R_PointToAngle2(source->pos[VX], source->pos[VY],
                            target->pos[VX], target->pos[VY]);
    an = angle >> ANGLETOFINESHIFT;
    thrust = 16 + FIX2FLT(P_Random() << 10);
    target->mom[MX] += thrust * FIX2FLT(finecosine[angle]);
    target->mom[MY] += thrust * FIX2FLT(finesine[angle]);

    P_DamageMobj(target, NULL, NULL, HITDICE(6));
    if(target->player)
    {
        target->reactionTime = 14 + (P_Random() & 7);
    }
}

void P_TouchWhirlwind(mobj_t *target)
{
    int                 randVal;

    target->angle += (P_Random() - P_Random()) << 20;
    target->mom[MX] += FIX2FLT((P_Random() - P_Random()) << 10);
    target->mom[MY] += FIX2FLT((P_Random() - P_Random()) << 10);

    if(levelTime & 16 && !(target->flags2 & MF2_BOSS))
    {
        randVal = P_Random();
        if(randVal > 160)
        {
            randVal = 160;
        }

        target->mom[MZ] += FIX2FLT(randVal << 10);
        if(target->mom[MZ] > 12)
        {
            target->mom[MZ] = 12;
        }
    }

    if(!(levelTime & 7))
    {
        P_DamageMobj(target, NULL, NULL, 3);
    }
}

/**
 * @return              @c true, if the player is morphed.
 */
boolean P_MorphPlayer(player_t *player)
{
    mobj_t             *pmo, *fog, *chicken;
    float               pos[3];
    angle_t             angle;
    int                 oldFlags2;

    if(player->morphTics)
    {
        if((player->morphTics < CHICKENTICS - TICSPERSEC) &&
           !player->powers[PT_WEAPONLEVEL2])
        {   // Make a super chicken.
            P_GivePower(player, PT_WEAPONLEVEL2);
        }
        return false;
    }

    if(player->powers[PT_INVULNERABILITY])
    {   // Immune when invulnerable.
        return false;
    }

    pmo = player->plr->mo;
    memcpy(pos, pmo->pos, sizeof(pos));
    angle = pmo->angle;
    oldFlags2 = pmo->flags2;
    P_MobjChangeState(pmo, S_FREETARGMOBJ);

    fog = P_SpawnMobj3f(MT_TFOG, pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT,
                        angle + ANG180);
    S_StartSound(SFX_TELEPT, fog);

    chicken = P_SpawnMobj3fv(MT_CHICPLAYER, pos, angle);
    chicken->special1 = player->readyWeapon;
    chicken->player = player;
    chicken->dPlayer = player->plr;

    player->health = chicken->health = MAXCHICKENHEALTH;
    player->plr->mo = chicken;
    player->armorPoints = player->armorType = 0;
    player->powers[PT_INVISIBILITY] = 0;
    player->powers[PT_WEAPONLEVEL2] = 0;

    if(oldFlags2 & MF2_FLY)
        chicken->flags2 |= MF2_FLY;

    player->morphTics = CHICKENTICS;
    player->plr->flags |= DDPF_FIXPOS | DDPF_FIXMOM;
    player->update |=
        PSF_MORPH_TIME | PSF_HEALTH | PSF_POWERS | PSF_ARMOR_POINTS;

    P_ActivateMorphWeapon(player);
    return true;
}

boolean P_MorphMonster(mobj_t *actor)
{
    mobj_t             *fog, *chicken, *target;
    mobjtype_t          moType;
    float               pos[3];
    angle_t             angle;
    int                 ghost;

    if(actor->player)
        return false;

    moType = actor->type;
    switch(moType)
    {
    case MT_POD:
    case MT_CHICKEN:
    case MT_HEAD:
    case MT_MINOTAUR:
    case MT_SORCERER1:
    case MT_SORCERER2:
        return false;

    default:
        break;
    }

    memcpy(pos, actor->pos, sizeof(pos));
    angle = actor->angle;
    ghost = actor->flags & MF_SHADOW;
    target = actor->target;
    P_MobjChangeState(actor, S_FREETARGMOBJ);

    fog = P_SpawnMobj3f(MT_TFOG, pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT,
                        angle + ANG180);
    S_StartSound(SFX_TELEPT, fog);

    chicken = P_SpawnMobj3fv(MT_CHICKEN, pos, angle);
    chicken->special2 = moType;
    chicken->special1 = CHICKENTICS + P_Random();
    chicken->flags |= ghost;
    chicken->target = target;

    return true;
}

boolean P_AutoUseChaosDevice(player_t *player)
{
    int                 i;

    //// \todo Do this in the inventory code.
    for(i = 0; i < player->inventorySlotNum; ++i)
    {
        if(player->inventory[i].type == arti_teleport)
        {
            P_InventoryUseArtifact(player, arti_teleport);
            player->health = player->plr->mo->health =
                (player->health + 1) / 2;
            return (true);
        }
    }

    return false;
}

void P_AutoUseHealth(player_t *player, int saveHealth)
{
    int                 i;
    int                 count;
    int                 normalCount = 0;
    int                 normalSlot = 0;
    int                 superCount = 0;
    int                 superSlot = 0;

    //// \todo Do this in the inventory code.
    for(i = 0; i < player->inventorySlotNum; ++i)
    {
        if(player->inventory[i].type == arti_health)
        {
            normalSlot = i;
            normalCount = player->inventory[i].count;
        }
        else if(player->inventory[i].type == arti_superhealth)
        {
            superSlot = i;
            superCount = player->inventory[i].count;
        }
    }

    if((gameSkill == SM_BABY) && (normalCount * 25 >= saveHealth))
    {
        // Use quartz flasks.
        count = (saveHealth + 24) / 25;
        for(i = 0; i < count; ++i)
        {
            player->health += 25;
            P_InventoryRemoveArtifact(player, normalSlot);
        }
    }
    else if(superCount * 100 >= saveHealth)
    {
        // Use mystic urns.
        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; ++i)
        {
            player->health += 100;
            P_InventoryRemoveArtifact(player, superSlot);
        }
    }
    else if((gameSkill == SM_BABY) &&
            (superCount * 100 + normalCount * 25 >= saveHealth))
    {
        // Use mystic urns and quartz flasks.
        count = (saveHealth + 24) / 25;
        saveHealth -= count * 25;
        for(i = 0; i < count; ++i)
        {
            player->health += 25;
            P_InventoryRemoveArtifact(player, normalSlot);
        }

        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; ++i)
        {
            player->health += 100;
            P_InventoryRemoveArtifact(player, normalSlot);
        }
    }

    player->plr->mo->health = player->health;
}

void P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                  int damage)
{
    P_DamageMobj2(target, inflictor, source, damage, false);
}

/**
 * Damages both enemies and players
 * inflictor is the thing that caused the damage
 *        creature or missile, can be NULL (slime, etc)
 * source is the thing to target after taking damage
 *        creature or NULL
 * Source and inflictor are the same for melee attacks
 * source can be null for barrel explosions and other environmental stuff
 */
void P_DamageMobj2(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                  int damage, boolean stomping)
{
    angle_t             angle;
    int                 saved;
    player_t           *player;
    float               thrust;
    int                 temp;

    // Clients are not able to damage anything.
    if(IS_CLIENT)
        return;

    if(!(target->flags & MF_SHOOTABLE)) // Shouldn't happen.
        return;

    if(target->health <= 0)
        return;

    if(target->flags & MF_SKULLFLY)
    {
        if(target->type == MT_MINOTAUR)
        {   // Minotaur is invulnerable during charge attack.
            return;
        }
        target->mom[MX] = target->mom[MY] = target->mom[MZ] = 0;
    }

    player = target->player;

    // In trainer mode? then take half damage.
    if(player && gameSkill == SM_BABY)
        damage /= 2;

    // Special damage types.
    if(inflictor)
    {
        switch(inflictor->type)
        {
        case MT_EGGFX:
            if(player)
            {
                P_MorphPlayer(player);
            }
            else
            {
                P_MorphMonster(target);
            }
            return; // Always return.

        case MT_WHIRLWIND:
            P_TouchWhirlwind(target);
            return;

        case MT_MINOTAUR:
            if(inflictor->flags & MF_SKULLFLY)
            {
                // Slam only when in charge mode.
                P_MinotaurSlam(inflictor, target);
                return;
            }
            break;

        case MT_MACEFX4:
            // Death ball.
            if((target->flags2 & MF2_BOSS) || target->type == MT_HEAD)
            {
                // Don't allow cheap boss kills.
                break;
            }
            else if(target->player)
            {
                // Player specific checks.

                // Is player invulnerable?
                if(target->player->powers[PT_INVULNERABILITY])
                    break;

                // Does the player have a Chaos Device he can use to get
                // him out of trouble?
                if(P_AutoUseChaosDevice(target->player))
                    return; // He's lucky... this time.
            }

            // Something's gonna die.
            damage = 10000;
            break;

        case MT_PHOENIXFX2:
            // Flame thrower.
            if(target->player && P_Random() < 128)
            {
                // Freeze player for a bit.
                target->reactionTime += 4;
            }
            break;

        case MT_RAINPLR1:
        case MT_RAINPLR2:
        case MT_RAINPLR3:
        case MT_RAINPLR4:
            // Rain missiles.
            if(target->flags2 & MF2_BOSS)
            {
                // Decrease damage for bosses.
                damage = (P_Random() & 7) + 1;
            }
            break;

        case MT_HORNRODFX2:
        case MT_PHOENIXFX1:
            if(target->type == MT_SORCERER2 && P_Random() < 96)
            {
                // D'Sparil teleports away.
                P_DSparilTeleport(target);
                return;
            }
            break;

        case MT_BLASTERFX1:
        case MT_RIPPER:
            if(target->type == MT_HEAD)
            {
                // Less damage to Ironlich bosses.
                damage = P_Random() & 1;
                if(!damage)
                    return;
            }
            break;

        default:
            break;
        }
    }

    // Push the target unless source is using the gauntlets.
    if(inflictor && !(target->flags & MF_NOCLIP) &&
       (!source || !source->player ||
        source->player->readyWeapon != WT_EIGHTH) &&
       !(inflictor->flags2 & MF2_NODMGTHRUST))
    {
        uint                    an;

        angle = R_PointToAngle2(inflictor->pos[VX], inflictor->pos[VY],
                                target->pos[VX], target->pos[VY]);

        thrust = FIX2FLT(damage * (FRACUNIT>>3) * 100 / target->info->mass);

        // Make fall forwards sometimes.
        if((damage < 40) && (damage > target->health) &&
           (target->pos[VZ] - inflictor->pos[VZ] > 64) && (P_Random() & 1))
        {
            angle += ANG180;
            thrust *= 4;
        }

        an = angle >> ANGLETOFINESHIFT;

        if(source && source->player && (source == inflictor) &&
           source->player->powers[PT_WEAPONLEVEL2] &&
           source->player->readyWeapon == WT_FIRST)
        {
            // Staff power level 2
            target->mom[MX] += 10 * FIX2FLT(finecosine[an]);
            target->mom[MY] += 10 * FIX2FLT(finesine[an]);
            if(!(target->flags & MF_NOGRAVITY))
            {
                target->mom[MZ] += 5;
            }
        }
        else
        {
            target->mom[MX] += thrust * FIX2FLT(finecosine[an]);
            target->mom[MY] += thrust * FIX2FLT(finesine[an]);
        }

        if(target->dPlayer)
        {
            // Only fix momentum. Otherwise clients will find it difficult
            // to escape from the damage inflictor.
            target->dPlayer->flags |= DDPF_FIXMOM;
        }

        // $dropoff_fix: thrust objects hanging off ledges.
        if((target->intFlags & MIF_FALLING) && target->gear >= MAXGEAR)
            target->gear = 0;
    }

    // Player specific.
    if(player)
    {
        if(damage < 1000 &&
           ((P_GetPlayerCheats(player) & CF_GODMODE) ||
            player->powers[PT_INVULNERABILITY]))
        {
            return;
        }

        if(player->armorType)
        {
            if(player->armorType == 1)
            {
                saved = damage / 2;
            }
            else
            {
                saved = (damage / 2) + (damage / 4);
            }

            if(player->armorPoints <= saved)
            {
                // Armor is used up.
                saved = player->armorPoints;
                player->armorType = 0;
            }

            player->armorPoints -= saved;
            damage -= saved;
            player->update |= PSF_ARMOR_POINTS;
        }

        if(damage >= player->health &&
           ((gameSkill == SM_BABY) || deathmatch) && !player->morphTics)
        {   // Try to use some inventory health.
            P_AutoUseHealth(player, damage - player->health + 1);
        }

        player->health -= damage;
        if(player->health < 0)
        {
            player->health = 0;
        }

        player->update |= PSF_HEALTH;
        player->attacker = source;

        player->damageCount += damage; // Add damage after armor / invuln.
        if(player->damageCount > 100)
        {
            player->damageCount = 100; // Teleport stomp does 10k points...
        }
        temp = damage < 100 ? damage : 100;

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER]);
            ST_HUDUnHide(HUE_ON_DAMAGE);
    }

    // How about some particles, yes?
    // Only works when both target and inflictor are real mobjs.
    P_SpawnDamageParticleGen(target, inflictor, damage);

    // Do the damage.
    target->health -= damage;
    if(target->health <= 0)
    {
        // Death.
        target->special1 = damage;
        if(target->type == MT_POD && source && source->type != MT_POD)
        {
            // Make sure players get frags for chain-reaction kills.
            target->target = source;
        }

        if(player && inflictor && !player->morphTics)
        {
            // Check for flame death.
            if((inflictor->flags2 & MF2_FIREDAMAGE) ||
               ((inflictor->type == MT_PHOENIXFX1) && (target->health > -50) &&
                (damage > 25)))
            {
                target->flags2 |= MF2_FIREDAMAGE;
            }
        }

        P_KillMobj(source, target);
        return;
    }

    if((P_Random() < target->info->painChance) &&
       !(target->flags & MF_SKULLFLY))
    {
        // Fight back!
        target->flags |= MF_JUSTHIT;
        P_MobjChangeState(target, target->info->painState);
    }

    // We're awake now...
    target->reactionTime = 0;
    if(!target->threshold && source && !(source->flags3 & MF3_NOINFIGHT) &&
       !(target->type == MT_SORCERER2 && source->type == MT_WIZARD))
    {
        // Target actor is not intent on another actor, so make him chase
        // after the source of the damage.
        target->target = source;
        target->threshold = BASETHRESHOLD;
        if(target->state == &states[target->info->spawnState] &&
           target->info->seeState != S_NULL)
        {
            P_MobjChangeState(target, target->info->seeState);
        }
    }
}
