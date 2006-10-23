/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "am_map.h"
#include "p_inventory.h"
#include "p_player.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

#define BONUSADD 6

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void SetDormantArtifact(mobj_t *arti);
static void TryPickupArtifact(player_t *player, artitype_e artifactType,
                              mobj_t *artifact);
static void TryPickupWeapon(player_t *player, pclass_t weaponClass,
                            weapontype_t weaponType, mobj_t *weapon,
                            char *message);
static void TryPickupWeaponPiece(player_t *player, pclass_t matchClass,
                                 int pieceValue, mobj_t *pieceMobj);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     echoMsg = 1;

//char *TextKeyMessages[] =
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

//----------------------------------------------------------------------------
//
// PROC P_HideSpecialThing
//
//----------------------------------------------------------------------------

void P_HideSpecialThing(mobj_t *thing)
{
    thing->flags &= ~MF_SPECIAL;
    thing->flags2 |= MF2_DONTDRAW;
    P_SetMobjState(thing, S_HIDESPECIAL1);
}

//--------------------------------------------------------------------------
//
// FUNC P_GiveMana
//
// Returns true if the player accepted the mana, false if it was
// refused (player has MAX_MANA).
//
//--------------------------------------------------------------------------

boolean P_GiveMana(player_t *player, ammotype_t mana, int count)
{
    int     prevMana;

    //weapontype_t changeWeapon;

    if(mana == AM_NOAMMO || mana == MANA_BOTH)
    {
        return (false);
    }
    if(mana < 0 || mana > NUMAMMO)
    {
        Con_Error("P_GiveMana: bad type %i", mana);
    }
    if(player->ammo[mana] == MAX_MANA)
    {
        return (false);
    }
    if(gameskill == sk_baby || gameskill == sk_nightmare)
    {                           // extra mana in baby mode and nightmare mode
        count += count >> 1;
    }
    prevMana = player->ammo[mana];

    player->ammo[mana] += count;
    if(player->ammo[mana] > MAX_MANA)
    {
        player->ammo[mana] = MAX_MANA;
    }
    if(player->class == PCLASS_FIGHTER && player->readyweapon == WP_SECOND &&
       mana == MANA_1 && prevMana <= 0)
    {
        P_SetPsprite(player, ps_weapon, S_FAXEREADY_G);
    }
    player->update |= PSF_AMMO;
    return (true);
}

//==========================================================================
//
// TryPickupWeapon
//
//==========================================================================

static void TryPickupWeapon(player_t *player, pclass_t weaponClass,
                            weapontype_t weaponType, mobj_t *weapon,
                            char *message)
{
    boolean remove;
    boolean gaveMana;
    boolean gaveWeapon;

    player->update |= PSF_WEAPONS;

    remove = true;
    if(player->class != weaponClass)
    {                           // Wrong class, but try to pick up for mana
        if(IS_NETGAME && !deathmatch)
        {                       // Can't pick up weapons for other classes in coop netplay
            return;
        }
        if(weaponType == WP_SECOND)
        {
            if(!P_GiveMana(player, MANA_1, 25))
            {
                return;
            }
        }
        else
        {
            if(!P_GiveMana(player, MANA_2, 25))
            {
                return;
            }
        }
    }
    else if(IS_NETGAME && !deathmatch)
    {                           // Cooperative net-game
        if(player->weaponowned[weaponType])
        {
            return;
        }
        player->weaponowned[weaponType] = true;
        player->update |= PSF_OWNED_WEAPONS;
        if(weaponType == WP_SECOND)
        {
            P_GiveMana(player, MANA_1, 25);
        }
        else
        {
            P_GiveMana(player, MANA_2, 25);
        }
        player->pendingweapon = weaponType;
        remove = false;
    }
    else
    {                           // Deathmatch or single player game
        if(weaponType == WP_SECOND)
        {
            gaveMana = P_GiveMana(player, MANA_1, 25);
        }
        else
        {
            gaveMana = P_GiveMana(player, MANA_2, 25);
        }
        if(player->weaponowned[weaponType])
        {
            gaveWeapon = false;
        }
        else
        {
            gaveWeapon = true;
            player->weaponowned[weaponType] = true;
            player->update |= PSF_OWNED_WEAPONS;
            if(weaponType > player->readyweapon)
            {                   // Only switch to more powerful weapons
                player->pendingweapon = weaponType;
            }
        }
        if(!(gaveWeapon || gaveMana))
        {                       // Player didn't need the weapon or any mana
            return;
        }
    }

    P_SetMessage(player, message, false);
    if(weapon->special)
    {
        P_ExecuteLineSpecial(weapon->special, weapon->args, NULL, 0,
                             player->plr->mo);
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
            P_RemoveMobj(weapon);
        }
    }

    player->bonuscount += BONUSADD;
    S_ConsoleSound(SFX_PICKUP_WEAPON, NULL, player - players);
    if(player == &players[consoleplayer])
    {
        ST_doPaletteStuff(false);
    }
}

//--------------------------------------------------------------------------
//
// FUNC P_GiveWeapon
//
// Returns true if the weapon or its mana was accepted.
//
//--------------------------------------------------------------------------

/*
   boolean P_GiveWeapon(player_t *player, pclass_t class, weapontype_t weapon)
   {
   boolean gaveMana;
   boolean gaveWeapon;

   if(player->class != class)
   { // player cannot use this weapon, take it anyway, and get mana
   if(IS_NETGAME && !deathmatch)
   { // Can't pick up weapons for other classes in coop netplay
   return false;
   }
   if(weapon == WP_SECOND)
   {
   return P_GiveMana(player, MANA_1, 25);
   }
   else
   {
   return P_GiveMana(player, MANA_2, 25);
   }
   }
   if(IS_NETGAME && !deathmatch)
   { // Cooperative net-game
   if(player->weaponowned[weapon])
   {
   return(false);
   }
   player->bonuscount += BONUSADD;
   player->weaponowned[weapon] = true;
   if(weapon == WP_SECOND)
   {
   P_GiveMana(player, MANA_1, 25);
   }
   else
   {
   P_GiveMana(player, MANA_2, 25);
   }
   player->pendingweapon = weapon;
   if(player == &players[consoleplayer])
   {
   S_StartSound(NULL, SFX_PICKUP_WEAPON);
   }
   return(false);
   }
   if(weapon == WP_SECOND)
   {
   gaveMana = P_GiveMana(player, MANA_1, 25);
   }
   else
   {
   gaveMana = P_GiveMana(player, MANA_2, 25);
   }
   if(player->weaponowned[weapon])
   {
   gaveWeapon = false;
   }
   else
   {
   gaveWeapon = true;
   player->weaponowned[weapon] = true;
   if(weapon > player->readyweapon)
   { // Only switch to more powerful weapons
   player->pendingweapon = weapon;
   }
   }
   return(gaveWeapon || gaveMana);
   }
 */

//===========================================================================
//
// P_GiveWeaponPiece
//
//===========================================================================

