/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_inter.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jhexen.h"

#include "am_map.h"
#include "p_inventory.h"
#include "p_player.h"
#include "p_map.h"
#include "p_user.h"

// MACROS ------------------------------------------------------------------

#define BONUSADD 6

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void SetDormantArtifact(mobj_t *arti);
static void TryPickupArtifact(player_t *plr, artitype_e artifactType,
                              mobj_t *artifact);
static void TryPickupWeapon(player_t *plr, playerclass_t weaponClass,
                            weapontype_t weaponType, mobj_t *weapon,
                            char *message);
static void TryPickupWeaponPiece(player_t *plr, playerclass_t matchClass,
                                 int pieceValue, mobj_t *pieceMobj);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     echoMsg = 1;

int     TextKeyMessages[] = {
    TXT_TXT_KEY_STEEL,
    TXT_TXT_KEY_CAVE,
    TXT_TXT_KEY_AXE,
    TXT_TXT_KEY_FIRE,
    TXT_TXT_KEY_EMERALD,
    TXT_TXT_KEY_DUNGEON,
    TXT_TXT_KEY_SILVER,
    TXT_TXT_KEY_RUSTED,
    TXT_TXT_KEY_HORN,
    TXT_TXT_KEY_SWAMP,
    TXT_TXT_KEY_CASTLE
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_HideSpecialThing(mobj_t *thing)
{
    thing->flags &= ~MF_SPECIAL;
    thing->flags2 |= MF2_DONTDRAW;
    P_MobjChangeState(thing, S_HIDESPECIAL1);
}

/**
 * @return              @c true, if the player accepted the mana,
 *                      @c false, if it was refused (player has MAX_MANA)
 */
boolean P_GiveMana(player_t *plr, ammotype_t ammo, int num)
{
    int                 prevMana;

    if(ammo == AT_NOAMMO)
        return false;

    if(ammo < 0 || ammo > NUM_AMMO_TYPES)
        Con_Error("P_GiveMana: bad type %i", ammo);

    if(!(plr->ammo[ammo].owned < MAX_MANA))
        return false;

    if(gameSkill == SM_BABY || gameSkill == SM_NIGHTMARE)
    {   // Extra mana in baby mode and nightmare mode.
        num += num / 2;
    }
    prevMana = plr->ammo[ammo].owned;

    // We are about to receive some more ammo. Does the plr want to
    // change weapon automatically?
    P_MaybeChangeWeapon(plr, WT_NOCHANGE, ammo, false);

    if(plr->ammo[ammo].owned + num > MAX_MANA)
        plr->ammo[ammo].owned = MAX_MANA;
    else
        plr->ammo[ammo].owned += num;
    plr->update |= PSF_AMMO;

    //// \fixme - DJS: This shouldn't be actioned from here.
    if(plr->class == PCLASS_FIGHTER && plr->readyWeapon == WT_SECOND &&
       ammo == AT_BLUEMANA && prevMana <= 0)
    {
        P_SetPsprite(plr, ps_weapon, S_FAXEREADY_G);
    } // < FIXME

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_AMMO);

    return true;
}

static void TryPickupWeapon(player_t *plr, playerclass_t weaponClass,
                            weapontype_t weaponType, mobj_t *weapon,
                            char *message)
{
    boolean         remove, gaveMana, gaveWeapon;

    plr->update |= PSF_WEAPONS;

    remove = true;
    if(plr->class != weaponClass)
    {   // Wrong class, but try to pick up for mana.
        if(IS_NETGAME && !deathmatch)
        {   // Can't pick up weapons for other classes in coop netplay.
            return;
        }

        if(weaponType == WT_SECOND)
        {
            if(!P_GiveMana(plr, AT_BLUEMANA, 25))
            {
                return;
            }
        }
        else
        {
            if(!P_GiveMana(plr, AT_GREENMANA, 25))
            {
                return;
            }
        }
    }
    else if(IS_NETGAME && !deathmatch)
    {   // Cooperative net-game.
        if(plr->weapons[weaponType].owned)
        {
            return;
        }

        plr->weapons[weaponType].owned = true;
        plr->update |= PSF_OWNED_WEAPONS;
        if(weaponType == WT_SECOND)
        {
            P_GiveMana(plr, AT_BLUEMANA, 25);
        }
        else
        {
            P_GiveMana(plr, AT_GREENMANA, 25);
        }
        plr->pendingWeapon = weaponType;
        remove = false;

        // Maybe unhide the HUD?
        ST_HUDUnHide(plr - players, HUE_ON_PICKUP_WEAPON);
    }
    else
    {   // Deathmatch or single player game.
        if(weaponType == WT_SECOND)
        {
            gaveMana = P_GiveMana(plr, AT_BLUEMANA, 25);
        }
        else
        {
            gaveMana = P_GiveMana(plr, AT_GREENMANA, 25);
        }

        if(plr->weapons[weaponType].owned)
        {
            gaveWeapon = false;
        }
        else
        {
            gaveWeapon = true;
            plr->weapons[weaponType].owned = true;
            plr->update |= PSF_OWNED_WEAPONS;

            // Should we change weapon automatically?
            P_MaybeChangeWeapon(plr, weaponType, AT_NOAMMO, false);
        }

        // Maybe unhide the HUD?
        if(gaveWeapon)
            ST_HUDUnHide(plr - players, HUE_ON_PICKUP_WEAPON);

        if(!(gaveWeapon || gaveMana))
        {    // Player didn't need the weapon or any mana.
            return;
        }
    }

    P_SetMessage(plr, message, false);
    if(weapon->special)
    {
        P_ExecuteLineSpecial(weapon->special, weapon->args, NULL, 0,
                             plr->plr->mo);
        weapon->special = 0;
    }

    if(remove)
    {
        if(deathmatch && !(weapon->flags2 & MF2_DROPPED))
        {
            P_HideSpecialThing(weapon);
        }
        else
        {
            P_MobjRemove(weapon, false);
        }
    }

    plr->bonusCount += BONUSADD;
    S_ConsoleSound(SFX_PICKUP_WEAPON, NULL, plr - players);
    ST_doPaletteStuff(plr - players, false);
}

static void TryPickupWeaponPiece(player_t *plr, playerclass_t matchClass,
                                 int pieceValue, mobj_t *pieceMobj)
{
    boolean         remove, checkAssembled, gaveWeapon;
    int             gaveMana;
    static int      fourthWeaponText[] = {
        TXT_TXT_WEAPON_F4,
        TXT_TXT_WEAPON_C4,
        TXT_TXT_WEAPON_M4
    };
    static int      weaponPieceText[] = {
        TXT_TXT_QUIETUS_PIECE,
        TXT_TXT_WRAITHVERGE_PIECE,
        TXT_TXT_BLOODSCOURGE_PIECE
    };
    static int      pieceValueTrans[] = {
        0,                      // 0: never
        WPIECE1 | WPIECE2 | WPIECE3,    // WPIECE1 (1)
        WPIECE2 | WPIECE3,      // WPIECE2 (2)
        0,                      // 3: never
        WPIECE3                 // WPIECE3 (4)
    };

    remove = true;
    checkAssembled = true;
    gaveWeapon = false;
    if(plr->class != matchClass)
    {   // Wrong class, but try to pick up for mana.
        if(IS_NETGAME && !deathmatch)
        {   // Can't pick up wrong-class weapons in coop netplay.
            return;
        }

        checkAssembled = false;
        gaveMana =
            P_GiveMana(plr, AT_BLUEMANA, 20) + P_GiveMana(plr, AT_GREENMANA, 20);
        if(!gaveMana)
        {   // Didn't need the mana, so don't pick it up.
            return;
        }
    }
    else if(IS_NETGAME && !deathmatch)
    {   // Cooperative net-game.
        if(plr->pieces & pieceValue)
        {   // Already has the piece.
            return;
        }

        pieceValue = pieceValueTrans[pieceValue];
        P_GiveMana(plr, AT_BLUEMANA, 20);
        P_GiveMana(plr, AT_GREENMANA, 20);
        remove = false;
    }
    else
    {   // Deathmatch or single plr game.
        gaveMana =
            P_GiveMana(plr, AT_BLUEMANA, 20) + P_GiveMana(plr, AT_GREENMANA, 20);
        if(plr->pieces & pieceValue)
        {   // Already has the piece, check if mana needed.
            if(!gaveMana)
            {   // Didn't need the mana, so don't pick it up.
                return;
            }

            checkAssembled = false;
        }
    }

    // Pick up the weapon piece.
    if(pieceMobj->special)
    {
        P_ExecuteLineSpecial(pieceMobj->special, pieceMobj->args, NULL, 0,
                             plr->plr->mo);
        pieceMobj->special = 0;
    }

    if(remove)
    {
        if(deathmatch && !(pieceMobj->flags2 & MF2_DROPPED))
        {
            P_HideSpecialThing(pieceMobj);
        }
        else
        {
            P_MobjRemove(pieceMobj, false);
        }
    }

    plr->bonusCount += BONUSADD;
    ST_doPaletteStuff(plr - players, false);

    // Check if fourth weapon assembled.
    if(checkAssembled)
    {
        plr->pieces |= pieceValue;
        if(plr->pieces == (WPIECE1 | WPIECE2 | WPIECE3))
        {
            gaveWeapon = true;
            plr->weapons[WT_FOURTH].owned = true;
            plr->pendingWeapon = WT_FOURTH;
            plr->update |= PSF_WEAPONS | PSF_OWNED_WEAPONS;
        }
    }

    if(gaveWeapon)
    {
        P_SetMessage(plr, GET_TXT(fourthWeaponText[matchClass]), false);
        // Play the build-sound full volume for all players.
        S_StartSound(SFX_WEAPON_BUILD, NULL);

        // Should we change weapon automatically?
        P_MaybeChangeWeapon(plr, WT_FOURTH, AT_NOAMMO, false);
    }
    else
    {
        P_SetMessage(plr, GET_TXT(weaponPieceText[matchClass]), false);
        S_ConsoleSound(SFX_PICKUP_WEAPON, NULL, plr - players);
    }
}

/**
 * @returns             @c false, if the body isn't needed at all.
 */
boolean P_GiveBody(player_t* plr, int num)
{
    int                 max;

    if(plr->morphTics)
        max = MAXMORPHHEALTH;
    else
        max = maxHealth;

    if(plr->health >= max)
    {
        return false;
    }

    plr->health += num;
    if(plr->health > max)
    {
        plr->health = max;
    }
    plr->plr->mo->health = plr->health;
    plr->update |= PSF_HEALTH;

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_HEALTH);

    return true;
}

/**
 * @return              @c true, iff the armor was given.
 */
boolean P_GiveArmor(player_t* plr, armortype_t type, int points)
{
    if(plr->armorPoints[type] >= points)
        return false;

    plr->armorPoints[type] = points;
    plr->update |= PSF_ARMOR;

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_ARMOR);

    return true;
}

/**
 * @return              @c true, iff the armor was given.
 */
boolean P_GiveArmor2(player_t* plr, armortype_t type, int amount)
{
    int             hits, totalArmor;

    hits = amount * 5 * FRACUNIT;
    totalArmor =
        plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET] + PCLASS_INFO(plr->class)->autoArmorSave;
    if(totalArmor >= PCLASS_INFO(plr->class)->maxArmor * 5 * FRACUNIT)
        return false;

    plr->armorPoints[type] += hits;
    plr->update |= PSF_ARMOR;

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_ARMOR);

    return true;
}

int P_GiveKey(player_t *plr, keytype_t key)
{
    if(plr->keys & (1 << key))
    {
        return false;
    }

    plr->bonusCount += BONUSADD;
    plr->keys |= 1 << key;
    plr->update |= PSF_KEYS;

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_KEY);

    return true;
}

/**
 * @return              @c true, if power accepted.
 */
boolean P_GivePower(player_t *plr, powertype_t power)
{
    boolean     retval = false;

    plr->update |= PSF_POWERS;

    switch(power)
    {
    case PT_INVULNERABILITY:
        if(!(plr->powers[power] > BLINKTHRESHOLD))
        {
            plr->powers[power] = INVULNTICS;
            plr->plr->mo->flags2 |= MF2_INVULNERABLE;
            if(plr->class == PCLASS_MAGE)
            {
                plr->plr->mo->flags2 |= MF2_REFLECTIVE;
            }
            retval = true;
        }
        break;

    case PT_FLIGHT:
        if(!(plr->powers[power] > BLINKTHRESHOLD))
        {
            plr->powers[power] = FLIGHTTICS;
            plr->plr->mo->flags2 |= MF2_FLY;
            plr->plr->mo->flags |= MF_NOGRAVITY;
            if(plr->plr->mo->pos[VZ] <= plr->plr->mo->floorZ)
            {
                plr->flyHeight = 10; // Thrust the plr in the air a bit.
                plr->plr->flags |= DDPF_FIXMOM;
            }
            retval = true;
        }
        break;

    case PT_INFRARED:
        if(!(plr->powers[power] > BLINKTHRESHOLD))
        {
            plr->powers[power] = INFRATICS;
            retval = true;
        }
        break;

    case PT_SPEED:
        if(!(plr->powers[power] > BLINKTHRESHOLD))
        {
            plr->powers[power] = SPEEDTICS;
            retval = true;
        }
        break;

    case PT_MINOTAUR:
        // Doesn't matter if already have power, renew ticker.
        plr->powers[power] = MAULATORTICS;
        retval = true;
        break;

    default:
        if(!(plr->powers[power]))
        {
            plr->powers[power] = 1;
            retval = true;
        }
        break;
    }

    if(retval)
    {
        // Maybe unhide the HUD?
        ST_HUDUnHide(plr - players, HUE_ON_PICKUP_POWER);
    }

    return retval;
}