/*
   boolean P_GiveWeaponPiece(player_t *player, pclass_t class, int piece)
   {
   P_GiveMana(player, MANA_1, 20);
   P_GiveMana(player, MANA_2, 20);
   if(player->class != class)
   {
   return true;
   }
   else if(player->pieces&piece)
   { // player already has that weapon piece
   return true;
   }
   player->pieces |= piece;
   if(player->pieces == 7)
   { // player has built the fourth weapon!
   P_GiveWeapon(player, class, WP_FOURTH);
   S_StartSound(player->plr->mo, SFX_WEAPON_BUILD);
   }
   return true;
   }
 */

//==========================================================================
//
// TryPickupWeaponPiece
//
//==========================================================================

static void TryPickupWeaponPiece(player_t *player, pclass_t matchClass,
                                 int pieceValue, mobj_t *pieceMobj)
{
    boolean remove;
    boolean checkAssembled;
    boolean gaveWeapon;
    int     gaveMana;

    //static char *fourthWeaponText[] =
    int     fourthWeaponText[] = {
        TXT_TXT_WEAPON_F4,
        TXT_TXT_WEAPON_C4,
        TXT_TXT_WEAPON_M4
    };
    //static char *weaponPieceText[] =
    int     weaponPieceText[] = {
        TXT_TXT_QUIETUS_PIECE,
        TXT_TXT_WRAITHVERGE_PIECE,
        TXT_TXT_BLOODSCOURGE_PIECE
    };
    static int pieceValueTrans[] = {
        0,                      // 0: never
        WPIECE1 | WPIECE2 | WPIECE3,    // WPIECE1 (1)
        WPIECE2 | WPIECE3,      // WPIECE2 (2)
        0,                      // 3: never
        WPIECE3                 // WPIECE3 (4)
    };

    remove = true;
    checkAssembled = true;
    gaveWeapon = false;
    if(player->class != matchClass)
    {                           // Wrong class, but try to pick up for mana
        if(IS_NETGAME && !deathmatch)
        {                       // Can't pick up wrong-class weapons in coop netplay
            return;
        }
        checkAssembled = false;
        gaveMana =
            P_GiveMana(player, MANA_1, 20) + P_GiveMana(player, MANA_2, 20);
        if(!gaveMana)
        {                       // Didn't need the mana, so don't pick it up
            return;
        }
    }
    else if(IS_NETGAME && !deathmatch)
    {                           // Cooperative net-game
        if(player->pieces & pieceValue)
        {                       // Already has the piece
            return;
        }
        pieceValue = pieceValueTrans[pieceValue];
        P_GiveMana(player, MANA_1, 20);
        P_GiveMana(player, MANA_2, 20);
        remove = false;
    }
    else
    {                           // Deathmatch or single player game
        gaveMana =
            P_GiveMana(player, MANA_1, 20) + P_GiveMana(player, MANA_2, 20);
        if(player->pieces & pieceValue)
        {                       // Already has the piece, check if mana needed
            if(!gaveMana)
            {                   // Didn't need the mana, so don't pick it up
                return;
            }
            checkAssembled = false;
        }
    }

    // Pick up the weapon piece
    if(pieceMobj->special)
    {
        P_ExecuteLineSpecial(pieceMobj->special, pieceMobj->args, NULL, 0,
                             player->plr->mo);
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
            P_RemoveMobj(pieceMobj);
        }
    }
    player->bonuscount += BONUSADD;
    if(player == &players[consoleplayer])
    {
        ST_doPaletteStuff(false);
    }

    // Check if fourth weapon assembled
    if(checkAssembled)
    {
        player->pieces |= pieceValue;
        if(player->pieces == (WPIECE1 | WPIECE2 | WPIECE3))
        {
            gaveWeapon = true;
            player->weaponowned[WP_FOURTH] = true;
            player->pendingweapon = WP_FOURTH;
            player->update |= PSF_WEAPONS | PSF_OWNED_WEAPONS;
        }
    }

    if(gaveWeapon)
    {
        P_SetMessage(player, GET_TXT(fourthWeaponText[matchClass]), false);
        // Play the build-sound full volume for all players
        S_StartSound(SFX_WEAPON_BUILD, NULL);
    }
    else
    {
        P_SetMessage(player, GET_TXT(weaponPieceText[matchClass]), false);
        S_ConsoleSound(SFX_PICKUP_WEAPON, NULL, player - players);
    }
}

//---------------------------------------------------------------------------
//
// FUNC P_GiveBody
//
// Returns false if the body isn't needed at all.
//
//---------------------------------------------------------------------------

boolean P_GiveBody(player_t *player, int num)
{
    int     max;

    max = MAXHEALTH;
    if(player->morphTics)
    {
        max = MAXMORPHHEALTH;
    }
    if(player->health >= max)
    {
        return (false);
    }
    player->health += num;
    if(player->health > max)
    {
        player->health = max;
    }
    player->plr->mo->health = player->health;
    player->update |= PSF_HEALTH;
    return (true);
}

//---------------------------------------------------------------------------
//
// FUNC P_GiveArmor
//
// Returns false if the armor is worse than the current armor.
//
//---------------------------------------------------------------------------