static void TryPickupArtifact(player_t *plr, artitype_e artifactType,
                              mobj_t *artifact)
{
    int             artifactMessages[NUM_ARTIFACT_TYPES] = {
        0,
        TXT_TXT_ARTIINVULNERABILITY,
        TXT_TXT_ARTIHEALTH,
        TXT_TXT_ARTISUPERHEALTH,
        TXT_TXT_ARTIHEALINGRADIUS,
        TXT_TXT_ARTISUMMON,
        TXT_TXT_ARTITORCH,
        TXT_TXT_ARTIEGG,
        TXT_TXT_ARTIFLY,
        TXT_TXT_ARTIBLASTRADIUS,
        TXT_TXT_ARTIPOISONBAG,
        TXT_TXT_ARTITELEPORTOTHER,
        TXT_TXT_ARTISPEED,
        TXT_TXT_ARTIBOOSTMANA,
        TXT_TXT_ARTIBOOSTARMOR,
        TXT_TXT_ARTITELEPORT,
        TXT_TXT_ARTIPUZZSKULL,
        TXT_TXT_ARTIPUZZGEMBIG,
        TXT_TXT_ARTIPUZZGEMRED,
        TXT_TXT_ARTIPUZZGEMGREEN1,
        TXT_TXT_ARTIPUZZGEMGREEN2,
        TXT_TXT_ARTIPUZZGEMBLUE1,
        TXT_TXT_ARTIPUZZGEMBLUE2,
        TXT_TXT_ARTIPUZZBOOK1,
        TXT_TXT_ARTIPUZZBOOK2,
        TXT_TXT_ARTIPUZZSKULL2,
        TXT_TXT_ARTIPUZZFWEAPON,
        TXT_TXT_ARTIPUZZCWEAPON,
        TXT_TXT_ARTIPUZZMWEAPON,
        TXT_TXT_ARTIPUZZGEAR, // All gear pickups use the same text.
        TXT_TXT_ARTIPUZZGEAR,
        TXT_TXT_ARTIPUZZGEAR,
        TXT_TXT_ARTIPUZZGEAR
    };

    if(P_InventoryGive(plr, artifactType))
    {
        if(artifact->special)
        {
            P_ExecuteLineSpecial(artifact->special, artifact->args, NULL, 0,
                                 NULL);
            artifact->special = 0;
        }

        plr->bonusCount += BONUSADD;
        if(artifactType < AFT_FIRSTPUZZITEM)
        {
            SetDormantArtifact(artifact);
            S_StartSound(SFX_PICKUP_ARTIFACT, artifact);
            P_SetMessage(plr, GET_TXT(artifactMessages[artifactType]), false);
        }
        else
        {   // Puzzle item.
            S_StartSound(SFX_PICKUP_ITEM, NULL);
            P_SetMessage(plr, GET_TXT(artifactMessages[artifactType]), false);
            if(!IS_NETGAME || deathmatch)
            {   // Remove puzzle items if not cooperative netplay.
                P_MobjRemove(artifact, false);
            }
        }
    }
}

/**
 * Removes the MF_SPECIAL flag and initiates the artifact pickup animation.
 */
static void SetDormantArtifact(mobj_t *arti)
{
    arti->flags &= ~MF_SPECIAL;
    if(deathmatch && !(arti->flags2 & MF2_DROPPED))
    {
        if(arti->type == MT_ARTIINVULNERABILITY)
        {
            P_MobjChangeState(arti, S_DORMANTARTI3_1);
        }
        else if(arti->type == MT_SUMMONMAULATOR || arti->type == MT_ARTIFLY)
        {
            P_MobjChangeState(arti, S_DORMANTARTI2_1);
        }
        else
        {
            P_MobjChangeState(arti, S_DORMANTARTI1_1);
        }
    }
    else
    {   // Don't respawn.
        P_MobjChangeState(arti, S_DEADARTI1);
    }
}

void C_DECL A_RestoreArtifact(mobj_t *arti)
{
    arti->flags |= MF_SPECIAL;
    P_MobjChangeState(arti, arti->info->spawnState);
    S_StartSound(SFX_RESPAWN, arti);
}

/**
 * Make a special thing visible again.
 */