boolean P_GiveArmor(player_t *player, armortype_t armortype, int amount)
{
    int     hits;
    int     totalArmor;

    player->update |= PSF_ARMOR;
    if(amount == -1)
    {
        hits = PCLASS_INFO(player->class)->armorincrement[armortype];
        if(player->armorpoints[armortype] >= hits)
        {
            return false;
        }
        else
        {
            player->armorpoints[armortype] = hits;
        }
    }
    else
    {
        hits = amount * 5 * FRACUNIT;
        totalArmor =
            player->armorpoints[ARMOR_ARMOR] +
            player->armorpoints[ARMOR_SHIELD] +
            player->armorpoints[ARMOR_HELMET] +
            player->armorpoints[ARMOR_AMULET] + PCLASS_INFO(player->class)->autoarmorsave;
        if(totalArmor < PCLASS_INFO(player->class)->maxarmor * 5 * FRACUNIT)
        {
            player->armorpoints[armortype] += hits;
        }
        else
        {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------------------------
//
// PROC P_GiveKey
//
//---------------------------------------------------------------------------

int P_GiveKey(player_t *player, keytype_t key)
{
    if(player->keys & (1 << key))
    {
        return false;
    }
    player->bonuscount += BONUSADD;
    player->keys |= 1 << key;
    player->update |= PSF_KEYS;
    return true;
}

//---------------------------------------------------------------------------
//
// FUNC P_GivePower
//
// Returns true if power accepted.
//
//---------------------------------------------------------------------------

boolean P_GivePower(player_t *player, powertype_t power)
{
    player->update |= PSF_POWERS;
    if(power == pw_invulnerability)
    {
        if(player->powers[power] > BLINKTHRESHOLD)
        {                       // Already have it
            return (false);
        }
        player->powers[power] = INVULNTICS;
        player->plr->mo->flags2 |= MF2_INVULNERABLE;
        if(player->class == PCLASS_MAGE)
        {
            player->plr->mo->flags2 |= MF2_REFLECTIVE;
        }
        return (true);
    }
    if(power == pw_flight)
    {
        if(player->powers[power] > BLINKTHRESHOLD)
        {                       // Already have it
            return (false);
        }
        player->powers[power] = FLIGHTTICS;
        player->plr->mo->flags2 |= MF2_FLY;
        player->plr->mo->flags |= MF_NOGRAVITY;
        if(player->plr->mo->pos[VZ] <= player->plr->mo->floorz)
        {
            player->flyheight = 10; // thrust the player in the air a bit
            player->plr->flags |= DDPF_FIXMOM;
        }
        return (true);
    }
    if(power == pw_infrared)
    {
        if(player->powers[power] > BLINKTHRESHOLD)
        {                       // Already have it
            return (false);
        }
        player->powers[power] = INFRATICS;
        return (true);
    }
    if(power == pw_speed)
    {
        if(player->powers[power] > BLINKTHRESHOLD)
        {                       // Already have it
            return (false);
        }
        player->powers[power] = SPEEDTICS;
        return (true);
    }
    if(power == pw_minotaur)
    {
        // Doesn't matter if already have power, renew ticker
        player->powers[power] = MAULATORTICS;
        return (true);
    }
    /*
       if(power == pw_ironfeet)
       {
       player->powers[power] = IRONTICS;
       return(true);
       }
       if(power == pw_strength)
       {
       P_GiveBody(player, 100);
       player->powers[power] = 1;
       return(true);
       }
     */
    if(player->powers[power])
    {
        return (false);         // already got it
    }
    player->powers[power] = 1;
    return (true);
}

//==========================================================================
//
// TryPickupArtifact
//
//==========================================================================

static void TryPickupArtifact(player_t *player, artitype_e artifactType,
                              mobj_t *artifact)
{
    //static char *artifactMessages[NUMARTIFACTS] =
    int     artifactMessages[NUMARTIFACTS] = {
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
        TXT_TXT_ARTIPUZZGEAR,   // All gear pickups use the same text
        TXT_TXT_ARTIPUZZGEAR,
        TXT_TXT_ARTIPUZZGEAR,
        TXT_TXT_ARTIPUZZGEAR
    };

    if(P_GiveArtifact(player, artifactType, artifact))
    {
        if(artifact->special)
        {
            P_ExecuteLineSpecial(artifact->special, artifact->args, NULL, 0,
                                 NULL);
            artifact->special = 0;
        }
        player->bonuscount += BONUSADD;
        if(artifactType < arti_firstpuzzitem)
        {
            SetDormantArtifact(artifact);
            S_StartSound(SFX_PICKUP_ARTIFACT, artifact);
            P_SetMessage(player, GET_TXT(artifactMessages[artifactType]), false);
        }
        else
        {                       // Puzzle item
            S_StartSound(SFX_PICKUP_ITEM, NULL);
            P_SetMessage(player, GET_TXT(artifactMessages[artifactType]), false);
            if(!IS_NETGAME || deathmatch)
            {                   // Remove puzzle items if not cooperative netplay
                P_RemoveMobj(artifact);
            }
        }
    }
}

//==========================================================================
//
// SetDormantArtifact
//
// Removes the MF_SPECIAL flag and initiates the artifact pickup
// animation.
//
//==========================================================================

static void SetDormantArtifact(mobj_t *arti)
{
    arti->flags &= ~MF_SPECIAL;
    if(deathmatch && !(arti->flags2 & MF2_DROPPED))
    {
        if(arti->type == MT_ARTIINVULNERABILITY)
        {
            P_SetMobjState(arti, S_DORMANTARTI3_1);
        }
        else if(arti->type == MT_SUMMONMAULATOR || arti->type == MT_ARTIFLY)
        {
            P_SetMobjState(arti, S_DORMANTARTI2_1);
        }
        else
        {
            P_SetMobjState(arti, S_DORMANTARTI1_1);
        }
    }
    else
    {                           // Don't respawn
        P_SetMobjState(arti, S_DEADARTI1);
    }
}

//---------------------------------------------------------------------------
//
// PROC A_RestoreArtifact
//
//---------------------------------------------------------------------------

void C_DECL A_RestoreArtifact(mobj_t *arti)
{
    arti->flags |= MF_SPECIAL;
    P_SetMobjState(arti, arti->info->spawnstate);
    S_StartSound(SFX_RESPAWN, arti);
}

//---------------------------------------------------------------------------
//
// PROC A_RestoreSpecialThing1
//
// Make a special thing visible again.
//
//---------------------------------------------------------------------------

void C_DECL A_RestoreSpecialThing1(mobj_t *thing)
{
    thing->flags2 &= ~MF2_DONTDRAW;
    S_StartSound(SFX_RESPAWN, thing);
}

//---------------------------------------------------------------------------
//
// PROC A_RestoreSpecialThing2
//
//---------------------------------------------------------------------------

void C_DECL A_RestoreSpecialThing2(mobj_t *thing)
{
    thing->flags |= MF_SPECIAL;
    P_SetMobjState(thing, thing->info->spawnstate);
}

//---------------------------------------------------------------------------
//
// PROC P_TouchSpecialThing
//
//---------------------------------------------------------------------------

void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher)
{
    player_t *player;
    fixed_t delta;
    int     sound;
    boolean respawn;

    if(IS_CLIENT)
        return;

    delta = special->pos[VZ] - toucher->pos[VZ];
    if(delta > toucher->height || delta < -32 * FRACUNIT)
    {                           // Out of reach
        return;
    }
    if(toucher->health <= 0)
    {                           // Toucher is dead
        return;
    }
    sound = SFX_PICKUP_ITEM;
    player = toucher->player;
    if(player == NULL)
        return;

    respawn = true;
    switch (special->sprite)
    {
        // Items
    case SPR_PTN1:              // Item_HealingPotion
        if(!P_GiveBody(player, 10))
        {
            return;
        }
        P_SetMessage(player, TXT_ITEMHEALTH, false);
        break;
    case SPR_ARM1:
        if(!P_GiveArmor(player, ARMOR_ARMOR, -1))
        {
            return;
        }
        P_SetMessage(player, TXT_ARMOR1, false);
        break;
    case SPR_ARM2:
        if(!P_GiveArmor(player, ARMOR_SHIELD, -1))
        {
            return;
        }
        P_SetMessage(player, TXT_ARMOR2, false);
        break;
    case SPR_ARM3:
        if(!P_GiveArmor(player, ARMOR_HELMET, -1))
        {
            return;
        }
        P_SetMessage(player, TXT_ARMOR3, false);
        break;
    case SPR_ARM4:
        if(!P_GiveArmor(player, ARMOR_AMULET, -1))
        {
            return;
        }
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
        {
            return;
        }
        P_SetMessage(player,
                     GET_TXT(TextKeyMessages[special->sprite - SPR_KEY1]), false);
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
        {                       // Only remove keys in single player game
            break;
        }
        player->bonuscount += BONUSADD;
        S_ConsoleSound(sound, NULL, player - players);
        if(player == &players[consoleplayer])
        {
            ST_doPaletteStuff(false);
        }
        return;

        // Artifacts
    case SPR_PTN2:
        TryPickupArtifact(player, arti_health, special);
        return;
    case SPR_SOAR:
        TryPickupArtifact(player, arti_fly, special);
        return;
    case SPR_INVU:
        TryPickupArtifact(player, arti_invulnerability, special);
        return;
    case SPR_SUMN:
        TryPickupArtifact(player, arti_summon, special);
        return;
    case SPR_PORK:
        TryPickupArtifact(player, arti_egg, special);
        return;
    case SPR_SPHL:
        TryPickupArtifact(player, arti_superhealth, special);
        return;
    case SPR_HRAD:
        TryPickupArtifact(player, arti_healingradius, special);
        return;
    case SPR_TRCH:
        TryPickupArtifact(player, arti_torch, special);
        return;
    case SPR_ATLP:
        TryPickupArtifact(player, arti_teleport, special);
        return;
    case SPR_TELO:
        TryPickupArtifact(player, arti_teleportother, special);
        return;
    case SPR_PSBG:
        TryPickupArtifact(player, arti_poisonbag, special);
        return;
    case SPR_SPED:
        TryPickupArtifact(player, arti_speed, special);
        return;
    case SPR_BMAN:
        TryPickupArtifact(player, arti_boostmana, special);
        return;
    case SPR_BRAC:
        TryPickupArtifact(player, arti_boostarmor, special);
        return;
    case SPR_BLST:
        TryPickupArtifact(player, arti_blastradius, special);
        return;

        // Puzzle artifacts
    case SPR_ASKU:
        TryPickupArtifact(player, arti_puzzskull, special);
        return;
    case SPR_ABGM:
        TryPickupArtifact(player, arti_puzzgembig, special);
        return;
    case SPR_AGMR:
        TryPickupArtifact(player, arti_puzzgemred, special);
        return;
    case SPR_AGMG:
        TryPickupArtifact(player, arti_puzzgemgreen1, special);
        return;
    case SPR_AGG2:
        TryPickupArtifact(player, arti_puzzgemgreen2, special);
        return;
    case SPR_AGMB:
        TryPickupArtifact(player, arti_puzzgemblue1, special);
        return;
    case SPR_AGB2:
        TryPickupArtifact(player, arti_puzzgemblue2, special);
        return;
    case SPR_ABK1:
        TryPickupArtifact(player, arti_puzzbook1, special);
        return;
    case SPR_ABK2:
        TryPickupArtifact(player, arti_puzzbook2, special);
        return;
    case SPR_ASK2:
        TryPickupArtifact(player, arti_puzzskull2, special);
        return;
    case SPR_AFWP:
        TryPickupArtifact(player, arti_puzzfweapon, special);
        return;
    case SPR_ACWP:
        TryPickupArtifact(player, arti_puzzcweapon, special);
        return;
    case SPR_AMWP:
        TryPickupArtifact(player, arti_puzzmweapon, special);
        return;
    case SPR_AGER:
        TryPickupArtifact(player, arti_puzzgear1, special);
        return;
    case SPR_AGR2:
        TryPickupArtifact(player, arti_puzzgear2, special);
        return;
    case SPR_AGR3:
        TryPickupArtifact(player, arti_puzzgear3, special);
        return;
    case SPR_AGR4:
        TryPickupArtifact(player, arti_puzzgear4, special);
        return;

        // Mana
    case SPR_MAN1:
        if(!P_GiveMana(player, MANA_1, 15))
        {
            return;
        }
        P_SetMessage(player, TXT_MANA_1, false);
        break;
    case SPR_MAN2:
        if(!P_GiveMana(player, MANA_2, 15))
        {
            return;
        }
        P_SetMessage(player, TXT_MANA_2, false);
        break;
    case SPR_MAN3:              // Double Mana Dodecahedron
        if(!P_GiveMana(player, MANA_1, 20))
        {
            if(!P_GiveMana(player, MANA_2, 20))
            {
                return;
            }
        }
        else
        {
            P_GiveMana(player, MANA_2, 20);
        }
        P_SetMessage(player, TXT_MANA_BOTH, false);
        break;

        // 2nd and 3rd Mage Weapons
    case SPR_WMCS:              // Frost Shards
        TryPickupWeapon(player, PCLASS_MAGE, WP_SECOND, special,
                        TXT_WEAPON_M2);
        return;
    case SPR_WMLG:              // Arc of Death
        TryPickupWeapon(player, PCLASS_MAGE, WP_THIRD, special, TXT_WEAPON_M3);
        return;

        // 2nd and 3rd Fighter Weapons
    case SPR_WFAX:              // Timon's Axe
        TryPickupWeapon(player, PCLASS_FIGHTER, WP_SECOND, special,
                        TXT_WEAPON_F2);
        return;
    case SPR_WFHM:              // Hammer of Retribution
        TryPickupWeapon(player, PCLASS_FIGHTER, WP_THIRD, special,
                        TXT_WEAPON_F3);
        return;

        // 2nd and 3rd Cleric Weapons
    case SPR_WCSS:              // Serpent Staff
        TryPickupWeapon(player, PCLASS_CLERIC, WP_SECOND, special,
                        TXT_WEAPON_C2);
        return;
    case SPR_WCFM:              // Firestorm
        TryPickupWeapon(player, PCLASS_CLERIC, WP_THIRD, special,
                        TXT_WEAPON_C3);
        return;

        // Fourth Weapon Pieces
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
        P_RemoveMobj(special);
    }
    player->bonuscount += BONUSADD;
    S_ConsoleSound(sound, NULL, player - players);
    if(player == &players[consoleplayer])
    {
        ST_doPaletteStuff(false);
    }
}

// Search thinker list for minotaur
mobj_t *ActiveMinotaur(player_t *master)
{
    mobj_t *mo;
    player_t *plr;
    thinker_t *think;
    unsigned int *starttime;

    for(think = thinkercap.next; think != &thinkercap && think;
        think = think->next)
    {
        if(think->function != P_MobjThinker)
            continue;
        mo = (mobj_t *) think;
        if(mo->type != MT_MINOTAUR)
            continue;
        if(mo->health <= 0)
            continue;
        if(!(mo->flags & MF_COUNTKILL))
            continue;           // for morphed minotaurs
        if(mo->flags & MF_CORPSE)
            continue;
        starttime = (unsigned int *) mo->args;
        if((leveltime - *starttime) >= MAULATORTICS)
            continue;
        plr = mo->tracer->player;
        if(plr == master)
            return (mo);
    }
    return (NULL);
}

//---------------------------------------------------------------------------
//
// PROC P_KillMobj
//
//---------------------------------------------------------------------------

void P_KillMobj(mobj_t *source, mobj_t *target)
{
    int     dummy;
    mobj_t *master;

    if(!target) // nothing to kill
        return;

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_NOGRAVITY);
    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->flags2 &= ~MF2_PASSMOBJ;
    target->height >>= 2;
    if((target->flags & MF_COUNTKILL || target->type == MT_ZBELL) &&
       target->special)
    {                           // Initiate monster death actions
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
    {                           // Check for frag changes
        if(target->player && deathmatch)    // make sure it's a deathmatch
        {
            if(target == source)
            {                   // Self-frag
                target->player->frags[target->player - players]--;
                //target->player->update |= PSF_FRAGS;
                NetSv_FragsForAll(target->player);

                /*if(cmdfrag && IS_NETGAME && source->player == &players[consoleplayer])
                   { // Send out a frag count packet
                   //gi.SendFrags(source->player->plr);
                   } */
            }
            else
            {
                source->player->frags[target->player - players]++;
                //source->player->update |= PSF_FRAGS;
                NetSv_FragsForAll(source->player);

                /*if(cmdfrag && IS_NETGAME && source->player == &players[consoleplayer])
                   { // Send out a frag count packet
                   //gi.SendFrags(source->player->plr);
                   } */
            }
        }
    }
    if(target->player)
    {                           // Player death
        if(!source)
        {                       // Self-frag
            target->player->frags[target->player - players]--;
            //target->player->update |= PSF_FRAGS;
            NetSv_FragsForAll(target->player);

            /*if(cmdfrag && IS_NETGAME && target->player == &players[consoleplayer])
               { // Send out a frag count packet
               //gi.SendFrags(target->player->plr);
               } */
        }
        target->flags &= ~MF_SOLID;
        target->flags2 &= ~MF2_FLY;
        target->player->powers[pw_flight] = 0;
        target->player->playerstate = PST_DEAD;
        target->player->update |= PSF_STATE | PSF_POWERS;
        // Let the engine know about this, too. The DEAD flag will be
        // cleared when the player is reborn.
        target->player->plr->flags |= DDPF_DEAD;
        P_DropWeapon(target->player);
        if(target->flags2 & MF2_FIREDAMAGE)
        {                       // Player flame death
            switch (target->player->class)
            {
            case PCLASS_FIGHTER:
                S_StartSound(SFX_PLAYER_FIGHTER_BURN_DEATH, target);
                P_SetMobjState(target, S_PLAY_F_FDTH1);
                return;
            case PCLASS_CLERIC:
                S_StartSound(SFX_PLAYER_CLERIC_BURN_DEATH, target);
                P_SetMobjState(target, S_PLAY_C_FDTH1);
                return;
            case PCLASS_MAGE:
                S_StartSound(SFX_PLAYER_MAGE_BURN_DEATH, target);
                P_SetMobjState(target, S_PLAY_M_FDTH1);
                return;
            default:
                break;
            }
        }
        if(target->flags2 & MF2_ICEDAMAGE)
        {                       // Player ice death
            target->flags &= ~(7 << MF_TRANSSHIFT); //no translation
            target->flags |= MF_ICECORPSE;
            switch (target->player->class)
            {
            case PCLASS_FIGHTER:
                P_SetMobjState(target, S_FPLAY_ICE);
                return;
            case PCLASS_CLERIC:
                P_SetMobjState(target, S_CPLAY_ICE);
                return;
            case PCLASS_MAGE:
                P_SetMobjState(target, S_MPLAY_ICE);
                return;
            case PCLASS_PIG:
                P_SetMobjState(target, S_PIG_ICE);
                return;
            default:
                break;
            }
        }

        if(target->player == &players[consoleplayer] && automapactive)
        {
            // don't die in auto map,
            // switch view prior to dying
            AM_Stop();
        }
    }
    else
    {
        // target is some monster or an object

        // mobj death, record as player's kill in netgame + coop
        // could not find MF_ targets->flags that indicated *only*
        // enemies (not trees, pots, etc), so built a list
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
            switch (target->type)
            {
            case MT_FIGHTER_BOSS:
                S_StartSound(SFX_PLAYER_FIGHTER_BURN_DEATH, target);
                P_SetMobjState(target, S_PLAY_F_FDTH1);
                return;
            case MT_CLERIC_BOSS:
                S_StartSound(SFX_PLAYER_CLERIC_BURN_DEATH, target);
                P_SetMobjState(target, S_PLAY_C_FDTH1);
                return;
            case MT_MAGE_BOSS:
                S_StartSound(SFX_PLAYER_MAGE_BURN_DEATH, target);
                P_SetMobjState(target, S_PLAY_M_FDTH1);
                return;
            default:
                break;
            }
        }
        else if(target->type == MT_TREEDESTRUCTIBLE)
        {
            P_SetMobjState(target, S_ZTREEDES_X1);
            target->height = 24 * FRACUNIT;
            S_StartSound(SFX_TREE_EXPLODE, target);
            return;
        }
    }
    if(target->flags2 & MF2_ICEDAMAGE)
    {
        target->flags |= MF_ICECORPSE;
        switch (target->type)
        {
        case MT_BISHOP:
            P_SetMobjState(target, S_BISHOP_ICE);
            return;
        case MT_CENTAUR:
        case MT_CENTAURLEADER:
            P_SetMobjState(target, S_CENTAUR_ICE);
            return;
        case MT_DEMON:
        case MT_DEMON2:
            P_SetMobjState(target, S_DEMON_ICE);
            return;
        case MT_SERPENT:
        case MT_SERPENTLEADER:
            P_SetMobjState(target, S_SERPENT_ICE);
            return;
        case MT_WRAITH:
        case MT_WRAITHB:
            P_SetMobjState(target, S_WRAITH_ICE);
            return;
        case MT_ETTIN:
            P_SetMobjState(target, S_ETTIN_ICE1);
            return;
        case MT_FIREDEMON:
            P_SetMobjState(target, S_FIRED_ICE1);
            return;
        case MT_FIGHTER_BOSS:
            P_SetMobjState(target, S_FIGHTER_ICE);
            return;
        case MT_CLERIC_BOSS:
            P_SetMobjState(target, S_CLERIC_ICE);
            return;
        case MT_MAGE_BOSS:
            P_SetMobjState(target, S_MAGE_ICE);
            return;
        case MT_PIG:
            P_SetMobjState(target, S_PIG_ICE);
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
                master->player->powers[pw_minotaur] = 0;
            }
        }
    }
    else if(target->type == MT_TREEDESTRUCTIBLE)
    {
        target->height = 24 * FRACUNIT;
    }
    if(target->health < -(target->info->spawnhealth >> 1) &&
       target->info->xdeathstate)
    {                           // Extreme death
        P_SetMobjState(target, target->info->xdeathstate);
    }
    else
    {                           // Normal death
        if((target->type == MT_FIREDEMON) &&
           (target->pos[VZ] <= target->floorz + 2 * FRACUNIT) &&
           (target->info->xdeathstate))
        {
            // This is to fix the imps' staying in fall state
            P_SetMobjState(target, target->info->xdeathstate);
        }
        else
        {
            P_SetMobjState(target, target->info->deathstate);
        }
    }
    target->tics -= P_Random() & 3;
    //  I_StartSound(&actor->r, actor->info->deathsound);
}

//---------------------------------------------------------------------------
//
// FUNC P_MinotaurSlam
//
//---------------------------------------------------------------------------

void P_MinotaurSlam(mobj_t *source, mobj_t *target)
{
    angle_t angle;
    fixed_t thrust;

    angle = R_PointToAngle2(source->pos[VX], source->pos[VY],
                            target->pos[VX], target->pos[VY]);
    angle >>= ANGLETOFINESHIFT;
    thrust = 16 * FRACUNIT + (P_Random() << 10);
    target->momx += FixedMul(thrust, finecosine[angle]);
    target->momy += FixedMul(thrust, finesine[angle]);
    P_DamageMobj(target, NULL, source, HITDICE(4));
    if(target->player)
    {
        target->reactiontime = 14 + (P_Random() & 7);
    }
    source->args[0] = 0;        // Stop charging
}

//---------------------------------------------------------------------------
//
// FUNC P_MorphPlayer
//
// Returns true if the player gets turned into a pig
//
//---------------------------------------------------------------------------

boolean P_MorphPlayer(player_t *player)
{
    mobj_t *pmo;
    mobj_t *fog;
    mobj_t *beastMo;
    fixed_t pos[3];
    angle_t angle;
    int     oldFlags2;

    if(player->powers[pw_invulnerability])
    {                           // Immune when invulnerable
        return (false);
    }
    if(player->morphTics)
    {                           // Player is already a beast
        return false;
    }
    pmo = player->plr->mo;
    memcpy(pos, pmo->pos, sizeof(pos));
    angle = pmo->angle;
    oldFlags2 = pmo->flags2;
    P_SetMobjState(pmo, S_FREETARGMOBJ);
    fog = P_SpawnMobj(pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT, MT_TFOG);
    S_StartSound(SFX_TELEPORT, fog);
    beastMo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PIGPLAYER);
    beastMo->special1 = player->readyweapon;
    beastMo->angle = angle;
    beastMo->player = player;
    beastMo->dplayer = player->plr;
    player->health = beastMo->health = MAXMORPHHEALTH;
    player->plr->mo = beastMo;
    memset(&player->armorpoints[0], 0, NUMARMOR * sizeof(int));
    player->class = PCLASS_PIG;

    if(oldFlags2 & MF2_FLY)
        beastMo->flags2 |= MF2_FLY;

    player->morphTics = MORPHTICS;
    player->update |= PSF_MORPH_TIME | PSF_HEALTH;
    player->plr->flags |= DDPF_FIXPOS | DDPF_FIXMOM;
    P_ActivateMorphWeapon(player);
    return (true);
}