void C_DECL A_RestoreSpecialThing1(mobj_t *thing)
{
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
    player_t       *player;
    float           delta;
    int             sound;
    boolean         respawn;

    if(IS_CLIENT)
        return;

    delta = special->pos[VZ] - toucher->pos[VZ];
    if(delta > toucher->height || delta < -32)
    {   // Out of reach.
        return;
    }

    if(toucher->health <= 0)
    {   // Toucher is dead.
        return;
    }

    sound = SFX_PICKUP_ITEM;
    player = toucher->player;
    if(player == NULL)
        return;

    respawn = true;
    switch(special->sprite)
    {
    // Items
    case SPR_PTN1: // Item_HealingPotion.
        if(!P_GiveBody(player, 10))
            return;

        P_SetMessage(player, TXT_ITEMHEALTH, false);
        break;

    case SPR_ARM1:
        if(!P_GiveArmor(player, ARMOR_ARMOR,
                        PCLASS_INFO(player->class)->armorIncrement[ARMOR_ARMOR]))
            return;

        P_SetMessage(player, TXT_ARMOR1, false);
        break;

    case SPR_ARM2:
        if(!P_GiveArmor(player, ARMOR_SHIELD,
                        PCLASS_INFO(player->class)->armorIncrement[ARMOR_SHIELD]))
            return;

        P_SetMessage(player, TXT_ARMOR2, false);
        break;

    case SPR_ARM3:
        if(!P_GiveArmor(player, ARMOR_HELMET,
                        PCLASS_INFO(player->class)->armorIncrement[ARMOR_HELMET]))
            return;

        P_SetMessage(player, TXT_ARMOR3, false);
        break;

    case SPR_ARM4:
        if(!P_GiveArmor(player, ARMOR_AMULET,
                        PCLASS_INFO(player->class)->armorIncrement[ARMOR_AMULET]))
            return;

        P_SetMessage(player, TXT_ARMOR4, false);
        break;

    // Keys
    case SPR_KEY1:
    case SPR_KEY2:
    case SPR_KEY3:
    case SPR_KEY4:
    case SPR_KEY5:
    case SPR_KEY6:
    case SPR_KEY7:
    case SPR_KEY8:
    case SPR_KEY9:
    case SPR_KEYA:
    case SPR_KEYB:
        if(!P_GiveKey(player, special->sprite - SPR_KEY1))
            return;

        P_SetMessage(player,
                     GET_TXT(TextKeyMessages[special->sprite - SPR_KEY1]),
                     false);
        sound = SFX_PICKUP_KEY;

        // Check and process the special now in case the key doesn't
        // get removed for coop netplay
        if(special->special)
        {
            P_ExecuteLineSpecial(special->special, special->args, NULL, 0,
                                 toucher);
            special->special = 0;
        }

        if(!IS_NETGAME)
        {   // Only remove keys in single player game.
            break;
        }
        player->bonusCount += BONUSADD;
        S_ConsoleSound(sound, NULL, player - players);
        ST_doPaletteStuff(player - players, false);
        return;

    // Artifacts
    case SPR_PTN2:
        TryPickupArtifact(player, AFT_HEALTH, special);
        return;

    case SPR_SOAR:
        TryPickupArtifact(player, AFT_FLY, special);
        return;

    case SPR_INVU:
        TryPickupArtifact(player, AFT_INVULNERABILITY, special);
        return;

    case SPR_SUMN:
        TryPickupArtifact(player, AFT_SUMMON, special);
        return;

    case SPR_PORK:
        TryPickupArtifact(player, AFT_EGG, special);
        return;

    case SPR_SPHL:
        TryPickupArtifact(player, AFT_SUPERHEALTH, special);
        return;

    case SPR_HRAD:
        TryPickupArtifact(player, AFT_HEALINGRADIUS, special);
        return;

    case SPR_TRCH:
        TryPickupArtifact(player, AFT_TORCH, special);
        return;

    case SPR_ATLP:
        TryPickupArtifact(player, AFT_TELEPORT, special);
        return;

    case SPR_TELO:
        TryPickupArtifact(player, AFT_TELEPORTOTHER, special);
        return;

    case SPR_PSBG:
        TryPickupArtifact(player, AFT_POISONBAG, special);
        return;

    case SPR_SPED:
        TryPickupArtifact(player, AFT_SPEED, special);
        return;

    case SPR_BMAN:
        TryPickupArtifact(player, AFT_BOOSTMANA, special);
        return;

    case SPR_BRAC:
        TryPickupArtifact(player, AFT_BOOSTARMOR, special);
        return;

    case SPR_BLST:
        TryPickupArtifact(player, AFT_BLASTRADIUS, special);
        return;

    // Puzzle artifacts
    case SPR_ASKU:
        TryPickupArtifact(player, AFT_PUZZSKULL, special);
        return;

    case SPR_ABGM:
        TryPickupArtifact(player, AFT_PUZZGEMBIG, special);
        return;

    case SPR_AGMR:
        TryPickupArtifact(player, AFT_PUZZGEMRED, special);
        return;

    case SPR_AGMG:
        TryPickupArtifact(player, AFT_PUZZGEMGREEN1, special);
        return;

    case SPR_AGG2:
        TryPickupArtifact(player, AFT_PUZZGEMGREEN2, special);
        return;

    case SPR_AGMB:
        TryPickupArtifact(player, AFT_PUZZGEMBLUE1, special);
        return;

    case SPR_AGB2:
        TryPickupArtifact(player, AFT_PUZZGEMBLUE2, special);
        return;

    case SPR_ABK1:
        TryPickupArtifact(player, AFT_PUZZBOOK1, special);
        return;

    case SPR_ABK2:
        TryPickupArtifact(player, AFT_PUZZBOOK2, special);
        return;

    case SPR_ASK2:
        TryPickupArtifact(player, AFT_PUZZSKULL2, special);
        return;

    case SPR_AFWP:
        TryPickupArtifact(player, AFT_PUZZFWEAPON, special);
        return;

    case SPR_ACWP:
        TryPickupArtifact(player, AFT_PUZZCWEAPON, special);
        return;

    case SPR_AMWP:
        TryPickupArtifact(player, AFT_PUZZMWEAPON, special);
        return;

    case SPR_AGER:
        TryPickupArtifact(player, AFT_PUZZGEAR1, special);
        return;

    case SPR_AGR2:
        TryPickupArtifact(player, AFT_PUZZGEAR2, special);
        return;

    case SPR_AGR3:
        TryPickupArtifact(player, AFT_PUZZGEAR3, special);
        return;

    case SPR_AGR4:
        TryPickupArtifact(player, AFT_PUZZGEAR4, special);
        return;

    // Mana
    case SPR_MAN1:
        if(!P_GiveMana(player, AT_BLUEMANA, 15))
            return;

        P_SetMessage(player, TXT_MANA_1, false);
        break;

    case SPR_MAN2:
        if(!P_GiveMana(player, AT_GREENMANA, 15))
            return;

        P_SetMessage(player, TXT_MANA_2, false);
        break;

    case SPR_MAN3: // Double Mana Dodecahedron.
        if(!P_GiveMana(player, AT_BLUEMANA, 20))
        {
            if(!P_GiveMana(player, AT_GREENMANA, 20))
                return;
        }
        else
        {
            P_GiveMana(player, AT_GREENMANA, 20);
        }
        P_SetMessage(player, TXT_MANA_BOTH, false);
        break;

    // 2nd and 3rd Mage Weapons
    case SPR_WMCS: // Frost Shards.
        TryPickupWeapon(player, PCLASS_MAGE, WT_SECOND, special,
                        TXT_WEAPON_M2);
        return;

    case SPR_WMLG: // Arc of Death.
        TryPickupWeapon(player, PCLASS_MAGE, WT_THIRD, special, TXT_WEAPON_M3);
        return;

    // 2nd and 3rd Fighter Weapons
    case SPR_WFAX: // Timon's Axe.
        TryPickupWeapon(player, PCLASS_FIGHTER, WT_SECOND, special,
                        TXT_WEAPON_F2);
        return;

    case SPR_WFHM: // Hammer of Retribution.
        TryPickupWeapon(player, PCLASS_FIGHTER, WT_THIRD, special,
                        TXT_WEAPON_F3);
        return;

    // 2nd and 3rd Cleric Weapons
    case SPR_WCSS: // Serpent Staff.
        TryPickupWeapon(player, PCLASS_CLERIC, WT_SECOND, special,
                        TXT_WEAPON_C2);
        return;

    case SPR_WCFM: // Firestorm.
        TryPickupWeapon(player, PCLASS_CLERIC, WT_THIRD, special,
                        TXT_WEAPON_C3);
        return;

    // Fourth Weapon Pieces.
    case SPR_WFR1:
        TryPickupWeaponPiece(player, PCLASS_FIGHTER, WPIECE1, special);
        return;

    case SPR_WFR2:
        TryPickupWeaponPiece(player, PCLASS_FIGHTER, WPIECE2, special);
        return;

    case SPR_WFR3:
        TryPickupWeaponPiece(player, PCLASS_FIGHTER, WPIECE3, special);
        return;

    case SPR_WCH1:
        TryPickupWeaponPiece(player, PCLASS_CLERIC, WPIECE1, special);
        return;

    case SPR_WCH2:
        TryPickupWeaponPiece(player, PCLASS_CLERIC, WPIECE2, special);
        return;

    case SPR_WCH3:
        TryPickupWeaponPiece(player, PCLASS_CLERIC, WPIECE3, special);
        return;

    case SPR_WMS1:
        TryPickupWeaponPiece(player, PCLASS_MAGE, WPIECE1, special);
        return;

    case SPR_WMS2:
        TryPickupWeaponPiece(player, PCLASS_MAGE, WPIECE2, special);
        return;

    case SPR_WMS3:
        TryPickupWeaponPiece(player, PCLASS_MAGE, WPIECE3, special);
        return;

    default:
        Con_Error("P_SpecialThing: Unknown gettable thing");
    }

    if(special->special)
    {
        P_ExecuteLineSpecial(special->special, special->args, NULL, 0,
                             toucher);
        special->special = 0;
    }

    if(deathmatch && respawn && !(special->flags2 & MF2_DROPPED))
    {
        P_HideSpecialThing(special);
    }
    else
    {
        P_MobjRemove(special, false);
    }

    player->bonusCount += BONUSADD;
    S_ConsoleSound(sound, NULL, player - players);
    ST_doPaletteStuff(player - players, false);
}

typedef struct {
    player_t*           master;
    mobj_t*             foundMobj;
} findactiveminotaurparams_t;

static boolean findActiveMinotaur(thinker_t* th, void* context)
{
    findactiveminotaurparams_t* params =
        (findactiveminotaurparams_t*) context;
    mobj_t*             mo = (mobj_t *) th;

    if(mo->type != MT_MINOTAUR)
        return true; // Continue iteration.
    if(mo->health <= 0)
        return true; // Continue iteration.
    if(!(mo->flags & MF_COUNTKILL)) // For morphed minotaurs.
        return true; // Continue iteration.
    if(mo->flags & MF_CORPSE)
        return true; // Continue iteration.

    if((mapTime - *((unsigned int *) mo->args)) >= MAULATORTICS)
        return true; // Continue iteration.

    if(mo->tracer->player == params->master)
    {   // Found it!
        params->foundMobj = mo;
        return false; // Stop iteration.
    }

    return true; // Continue iteration.
}

mobj_t* ActiveMinotaur(player_t* master)
{
    findactiveminotaurparams_t params;

    params.master = master;
    params.foundMobj = NULL;

    if(!P_IterateThinkers(P_MobjThinker, findActiveMinotaur, &params))
        return params.foundMobj;

    return NULL;
}