//---------------------------------------------------------------------------
//
// FUNC P_MorphMonster
//
//---------------------------------------------------------------------------

boolean P_MorphMonster(mobj_t *actor)
{
    mobj_t *master, *monster, *fog;
    mobjtype_t moType;
    fixed_t pos[3];
    mobj_t  oldMonster;

    if(actor->player)
        return (false);
    if(!(actor->flags & MF_COUNTKILL))
        return false;
    if(actor->flags2 & MF2_BOSS)
        return false;

    moType = actor->type;
    switch (moType)
    {
    case MT_PIG:
        return (false);
    case MT_FIGHTER_BOSS:
    case MT_CLERIC_BOSS:
    case MT_MAGE_BOSS:
        return (false);
    default:
        break;
    }

    oldMonster = *actor;
    memcpy(pos, oldMonster.pos, sizeof(pos));
    P_RemoveMobjFromTIDList(actor);
    P_SetMobjState(actor, S_FREETARGMOBJ);
    fog = P_SpawnMobj(pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT, MT_TFOG);
    S_StartSound(SFX_TELEPORT, fog);
    monster = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PIG);
    monster->special2 = moType;
    monster->special1 = MORPHTICS + P_Random();
    monster->flags |= (oldMonster.flags & MF_SHADOW);
    monster->target = oldMonster.target;
    monster->angle = oldMonster.angle;
    monster->tid = oldMonster.tid;
    monster->special = oldMonster.special;
    P_InsertMobjIntoTIDList(monster, oldMonster.tid);
    memcpy(monster->args, oldMonster.args, 5);

    // check for turning off minotaur power for active icon
    if(moType == MT_MINOTAUR)
    {
        master = oldMonster.tracer;
        if(master->health > 0)
        {
            if(!ActiveMinotaur(master->player))
            {
                master->player->powers[pw_minotaur] = 0;
            }
        }
    }
    return (true);
}

//---------------------------------------------------------------------------
//
// PROC P_AutoUseHealth
//
//---------------------------------------------------------------------------

void P_AutoUseHealth(player_t *player, int saveHealth)
{
    int     i;
    int     count;
    int     normalCount;
    int     normalSlot = 0;
    int     superCount;
    int     superSlot = 0;

    normalCount = superCount = 0;
    for(i = 0; i < player->inventorySlotNum; i++)
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
    if((gameskill == sk_baby) && (normalCount * 25 >= saveHealth))
    {                           // Use quartz flasks
        count = (saveHealth + 24) / 25;
        for(i = 0; i < count; i++)
        {
            player->health += 25;
            P_InventoryRemoveArtifact(player, normalSlot);
        }
    }
    else if(superCount * 100 >= saveHealth)
    {                           // Use mystic urns
        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; i++)
        {
            player->health += 100;
            P_InventoryRemoveArtifact(player, superSlot);
        }
    }
    else if((gameskill == sk_baby) &&
            (superCount * 100 + normalCount * 25 >= saveHealth))
    {                           // Use mystic urns and quartz flasks
        count = (saveHealth + 24) / 25;
        saveHealth -= count * 25;
        for(i = 0; i < count; i++)
        {
            player->health += 25;
            P_InventoryRemoveArtifact(player, normalSlot);
        }
        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; i++)
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

/*
 * Damages both enemies and players
 * Source and inflictor are the same for melee attacks.
 * Source can be NULL for slime, barrel explosions
 * and other environmental stuff.
 *
 * @parm inflictor: is the thing that caused the damage
 *                  creature or missile, can be NULL (slime, etc)
 * @parm source: is the thing to target after taking damage
 *               creature or NULL
 */