void P_KillMobj(mobj_t *source, mobj_t *target)
{
    int             dummy;
    mobj_t         *master;

    if(!target)
        return; // Nothing to kill.

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_NOGRAVITY);
    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->flags2 &= ~MF2_PASSMOBJ;
    target->height /= 2*2;
    if((target->flags & MF_COUNTKILL || target->type == MT_ZBELL) &&
       target->special)
    {   // Initiate monster death actions.
        if(target->type == MT_SORCBOSS)
        {
            dummy = 0;
            P_StartACS(target->special, 0, (byte *) &dummy, target, NULL, 0);
        }
        else
        {
            P_ExecuteLineSpecial(target->special, target->args, NULL, 0,
                                 target);
        }
    }

    if(source && source->player)
    {   // Check for frag changes.
        if(target->player && deathmatch)
        {
            if(target == source)
            {   // Self-frag.
                target->player->frags[target->player - players]--;
                NetSv_FragsForAll(target->player);
            }
            else
            {
                source->player->frags[target->player - players]++;
                NetSv_FragsForAll(source->player);
            }
        }
    }

    if(target->player)
    {   // Player death.
        if(!source)
        {   // Self-frag
            target->player->frags[target->player - players]--;
            NetSv_FragsForAll(target->player);
        }

        target->flags &= ~MF_SOLID;
        target->flags2 &= ~MF2_FLY;
        target->player->powers[PT_FLIGHT] = 0;
        target->player->playerState = PST_DEAD;
        target->player->rebornWait = PLAYER_REBORN_TICS;
        target->player->update |= PSF_STATE | PSF_POWERS;

        // Let the engine know about this, too. The DEAD flag will be
        // cleared when the player is reborn.
        target->player->plr->flags |= DDPF_DEAD;
        P_DropWeapon(target->player);
        if(target->flags2 & MF2_FIREDAMAGE)
        {   // Player flame death.
            //// \todo Should be pulled from the player class definition.
            switch(target->player->class)
            {
            case PCLASS_FIGHTER:
                S_StartSound(SFX_PLAYER_FIGHTER_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_F_FDTH1);
                return;

            case PCLASS_CLERIC:
                S_StartSound(SFX_PLAYER_CLERIC_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_C_FDTH1);
                return;

            case PCLASS_MAGE:
                S_StartSound(SFX_PLAYER_MAGE_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_M_FDTH1);
                return;

            default:
                break;
            }
        }

        if(target->flags2 & MF2_ICEDAMAGE)
        {   // Player ice death.
            target->flags &= ~(7 << MF_TRANSSHIFT); //no translation
            target->flags |= MF_ICECORPSE;
            //// \todo Should be pulled from the player class definition.
            switch(target->player->class)
            {
            case PCLASS_FIGHTER:
                P_MobjChangeState(target, S_FPLAY_ICE);
                return;

            case PCLASS_CLERIC:
                P_MobjChangeState(target, S_CPLAY_ICE);
                return;

            case PCLASS_MAGE:
                P_MobjChangeState(target, S_MPLAY_ICE);
                return;

            case PCLASS_PIG:
                P_MobjChangeState(target, S_PIG_ICE);
                return;

            default:
                break;
            }
        }

        // Don't die with the automap open.
        AM_Open(target->player - players, false, false);
    }
    else
    {   // Target is some monster or an object.

        /**
         * mobj death, record as player's kill in netgame + coop could not
         * find MF_ targets->flags that indicated *only* enemies (not trees,
         * pots, etc), so built a list.
         *
         * \todo This should be a Thing definition flag.
         */
        if(IS_NETGAME && !deathmatch && source && source->player &&
           source->player->plr && (target->type == MT_CENTAUR ||
                                   target->type == MT_CENTAURLEADER ||
                                   target->type == MT_DEMON ||
                                   target->type == MT_DEMON2 ||
                                   target->type == MT_ETTIN ||
                                   target->type == MT_PIG ||
                                   target->type == MT_FIREDEMON ||
                                   target->type == MT_SERPENT ||
                                   target->type == MT_SERPENTLEADER ||
                                   target->type == MT_WRAITH ||
                                   target->type == MT_WRAITHB ||
                                   target->type == MT_BISHOP ||
                                   target->type == MT_ICEGUY ||
                                   target->type == MT_FIGHTER_BOSS ||
                                   target->type == MT_CLERIC_BOSS ||
                                   target->type == MT_MAGE_BOSS ||
                                   target->type == MT_MINOTAUR))
        {
            source->player->frags[0]++;
        }
    }

    if(target->flags2 & MF2_FIREDAMAGE)
    {
        if(target->type == MT_FIGHTER_BOSS || target->type == MT_CLERIC_BOSS ||
           target->type == MT_MAGE_BOSS)
        {
            switch(target->type)
            {
            case MT_FIGHTER_BOSS:
                S_StartSound(SFX_PLAYER_FIGHTER_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_F_FDTH1);
                return;

            case MT_CLERIC_BOSS:
                S_StartSound(SFX_PLAYER_CLERIC_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_C_FDTH1);
                return;

            case MT_MAGE_BOSS:
                S_StartSound(SFX_PLAYER_MAGE_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_M_FDTH1);
                return;

            default:
                break;
            }
        }
        else if(target->type == MT_TREEDESTRUCTIBLE)
        {
            P_MobjChangeState(target, S_ZTREEDES_X1);
            target->height = 24;
            S_StartSound(SFX_TREE_EXPLODE, target);
            return;
        }
    }

    if(target->flags2 & MF2_ICEDAMAGE)
    {
        target->flags |= MF_ICECORPSE;
        switch(target->type)
        {
        case MT_BISHOP:
            P_MobjChangeState(target, S_BISHOP_ICE);
            return;

        case MT_CENTAUR:
        case MT_CENTAURLEADER:
            P_MobjChangeState(target, S_CENTAUR_ICE);
            return;

        case MT_DEMON:
        case MT_DEMON2:
            P_MobjChangeState(target, S_DEMON_ICE);
            return;

        case MT_SERPENT:
        case MT_SERPENTLEADER:
            P_MobjChangeState(target, S_SERPENT_ICE);
            return;

        case MT_WRAITH:
        case MT_WRAITHB:
            P_MobjChangeState(target, S_WRAITH_ICE);
            return;

        case MT_ETTIN:
            P_MobjChangeState(target, S_ETTIN_ICE1);
            return;

        case MT_FIREDEMON:
            P_MobjChangeState(target, S_FIRED_ICE1);
            return;

        case MT_FIGHTER_BOSS:
            P_MobjChangeState(target, S_FIGHTER_ICE);
            return;

        case MT_CLERIC_BOSS:
            P_MobjChangeState(target, S_CLERIC_ICE);
            return;

        case MT_MAGE_BOSS:
            P_MobjChangeState(target, S_MAGE_ICE);
            return;

        case MT_PIG:
            P_MobjChangeState(target, S_PIG_ICE);
            return;

        default:
            target->flags &= ~MF_ICECORPSE;
            break;
        }
    }

    if(target->type == MT_MINOTAUR)
    {
        master = target->tracer;
        if(master && master->health > 0)
        {
            if(!ActiveMinotaur(master->player))
            {
                master->player->powers[PT_MINOTAUR] = 0;
            }
        }
    }
    else if(target->type == MT_TREEDESTRUCTIBLE)
    {
        target->height = 24;
    }

    if(target->health < -(target->info->spawnHealth / 2) &&
       target->info->xDeathState)
    {   // Extreme death.
        P_MobjChangeState(target, target->info->xDeathState);
    }
    else
    {   // Normal death.
        if((target->type == MT_FIREDEMON) &&
           (target->pos[VZ] <= target->floorZ + 2) &&
           (target->info->xDeathState))
        {
            // This is to fix the imps' staying in fall state.
            P_MobjChangeState(target, target->info->xDeathState);
        }
        else
        {
            P_MobjChangeState(target, target->info->deathState);
        }
    }

    target->tics -= P_Random() & 3;
}

/**
 * @return              @c true, if the player gets turned into a pig.
 */
boolean P_MorphPlayer(player_t *player)
{
    mobj_t         *pmo, *fog, *beastMo;
    float           pos[3];
    angle_t         angle;
    int             oldFlags2;

    if(player->powers[PT_INVULNERABILITY])
        return false; // Immune when invulnerable.

    if(player->morphTics)
        return false; // Player is already morphed.

    pmo = player->plr->mo;

    pos[VX] = pmo->pos[VX];
    pos[VY] = pmo->pos[VY];
    pos[VZ] = pmo->pos[VZ];
    angle = pmo->angle;
    oldFlags2 = pmo->flags2;
    P_MobjChangeState(pmo, S_FREETARGMOBJ);

    fog = P_SpawnMobj3f(MT_TFOG, pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT,
                        angle + ANG180);
    S_StartSound(SFX_TELEPORT, fog);

    beastMo = P_SpawnMobj3fv(MT_PIGPLAYER, pos, angle);
    beastMo->special1 = player->readyWeapon;
    beastMo->player = player;
    beastMo->dPlayer = player->plr;

    player->health = beastMo->health = MAXMORPHHEALTH;
    player->plr->mo = beastMo;
    memset(&player->armorPoints[0], 0, NUMARMOR * sizeof(int));
    player->class = PCLASS_PIG;

    if(oldFlags2 & MF2_FLY)
        beastMo->flags2 |= MF2_FLY;

    player->morphTics = MORPHTICS;
    player->update |= PSF_MORPH_TIME | PSF_HEALTH;
    player->plr->flags |= DDPF_FIXPOS | DDPF_FIXMOM;
    P_ActivateMorphWeapon(player);
    return true;
}

boolean P_MorphMonster(mobj_t *actor)
{
    mobj_t*         master, *monster, *fog;
    mobjtype_t      moType;
    float           pos[3];
    mobj_t          oldMonster;
    angle_t         oldAngle;

    if(actor->player)
        return (false);
    if(!(actor->flags & MF_COUNTKILL))
        return false;
    if(actor->flags2 & MF2_BOSS)
        return false;

    moType = actor->type;
    switch(moType)
    {
    case MT_PIG:
    case MT_FIGHTER_BOSS:
    case MT_CLERIC_BOSS:
    case MT_MAGE_BOSS:
        return false;

    default:
        break;
    }

    //// \fixme Do this properly!
    oldMonster = *actor;

    pos[VX] = actor->pos[VX];
    pos[VY] = actor->pos[VY];
    pos[VZ] = actor->pos[VZ];
    oldAngle = actor->angle;
    P_MobjRemoveFromTIDList(actor);
    P_MobjChangeState(actor, S_FREETARGMOBJ);

    fog = P_SpawnMobj3f(MT_TFOG, pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT,
                        oldAngle + ANG180);
    S_StartSound(SFX_TELEPORT, fog);

    monster = P_SpawnMobj3fv(MT_PIG, pos, oldMonster.angle);
    monster->special2 = moType;
    monster->special1 = MORPHTICS + P_Random();
    monster->flags |= (oldMonster.flags & MF_SHADOW);
    monster->target = oldMonster.target;
    monster->tid = oldMonster.tid;
    monster->special = oldMonster.special;
    P_MobjInsertIntoTIDList(monster, oldMonster.tid);
    memcpy(monster->args, oldMonster.args, 5);

    // Check for turning off minotaur power for active icon.
    if(moType == MT_MINOTAUR)
    {
        master = oldMonster.tracer;
        if(master->health > 0)
        {
            if(!ActiveMinotaur(master->player))
            {
                master->player->powers[PT_MINOTAUR] = 0;
            }
        }
    }

    return true;
}

void P_AutoUseHealth(player_t *player, int saveHealth)
{
    int             i, count, normalCount, superCount;
    int             normalSlot = 0, superSlot = 0;

    normalCount = superCount = 0;
    for(i = 0; i < player->inventorySlotNum; ++i)
    {
        if(player->inventory[i].type == AFT_HEALTH)
        {
            normalSlot = i;
            normalCount = player->inventory[i].count;
        }
        else if(player->inventory[i].type == AFT_SUPERHEALTH)
        {
            superSlot = i;
            superCount = player->inventory[i].count;
        }
    }

    if((gameSkill == SM_BABY) && (normalCount * 25 >= saveHealth))
    {   // Use quartz flasks.
        count = (saveHealth + 24) / 25;
        for(i = 0; i < count; i++)
        {
            player->health += 25;
            P_InventoryTake(player, normalSlot);
        }
    }
    else if(superCount * 100 >= saveHealth)
    {   // Use mystic urns.
        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; i++)
        {
            player->health += 100;
            P_InventoryTake(player, superSlot);
        }
    }
    else if((gameSkill == SM_BABY) &&
            (superCount * 100 + normalCount * 25 >= saveHealth))
    {   // Use mystic urns and quartz flasks.
        count = (saveHealth + 24) / 25;
        saveHealth -= count * 25;
        for(i = 0; i < count; ++i)
        {
            player->health += 25;
            P_InventoryTake(player, normalSlot);
        }

        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; ++i)
        {
            player->health += 100;
            P_InventoryTake(player, normalSlot);
        }
    }
    player->plr->mo->health = player->health;
}

/**
 * Sets up all data concerning poisoning.
 */
void P_PoisonPlayer(player_t *player, mobj_t *poisoner, int poison)
{
    if((P_GetPlayerCheats(player) & CF_GODMODE) ||
       player->powers[PT_INVULNERABILITY])
        return;

    player->poisonCount += poison;
    player->poisoner = poisoner;

    if(player->poisonCount > 100)
        player->poisonCount = 100;
}

/**
 * Damages both enemies and players
 * \note 'source' and 'inflictor' are the same for melee attacks.
 * 'source' can be NULL for slime, barrel explosions and other environmental
 * stuff.
 *
 * @param inflictor         Is the mobj that caused the damage, creature or
 *                          missile, can be @c NULL (slime, etc).
 * @param source            Is the mobj to target after taking damage
 *                          creature or @c NULL.
 */