void P_DamageMobj2(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                  int damageP, boolean stomping)
{
    unsigned ang;
    int     saved;
    fixed_t savedPercent;
    player_t *player;
    mobj_t *master;
    fixed_t thrust;
    int     temp;
    int     i;

    // the actual damage (== damageP * netMobDamageModifier for
    // any non-player mob)
    int     damage = damageP;

    // Clients can't harm anybody.
    if(IS_CLIENT)
        return;

    if(!(target->flags & MF_SHOOTABLE))
    {
        // Shouldn't happen
        return;
    }

    // use the cvar damage multiplier netMobDamageModifier
    // only if the inflictor is not a player
    if(inflictor && !inflictor->player && inflictor->type != MT_PLAYER_FIGHTER
       && inflictor->type != MT_PLAYER_MAGE &&
       inflictor->type != MT_PLAYER_CLERIC && (!source ||
                                               (source && !source->player &&
                                                source->type !=
                                                MT_PLAYER_FIGHTER &&
                                                source->type != MT_PLAYER_MAGE
                                                && source->type !=
                                                MT_PLAYER_CLERIC)))
    {
        // means inflictor->type == MT_PLAYER(CLASS)
        //damage = (int) ((float) damage * netMobDamageModifier);
        if(IS_NETGAME)
            damage *= cfg.netMobDamageModifier;
    }

    if(target->health <= 0)
    {
        if(inflictor && inflictor->flags2 & MF2_ICEDAMAGE)
        {
            return;
        }
        else if(target->flags & MF_ICECORPSE)   // frozen
        {
            target->tics = 1;
            target->momx = target->momy = 0;
        }
        return;
    }
    if((target->flags2 & MF2_INVULNERABLE) && damage < 10000)
    {                           // mobj is invulnerable
        if(target->player)
            return;             // for player, no exceptions
        if(inflictor)
        {
            switch (inflictor->type)
            {
                // These inflictors aren't foiled by invulnerability
            case MT_HOLY_FX:
            case MT_POISONCLOUD:
            case MT_FIREBOMB:
                break;
            default:
                return;
            }
        }
        else
        {
            return;
        }
    }
    if(target->player)
    {
        target->player->update |= PSF_HEALTH;

        if(damage < 1000 &&
           ((P_GetPlayerCheats(target->player) & CF_GODMODE) ||
            target->player->powers[pw_invulnerability]))
        {
            return;
        }
    }
    if(target->flags & MF_SKULLFLY)
    {
        target->momx = target->momy = target->momz = 0;
    }
    if(target->flags2 & MF2_DORMANT)
    {
        // Invulnerable, and won't wake up
        return;
    }
    player = target->player;
    if(player && gameskill == sk_baby)
    {
        // Take half damage in trainer mode
        damage >>= 1;
    }
    // Special damage types
    if(inflictor)
    {
        switch (inflictor->type)
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
            return;             // Always return
        case MT_TELOTHER_FX1:
        case MT_TELOTHER_FX2:
        case MT_TELOTHER_FX3:
        case MT_TELOTHER_FX4:
        case MT_TELOTHER_FX5:
            if((target->flags & MF_COUNTKILL) && (target->type != MT_SERPENT)
               && (target->type != MT_SERPENTLEADER) &&
               (!(target->flags2 & MF2_BOSS)))
            {
                P_TeleportOther(target);
            }
            return;
        case MT_MINOTAUR:
            if(inflictor->flags & MF_SKULLFLY)
            {                   // Slam only when in charge mode
                P_MinotaurSlam(inflictor, target);
                return;
            }
            break;
        case MT_BISH_FX:
            // Bishops are just too nasty
            damage >>= 1;
            break;
        case MT_SHARDFX1:
            switch (inflictor->special2)
            {
            case 3:
                damage <<= 3;
                break;
            case 2:
                damage <<= 2;
                break;
            case 1:
                damage <<= 1;
                break;
            default:
                break;
            }
            break;
        case MT_CSTAFF_MISSILE:
            // Cleric Serpent Staff does poison damage
            if(target->player)
            {
                P_PoisonPlayer(target->player, source, 20);
                damage >>= 1;
            }
            break;
        case MT_ICEGUY_FX2:
            damage >>= 1;
            break;
        case MT_POISONDART:
            if(target->player)
            {
                P_PoisonPlayer(target->player, source, 20);
                damage >>= 1;
            }
            break;
        case MT_POISONCLOUD:
            if(target->player)
            {
                if(target->player->poisoncount < 4)
                {
                    P_PoisonDamage(target->player, source, 15 + (P_Random() & 15), false);  // Don't play painsound
                    P_PoisonPlayer(target->player, source, 50);
                    S_StartSound(SFX_PLAYER_POISONCOUGH, target);
                }
                return;
            }
            else if(!(target->flags & MF_COUNTKILL))
            {                   // only damage monsters/players with the poison cloud
                return;
            }
            break;
        case MT_FSWORD_MISSILE:
            if(target->player)
            {
                damage -= damage >> 2;
            }
            break;
        default:
            break;
        }
    }
    // Push the target unless source is using the gauntlets
    if(inflictor && (!source || !source->player) &&
       !(inflictor->flags2 & MF2_NODMGTHRUST))
    {
        ang =
            R_PointToAngle2(inflictor->pos[VX], inflictor->pos[VY],
                            target->pos[VX], target->pos[VY]);
        //thrust = damage*(FRACUNIT>>3)*100/target->info->mass;
        if(!target->info->mass)
            Con_Error("P_DamageMobj: No target->info->mass!\n");
        thrust = damage * (FRACUNIT >> 3) * 150 / target->info->mass;
        // make fall forwards sometimes
        if((damage < 40) && (damage > target->health) &&
           (target->pos[VZ] - inflictor->pos[VZ] > 64 * FRACUNIT) && (P_Random() & 1))
        {
            ang += ANG180;
            thrust *= 4;
        }
        ang >>= ANGLETOFINESHIFT;
        target->momx += FixedMul(thrust, finecosine[ang]);
        target->momy += FixedMul(thrust, finesine[ang]);
        if(target->dplayer)
        {
            // Only fix momentum. Otherwise clients will find it difficult
            // to escape from the damage inflictor.
            target->dplayer->flags |= DDPF_FIXMOM;
        }
    }

    //
    // player specific
    //
    if(player)
    {
        savedPercent =
            PCLASS_INFO(player->class)->autoarmorsave + player->armorpoints[ARMOR_ARMOR] +
            player->armorpoints[ARMOR_SHIELD] +
            player->armorpoints[ARMOR_HELMET] +
            player->armorpoints[ARMOR_AMULET];
        if(savedPercent)
        {                       // armor absorbed some damage
            if(savedPercent > 100 * FRACUNIT)
            {
                savedPercent = 100 * FRACUNIT;
            }
            for(i = 0; i < NUMARMOR; i++)
            {
                if(player->armorpoints[i])
                {
                    player->armorpoints[i] -=
                        FixedDiv(FixedMul
                                 (damage << FRACBITS,
                                  PCLASS_INFO(player->class)->armorincrement[i]),
                                 300 * FRACUNIT);
                    if(player->armorpoints[i] < 2 * FRACUNIT)
                    {
                        player->armorpoints[i] = 0;
                    }
                }
            }
            saved =
                FixedDiv(FixedMul(damage << FRACBITS, savedPercent),
                         100 * FRACUNIT);

            if(saved > savedPercent * 2)
                saved = savedPercent * 2;

            damage -= saved >> FRACBITS;
        }
        if(damage >= player->health && ((gameskill == sk_baby) || deathmatch)
           && !player->morphTics)
        {                       // Try to use some inventory health
            P_AutoUseHealth(player, damage - player->health + 1);
        }

        player->health -= damage;   // mirror mobj health here for Dave

        if(player->health < 0)
            player->health = 0;

        player->attacker = source;
        player->damagecount += damage;  // add damage after armor / invuln

        if(player->damagecount > 100)
            player->damagecount = 100;  // teleport stomp does 10k points...

        temp = damage < 100 ? damage : 100;
        if(player == &players[consoleplayer])
        {
            //          I_Tactile(40, 10, 40+temp*2);
            ST_doPaletteStuff(false);
        }
    }

    // How about some particles, yes?
    // Only works when both target and inflictor are real mobjs.
    P_SpawnDamageParticleGen(target, inflictor, damage);

    //
    // do the damage
    //
    target->health -= damage;
    if(target->health <= 0)
    {                           // Death
        if(inflictor)
        {                       // check for special fire damage or ice damage deaths
            if(inflictor->flags2 & MF2_FIREDAMAGE)
            {
                if(player && !player->morphTics)
                {               // Check for flame death
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
        {   // Minotaur's kills go to his master
            master = source->tracer;
            // Make sure still alive and not a pointer to fighter head
            if(master->player && (master->player->plr->mo == master))
            {
                source = master;
            }
        }
        if(source && (source->player) &&
           (source->player->readyweapon == WP_FOURTH))
        {
            // Always extreme death from fourth weapon
            target->health = -5000;
        }
        P_KillMobj(source, target);
        return;
    }
    if((P_Random() < target->info->painchance) &&
       !(target->flags & MF_SKULLFLY))
    {
        if(inflictor &&
           (inflictor->type >= MT_LIGHTNING_FLOOR &&
            inflictor->type <= MT_LIGHTNING_ZAP))
        {
            if(P_Random() < 96)
            {
                target->flags |= MF_JUSTHIT;    // fight back!
                P_SetMobjState(target, target->info->painstate);
            }
            else
            {                   // "electrocute" the target
// fixme make fullbright for this frame -->
                //target->frame |= FF_FULLBRIGHT;
// <-- fixme
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
        else
        {
            target->flags |= MF_JUSTHIT;    // fight back!
            P_SetMobjState(target, target->info->painstate);
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
    target->reactiontime = 0;   // we're awake now...
    if(!target->threshold && source && !(source->flags2 & MF2_BOSS) &&
       !(target->type == MT_BISHOP) && !(target->type == MT_MINOTAUR))
    {
        // Target actor is not intent on another actor,
        // so make him chase after source
        if((target->type == MT_CENTAUR && source->type == MT_CENTAURLEADER) ||
           (target->type == MT_CENTAURLEADER && source->type == MT_CENTAUR))
        {
            return;
        }
        target->target = source;
        target->threshold = BASETHRESHOLD;
        if(target->state == &states[target->info->spawnstate] &&
           target->info->seestate != S_NULL)
        {
            P_SetMobjState(target, target->info->seestate);
        }
    }
}

//==========================================================================
//
// P_FallingDamage
//
//==========================================================================

void P_FallingDamage(player_t *player)
{
    int     damage;
    int     mom;
    int     dist;

    mom = abs(player->plr->mo->momz);
    dist = FixedMul(mom, 16 * FRACUNIT / 23);

    if(mom >= 63 * FRACUNIT)
    {                           // automatic death
        P_DamageMobj(player->plr->mo, NULL, NULL, 10000);
        return;
    }
    damage = ((FixedMul(dist, dist) / 10) >> FRACBITS) - 24;
    if(player->plr->mo->momz > -39 * FRACUNIT &&
       damage > player->plr->mo->health && player->plr->mo->health != 1)
    {                           // No-death threshold
        damage = player->plr->mo->health - 1;
    }
    S_StartSound(SFX_PLAYER_LAND, player->plr->mo);
    P_DamageMobj(player->plr->mo, NULL, NULL, damage);
}

//==========================================================================
//
// P_PoisonPlayer - Sets up all data concerning poisoning
//
//==========================================================================

void P_PoisonPlayer(player_t *player, mobj_t *poisoner, int poison)
{
    if((P_GetPlayerCheats(player) & CF_GODMODE) || player->powers[pw_invulnerability])
        return;

    player->poisoncount += poison;
    player->poisoner = poisoner;

    if(player->poisoncount > 100)
        player->poisoncount = 100;
}

//==========================================================================
//
// P_PoisonDamage - Similar to P_DamageMobj
//
//==========================================================================

void P_PoisonDamage(player_t *player, mobj_t *source, int damage,
                    boolean playPainSound)
{
    mobj_t *target;
    mobj_t *inflictor;

    target = player->plr->mo;
    inflictor = source;
    if(target->health <= 0)
    {
        return;
    }
    if(target->flags2 & MF2_INVULNERABLE && damage < 10000)
    {                           // mobj is invulnerable
        return;
    }
    if(player && gameskill == sk_baby)
    {
        // Take half damage in trainer mode
        damage >>= 1;
    }
    if(damage < 1000 &&
       ((P_GetPlayerCheats(player) & CF_GODMODE) || player->powers[pw_invulnerability]))
    {
        return;
    }
    if(damage >= player->health && ((gameskill == sk_baby) || deathmatch) &&
       !player->morphTics)
    {                           // Try to use some inventory health
        P_AutoUseHealth(player, damage - player->health + 1);
    }
    player->health -= damage;   // mirror mobj health here for Dave
    if(player->health < 0)
    {
        player->health = 0;
    }
    player->attacker = source;

    //
    // do the damage
    //
    target->health -= damage;
    if(target->health <= 0)
    {                           // Death
        target->special1 = damage;
        if(player && inflictor && !player->morphTics)
        {                       // Check for flame death
            if((inflictor->flags2 & MF2_FIREDAMAGE) && (target->health > -50)
               && (damage > 25))
            {
                target->flags2 |= MF2_FIREDAMAGE;
            }
            if(inflictor->flags2 & MF2_ICEDAMAGE)
            {
                target->flags2 |= MF2_ICEDAMAGE;
            }
        }
        P_KillMobj(source, target);
        return;
    }
    if(!(leveltime & 63) && playPainSound)
    {
        P_SetMobjState(target, target->info->painstate);
    }
    /*
       if((P_Random() < target->info->painchance)
       && !(target->flags&MF_SKULLFLY))
       {
       target->flags |= MF_JUSTHIT; // fight back!
       P_SetMobjState(target, target->info->painstate);
       }
     */
}