int P_DamageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source,
                 int damageP, boolean stomping)
{
    uint            an;
    angle_t         angle;
    int             i, temp, originalHealth;
    float           thrust, saved, savedPercent;
    player_t*       player;
    mobj_t*         master;
    int             damage;

    if(!target)
        return 0; // Wha?

    originalHealth = target->health;

    // The actual damage (== damageP * netMobDamageModifier for any
    // non-player mobj).
    damage = damageP;

    if(IS_NETGAME && !stomping &&
       D_NetDamageMobj(target, inflictor, source, damage))
    {   // We're done here.
        return 0;
    }

    // Clients can't harm anybody.
    if(IS_CLIENT)
        return 0;

    if(!(target->flags & MF_SHOOTABLE))
        return 0; // Shouldn't happen.

    if(target->health <= 0)
    {
        if(!(inflictor && (inflictor->flags2 & MF2_ICEDAMAGE)) &&
           (target->flags & MF_ICECORPSE))
        {   // Frozen.
            target->tics = 1;
            target->mom[MX] = target->mom[MY] = 0;
        }

        return 0;
    }

    if((target->flags2 & MF2_INVULNERABLE) && damage < 10000)
    {   // mobj is invulnerable.
        if(target->player)
            return 0; // for player, no exceptions.

        if(!inflictor)
            return 0;

        switch(inflictor->type)
        {
        // These inflictors aren't foiled by invulnerability.
        case MT_HOLY_FX:
        case MT_POISONCLOUD:
        case MT_FIREBOMB:
            break;

        default:
            return 0;
        }
    }

    if(target->player)
    {   // Player specific.
        // Check if player-player damage is disabled.
        if(source && source->player && source->player != target->player)
        {
#if 0
            // Co-op damage disabled?
            if(IS_NETGAME && !deathmatch && cfg.noCoopDamage)
                return 0;

            // Same color, no damage?
            if(cfg.noTeamDamage &&
               cfg.playerColor[target->player - players] ==
               cfg.playerColor[source->player - players])
                return 0;
#endif
        }
    }

    if(target->flags & MF_SKULLFLY)
    {
        target->mom[MX] = target->mom[MY] = target->mom[MZ] = 0;
    }

    if(target->flags2 & MF2_DORMANT)
        return 0; // Invulnerable, and won't wake up.

    player = target->player;
    if(player && gameSkill == SM_BABY)
        damage /= 2; // Take half damage in trainer mode.

    // Use the cvar damage multiplier netMobDamageModifier only if the
    // inflictor is not a player.
    if(inflictor && !inflictor->player &&
       (!source || (source && !source->player)))
    {
        // damage = (int) ((float) damage * netMobDamageModifier);
        if(IS_NETGAME)
            damage *= cfg.netMobDamageModifier;
    }

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
            return 0; // Does no actual "damage" but health IS modified.

        case MT_TELOTHER_FX1:
        case MT_TELOTHER_FX2:
        case MT_TELOTHER_FX3:
        case MT_TELOTHER_FX4:
        case MT_TELOTHER_FX5:
            if((target->flags & MF_COUNTKILL) && (target->type != MT_SERPENT)
               && (target->type != MT_SERPENTLEADER) &&
               (!(target->flags2 & MF2_BOSS)))
            {
                if(target->player)
                {
                    if(deathmatch)
                        P_TeleportToDeathmatchStarts(target);
                    else
                        P_TeleportToPlayerStarts(target);
                }
                else
                {
                    // If death action, run it upon teleport.
                    if(target->flags & MF_COUNTKILL && target->special)
                    {
                        P_MobjRemoveFromTIDList(target);
                        P_ExecuteLineSpecial(target->special, target->args, NULL, 0,
                                             target);
                        target->special = 0;
                    }

                    // Send all monsters to deathmatch spots.
                    P_TeleportToDeathmatchStarts(target);
                }
            }
            return 0;

        case MT_MINOTAUR:
            if(inflictor->flags & MF_SKULLFLY)
            {   // Slam only when in charge mode.
                uint            an;
                int             damageDone;
                angle_t         angle;
                float           thrust;

                angle = R_PointToAngle2(inflictor->pos[VX], inflictor->pos[VY],
                                        target->pos[VX], target->pos[VY]);

                an = angle >> ANGLETOFINESHIFT;
                thrust = 16 + FIX2FLT(P_Random() << 10);
                target->mom[MX] += thrust * FIX2FLT(finecosine[an]);
                target->mom[MY] += thrust * FIX2FLT(finesine[an]);
                damageDone = P_DamageMobj(target, NULL, inflictor, HITDICE(4), false);
                if(target->player)
                {
                    target->reactionTime = 14 + (P_Random() & 7);
                }

                inflictor->args[0] = 0; // Stop charging.
                return damageDone;
            }
            break;

        case MT_BISH_FX:
            // Bishops are just too nasty.
            damage /= 2;
            break;

        case MT_SHARDFX1:
            switch(inflictor->special2)
            {
            case 3:
                damage *= 8;
                break;

            case 2:
                damage *= 4;
                break;

            case 1:
                damage *= 2;
                break;

            default:
                break;
            }
            break;

        case MT_CSTAFF_MISSILE:
            // Cleric Serpent Staff does poison damage.
            if(target->player)
            {
                P_PoisonPlayer(target->player, source, 20);
                damage /= 2;
            }
            break;

        case MT_ICEGUY_FX2:
            damage /= 2;
            break;

        case MT_POISONDART:
            if(target->player)
            {
                P_PoisonPlayer(target->player, source, 20);
                damage /= 2;
            }
            break;

        case MT_POISONCLOUD:
            if(target->player)
            {
                int                 damageDone = 0;

                if(target->player->poisonCount < 4)
                {
                    damageDone = P_PoisonDamage(target->player, source, 15 + (P_Random() & 15), false);  // Don't play painsound
                    P_PoisonPlayer(target->player, source, 50);
                    S_StartSound(SFX_PLAYER_POISONCOUGH, target);
                }

                return damageDone;
            }
            else if(!(target->flags & MF_COUNTKILL))
            {   // Only damage monsters/players with the poison cloud.
                return 0;
            }
            break;

        case MT_FSWORD_MISSILE:
            if(target->player)
            {
                damage -= damage / 4;
            }
            break;

        default:
            break;
        }
    }

    // Some close combat weapons should not inflict thrust and push the
    // victim out of reach, thus kick away unless using a melee weapon.
    if(inflictor && (!source || !source->player) &&
       !(inflictor->flags2 & MF2_NODMGTHRUST))
    {
        angle =
            R_PointToAngle2(inflictor->pos[VX], inflictor->pos[VY],
                            target->pos[VX], target->pos[VY]);

        if(!target->info->mass)
            Con_Error("P_DamageMobj: No target->info->mass!\n");

        thrust = FIX2FLT(damage * (FRACUNIT>>3) * 100 / target->info->mass);

        // Make fall forwards sometimes.
        if((damage < 40) && (damage > target->health) &&
           (target->pos[VZ] - inflictor->pos[VZ] > 64) && (P_Random() & 1))
        {
            angle += ANG180;
            thrust *= 4;
        }

        an = angle >>ANGLETOFINESHIFT;
        target->mom[MX] += thrust * FIX2FLT(finecosine[an]);
        target->mom[MY] += thrust * FIX2FLT(finesine[an]);
        if(target->dPlayer)
        {
            // Only fix momentum. Otherwise clients will find it difficult
            // to escape from the damage inflictor.
            target->dPlayer->flags |= DDPF_FIXMOM;
        }
    }

    // Player specific.
    if(player)
    {
        target->player->update |= PSF_HEALTH;

        if(damage < 1000 &&
           ((P_GetPlayerCheats(target->player) & CF_GODMODE) ||
            target->player->powers[PT_INVULNERABILITY]))
        {
            return 0;
        }

        savedPercent = FIX2FLT(
            PCLASS_INFO(player->class)->autoArmorSave + player->armorPoints[ARMOR_ARMOR] +
            player->armorPoints[ARMOR_SHIELD] +
            player->armorPoints[ARMOR_HELMET] +
            player->armorPoints[ARMOR_AMULET]);
        if(savedPercent)
        {   // Armor absorbed some damage.
            if(savedPercent > 100)
            {
                savedPercent = 100;
            }

            for(i = 0; i < NUMARMOR; ++i)
            {
                if(player->armorPoints[i])
                {
                    player->armorPoints[i] -=
                        FLT2FIX(((float) damage * FIX2FLT(PCLASS_INFO(player->class)->armorIncrement[i])) /
                                 300);

                    if(player->armorPoints[i] < 2 * FRACUNIT)
                    {
                        player->armorPoints[i] = 0;
                    }
                }
            }
            saved = ((float) damage * savedPercent) / 100;

            if(saved > savedPercent * 2)
                saved = savedPercent * 2;

            damage -= (int) saved;
        }

        if(damage >= player->health &&
           ((gameSkill == SM_BABY) || deathmatch) && !player->morphTics)
        {   // Try to use some inventory health.
            P_AutoUseHealth(player, damage - player->health + 1);
        }

        player->health -= damage;

        if(player->health < 0)
            player->health = 0;

        player->attacker = source;
        player->damageCount += damage; // Add damage after armor / invuln.

        if(player->damageCount > 100)
            player->damageCount = 100; // Teleport stomp does 10k points...

        temp = (damage < 100 ? damage : 100);

        // Maybe unhide the HUD?
        ST_HUDUnHide(player - players, HUE_ON_DAMAGE);

        ST_doPaletteStuff(player - players, false);
    }

    // How about some particles, yes?
    // Only works when both target and inflictor are real mobjs.
    P_SpawnDamageParticleGen(target, inflictor, damage);

    // Do the damage.
    target->health -= damage;
    if(target->health > 0)
    {   // Still alive, phew!
        if((P_Random() < target->info->painChance) &&
           !(target->flags & MF_SKULLFLY))
        {
            if(inflictor &&
               (inflictor->type >= MT_LIGHTNING_FLOOR &&
                inflictor->type <= MT_LIGHTNING_ZAP))
            {
                if(P_Random() < 96)
                {
                    target->flags |= MF_JUSTHIT; // fight back!
                    P_MobjChangeState(target, target->info->painState);
                }
                else
                {   // "electrocute" the target.
    //// \fixme make fullbright for this frame -->
                    //target->frame |= FF_FULLBRIGHT;
    // <-- fixme
                    if((target->flags & MF_COUNTKILL) && P_Random() < 128 &&
                       !S_IsPlaying(SFX_PUPPYBEAT, target))
                    {
                        if((target->type == MT_CENTAUR) ||
                           (target->type == MT_CENTAURLEADER) ||
                           (target->type == MT_ETTIN))
                        {
                            S_StartSound(SFX_PUPPYBEAT, target);
                        }
                    }
                }
            }
            else
            {
                target->flags |= MF_JUSTHIT; // fight back!

                P_MobjChangeState(target, target->info->painState);
                if(inflictor && inflictor->type == MT_POISONCLOUD)
                {
                    if(target->flags & MF_COUNTKILL && P_Random() < 128 &&
                       !S_IsPlaying(SFX_PUPPYBEAT, target))
                    {
                        if((target->type == MT_CENTAUR) ||
                           (target->type == MT_CENTAURLEADER) ||
                           (target->type == MT_ETTIN))
                        {
                            S_StartSound(SFX_PUPPYBEAT, target);
                        }
                    }
                }
            }
        }

        target->reactionTime = 0; // We're awake now...

        if(!target->threshold && source && !(source->flags3 & MF3_NOINFIGHT) &&
           !(target->type == MT_BISHOP) && !(target->type == MT_MINOTAUR))
        {
            // Target is not intent on another, so make it chase source.
            if(!((target->type == MT_CENTAUR && source->type == MT_CENTAURLEADER) ||
                 (target->type == MT_CENTAURLEADER && source->type == MT_CENTAUR)))
            {
                target->target = source;
                target->threshold = BASETHRESHOLD;
                if(target->state == &states[target->info->spawnState] &&
                   target->info->seeState != S_NULL)
                {
                    P_MobjChangeState(target, target->info->seeState);
                }
            }
        }
    }
    else
    {   // Death.
        if(inflictor)
        {   // Check for special fire damage or ice damage deaths.
            if(inflictor->flags2 & MF2_FIREDAMAGE)
            {
                if(player && !player->morphTics)
                {   // Check for flame death.
                    if(target->health > -50 && damage > 25)
                    {
                        target->flags2 |= MF2_FIREDAMAGE;
                    }
                }
                else
                {
                    target->flags2 |= MF2_FIREDAMAGE;
                }
            }
            else if(inflictor->flags2 & MF2_ICEDAMAGE)
            {
                target->flags2 |= MF2_ICEDAMAGE;
            }
        }

        if(source && (source->type == MT_MINOTAUR))
        {   // Minotaur's kills go to his master.
            master = source->tracer;
            // Make sure still alive and not a pointer to fighter head.
            if(master->player && (master->player->plr->mo == master))
            {
                source = master;
            }
        }

        if(source && (source->player) &&
           (source->player->readyWeapon == WT_FOURTH))
        {
            // Always extreme death from fourth weapon.
            target->health = -5000;
        }

        P_KillMobj(source, target);
    }

    return originalHealth - target->health;
}

int P_FallingDamage(player_t *player)
{
    int             damage;
    float           mom, dist;

    mom = fabs(player->plr->mo->mom[MZ]);
    dist = mom * (16.0f / 23);

    if(mom >= 63)
    {   // Automatic death.
        return P_DamageMobj(player->plr->mo, NULL, NULL, 10000, false);
    }

    damage = ((dist * dist) / 10) - 24;
    if(player->plr->mo->mom[MZ] > -39 && damage > player->plr->mo->health &&
       player->plr->mo->health != 1)
    {   // No-death threshold.
        damage = player->plr->mo->health - 1;
    }

    S_StartSound(SFX_PLAYER_LAND, player->plr->mo);

    return P_DamageMobj(player->plr->mo, NULL, NULL, damage, false);
}

int P_PoisonDamage(player_t* player, mobj_t* source, int damage,
                   boolean playPainSound)
{
    int             originalHealth;
    mobj_t*         target, *inflictor;

    target = player->plr->mo;
    originalHealth = target->health;
    inflictor = source;

    if(target->health <= 0)
        return 0; // Already dead.

    if((target->flags2 & MF2_INVULNERABLE) && damage < 10000)
        return 0; // mobj is invulnerable.

    if(gameSkill == SM_BABY)
    {   // Take half damage in trainer mode
        damage /= 2;
    }

    if(damage < 1000 &&
       ((P_GetPlayerCheats(player) & CF_GODMODE) || player->powers[PT_INVULNERABILITY]))
    {
        return 0;
    }

    if(damage >= player->health && ((gameSkill == SM_BABY) || deathmatch) &&
       !player->morphTics)
    {   // Try to use some inventory health.
        P_AutoUseHealth(player, damage - player->health + 1);
    }

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_DAMAGE);

    player->health -= damage;
    if(player->health < 0)
    {
        player->health = 0;
    }
    player->attacker = source;

    // Do the damage.
    target->health -= damage;
    if(target->health > 0)
    {   // Still alive, phew!
        if(!(mapTime & 63) && playPainSound)
        {
            P_MobjChangeState(target, target->info->painState);
        }
    }
    else
    {   // Death
        target->special1 = damage;
        if(player && inflictor && !player->morphTics)
        {   // Check for flame death.
            if((inflictor->flags2 & MF2_FIREDAMAGE) &&
               (target->health > -50) && (damage > 25))
            {
                target->flags2 |= MF2_FIREDAMAGE;
            }

            if(inflictor->flags2 & MF2_ICEDAMAGE)
            {
                target->flags2 |= MF2_ICEDAMAGE;
            }
        }

        P_KillMobj(source, target);
    }

    return originalHealth - target->health;
}
