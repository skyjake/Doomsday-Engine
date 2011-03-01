/**\file m_cheat.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Cheats - Hexen specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef UNIX
# include <errno.h>
#endif

#include "jhexen.h"

#include "d_net.h"
#include "g_common.h"
#include "p_player.h"
#include "am_map.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "dmu_lib.h"
#include "p_user.h"
#include "p_start.h"
#include "p_inventory.h"
#include "hu_inventory.h"
#include "g_eventsequence.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int Cht_GodFunc(const int* args, int player);
int Cht_NoClipFunc(const int* args, int player);
int Cht_WeaponsFunc(const int* args, int player);
int Cht_HealthFunc(const int* args, int player);
int Cht_GiveKeysFunc(const int* args, int player);
int Cht_SoundFunc(const int* args, int player);
int Cht_InventoryFunc(const int* args, int player);
int Cht_PuzzleFunc(const int* args, int player);
int Cht_InitFunc(const int* args, int player);
int Cht_WarpFunc(const int* args, int player);
int Cht_PigFunc(const int* args, int player);
int Cht_MassacreFunc(const int* args, int player);
int Cht_IDKFAFunc(const int* args, int player);
int Cht_QuickenFunc1(const int* args, int player);
int Cht_QuickenFunc2(const int* args, int player);
int Cht_QuickenFunc3(const int* args, int player);
int Cht_ClassFunc1(const int* args, int player);
int Cht_ClassFunc2(const int* args, int player);
int Cht_VersionFunc(const int* args, int player);
int Cht_ScriptFunc1(const int* args, int player);
int Cht_ScriptFunc2(const int* args, int player);
int Cht_ScriptFunc3(const int* args, int player);
int Cht_RevealFunc(const int* args, int player);
int Cht_WhereFunc(const int* args, int player);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Toggle god mode
static unsigned char cheatGodSeq[] = {
    's', 'a', 't', 'a', 'n'
};

// Toggle no clipping mode
static unsigned char cheatNoClipSeq[] = {
    'c', 'a', 's', 'p', 'e', 'r'
};

// Get all weapons and mana
static unsigned char cheatWeaponsSeq[] = {
    'n', 'r', 'a'
};

// Get full health
static unsigned char cheatHealthSeq[] = {
    'c', 'l', 'u', 'b', 'm', 'e', 'd'
};

// Get all keys
static unsigned char cheatKeysSeq[] = {
    'l', 'o', 'c', 'k', 's', 'm', 'i', 't', 'h'
};

// Toggle sound debug info
static unsigned char cheatSoundSeq[] = {
    'n', 'o', 'i', 's', 'e'
};

// Toggle ticker
static unsigned char cheatTickerSeq[] = {
    't', 'i', 'c', 'k', 'e', 'r'
};

// Get all inventory items
static unsigned char cheatInventorySeq[] = {
    'i', 'n', 'd', 'i', 'a', 'n', 'a'
};

// Get all puzzle pieces
static unsigned char cheatPuzzleSeq[] = {
    's', 'h', 'e', 'r', 'l', 'o', 'c', 'k'
};

// Warp to new map
static unsigned char cheatWarpSeq[] = {
    'v', 'i', 's', 'i', 't', 1, 0, 0
};

// Become a pig
static unsigned char cheatPigSeq[] = {
    'd', 'e', 'l', 'i', 'v', 'e', 'r', 'a', 'n', 'c', 'e'
};

// Kill all monsters
static unsigned char cheatMassacreSeq[] = {
    'b', 'u', 't', 'c', 'h', 'e', 'r'
};

static unsigned char cheatIDKFASeq[] = {
    'c', 'o', 'n', 'a', 'n'
};

static unsigned char cheatQuickenSeq1[] = {
    'm', 'a', 'r', 't', 'e', 'k'
};

static unsigned char cheatQuickenSeq2[] = {
    'm', 'a', 'r', 't', 'e', 'k', 'm', 'a', 'r', 't', 'e', 'k'
};

static unsigned char cheatQuickenSeq3[] = {
    'm', 'a', 'r', 't', 'e', 'k', 'm', 'a', 'r', 't', 'e', 'k', 'm', 'a', 'r', 't', 'e', 'k'
};

// New class
static unsigned char cheatClass1Seq[] = {
    's', 'h', 'a', 'd', 'o', 'w', 'c', 'a', 's', 't', 'e', 'r'
};

static unsigned char cheatClass2Seq[] = {
    's', 'h', 'a', 'd', 'o', 'w', 'c', 'a', 's', 't', 'e', 'r', 1, 0
};

static unsigned char cheatInitSeq[] = {
    'i', 'n', 'i', 't'
};

static unsigned char cheatVersionSeq[] = {
    'm', 'r', 'j', 'o', 'n', 'e', 's'
};

static unsigned char cheatDebugSeq[] = {
    'w', 'h', 'e', 'r', 'e'
};

static unsigned char cheatScriptSeq1[] = {
    'p', 'u', 'k', 'e'
};

static unsigned char cheatScriptSeq2[] = {
    'p', 'u', 'k', 'e', 1, 0
};

static unsigned char cheatScriptSeq3[] = {
    'p', 'u', 'k', 'e', 1, 0, 0
};

static unsigned char cheatRevealSeq[] = {
    'm', 'a', 'p', 's', 'c', 'o'
};

static unsigned char cheatTrackSeq1[] = {
    'c', 'd', 't'
};

static unsigned char cheatTrackSeq2[] = {
    'c', 'd', 't', 1, 0, 0
};

// CODE --------------------------------------------------------------------

static boolean cheatsEnabled(void)
{
    return !IS_NETGAME;
}

void Cht_Init(void)
{
    G_AddEventSequence(cheatGodSeq, sizeof(cheatGodSeq), Cht_GodFunc);
    G_AddEventSequence(cheatNoClipSeq, sizeof(cheatNoClipSeq), Cht_NoClipFunc);
    G_AddEventSequence(cheatWeaponsSeq, sizeof(cheatWeaponsSeq), Cht_WeaponsFunc);
    G_AddEventSequence(cheatHealthSeq, sizeof(cheatHealthSeq), Cht_HealthFunc);
    G_AddEventSequence(cheatKeysSeq, sizeof(cheatKeysSeq), Cht_GiveKeysFunc);
    G_AddEventSequence(cheatSoundSeq, sizeof(cheatSoundSeq), Cht_SoundFunc);
    G_AddEventSequence(cheatInventorySeq, sizeof(cheatInventorySeq), Cht_InventoryFunc);
    G_AddEventSequence(cheatPuzzleSeq, sizeof(cheatPuzzleSeq), Cht_PuzzleFunc);
    G_AddEventSequence(cheatWarpSeq, sizeof(cheatWarpSeq), Cht_WarpFunc);
    G_AddEventSequence(cheatPigSeq, sizeof(cheatPigSeq), Cht_PigFunc);
    G_AddEventSequence(cheatMassacreSeq, sizeof(cheatMassacreSeq), Cht_MassacreFunc);
    G_AddEventSequence(cheatIDKFASeq, sizeof(cheatIDKFASeq), Cht_IDKFAFunc);
    G_AddEventSequence(cheatQuickenSeq3, sizeof(cheatQuickenSeq3), Cht_QuickenFunc3);
    G_AddEventSequence(cheatQuickenSeq2, sizeof(cheatQuickenSeq2), Cht_QuickenFunc2);
    G_AddEventSequence(cheatQuickenSeq1, sizeof(cheatQuickenSeq1), Cht_QuickenFunc1);
    G_AddEventSequence(cheatClass2Seq, sizeof(cheatClass2Seq), Cht_ClassFunc2);
    G_AddEventSequence(cheatClass1Seq, sizeof(cheatClass1Seq), Cht_ClassFunc1);
    G_AddEventSequence(cheatInitSeq, sizeof(cheatInitSeq), Cht_InitFunc);
    G_AddEventSequence(cheatVersionSeq, sizeof(cheatVersionSeq), Cht_VersionFunc);
    G_AddEventSequence(cheatDebugSeq, sizeof(cheatDebugSeq), Cht_WhereFunc);
    G_AddEventSequence(cheatScriptSeq3, sizeof(cheatScriptSeq3), Cht_ScriptFunc3);
    G_AddEventSequence(cheatScriptSeq2, sizeof(cheatScriptSeq2), Cht_ScriptFunc2);
    G_AddEventSequence(cheatScriptSeq1, sizeof(cheatScriptSeq1), Cht_ScriptFunc1);
    G_AddEventSequence(cheatRevealSeq, sizeof(cheatRevealSeq), Cht_RevealFunc);
}

int Cht_GodFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    plr->cheats ^= CF_GODMODE;
    plr->update |= PSF_STATE;

    P_SetMessage(plr,
                 ((P_GetPlayerCheats(plr) & CF_GODMODE) ? TXT_CHEATGODON : TXT_CHEATGODOFF), false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

static void giveArmor(player_t* plr)
{
    int i;

    plr->update |= PSF_ARMOR_POINTS;
    for(i = 0; i < NUMARMOR; ++i)
        plr->armorPoints[i] = PCLASS_INFO(plr->class)->armorIncrement[i];
}

static void giveWeapons(player_t* plr)
{
    int i;

    plr->update |= PSF_OWNED_WEAPONS;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        plr->weapons[i].owned = true;
    }
}

static void giveAmmo(player_t* plr)
{
    int i;

    plr->update |= PSF_AMMO;
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        plr->ammo[i].owned = MAX_MANA;
    }
}

int Cht_GiveKeysFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    plr->update |= PSF_KEYS;
    plr->keys = 2047;
    P_SetMessage(plr, TXT_CHEATKEYS, false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_WeaponsFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    giveWeapons(plr);
    giveAmmo(plr);
    giveArmor(plr);

    P_SetMessage(plr, TXT_CHEATWEAPONS, false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_NoClipFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    plr->cheats ^= CF_NOCLIP;
    plr->update |= PSF_STATE;
    P_SetMessage(plr,
                 ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? TXT_CHEATNOCLIPON : TXT_CHEATNOCLIPOFF), false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_WarpFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    int i, tens, ones;
    uint map;

    if(IS_NETGAME)
        return false;

    tens = args[0] - '0';
    ones = args[1] - '0';
    if(tens < 0 || tens > 9 || ones < 1 || ones > 9)
    {   // Bad map
        P_SetMessage(plr, TXT_CHEATBADINPUT, false);
        return false;
    }

    map = P_TranslateMap((tens * 10 + ones) - 1);
    if(userGame && map == gameMap)
    {   // Don't try to teleport to the current map.
        P_SetMessage(plr, TXT_CHEATBADINPUT, false);
        return false;
    }

    // Search primary lumps.
    if(!P_MapExists(0, map))
    {   // Can't find.
        P_SetMessage(plr, TXT_CHEATNOMAP, false);
        return false;
    }

    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    P_SetMessage(plr, TXT_CHEATWARP, false);

    // Clear the menu if open.
    Hu_MenuCommand(MCMD_CLOSEFAST);

    // Close any open automaps.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;
        AM_Open(AM_MapForPlayer(i), false, true);
        Hu_InventoryOpen(i, false);
    }

    // So be it.
    if(userGame)
    {
        nextMap = map;
        nextMapEntryPoint = 0;
        briefDisabled = true;
        G_SetGameAction(GA_LEAVEMAP);
    }
    else
    {
        briefDisabled = true;
        G_StartNewInit();
        G_InitNew(dSkill, 0, map);
    }

    return true;
}

int Cht_SoundFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    debugSound = !debugSound;
    if(debugSound)
    {
        P_SetMessage(plr, TXT_CHEATSOUNDON, false);
    }
    else
    {
        P_SetMessage(plr, TXT_CHEATSOUNDOFF, false);
    }
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

static void printDebugInfo(int player)
{
    player_t* plr = &players[player];
    char lumpName[9], textBuffer[256];
    subsector_t* sub;

    if(!plr->plr->mo)
        return;

    P_MapId(gameEpisode, gameMap, lumpName);
    sprintf(textBuffer, "MAP [%s]  X:%g  Y:%g  Z:%g",
            lumpName, plr->plr->mo->pos[VX], plr->plr->mo->pos[VY],
            plr->plr->mo->pos[VZ]);
    P_SetMessage(plr, textBuffer, false);

    // Also print some information to the console.
    Con_Message("%s", textBuffer);
    sub = plr->plr->mo->subsector;
    Con_Message("\nSubsector %i:\n", P_ToIndex(sub));
    Con_Message("  FloorZ:%g Material:%s\n",
                P_GetFloatp(sub, DMU_FLOOR_HEIGHT),
                Materials_GetName(P_GetPtrp(sub, DMU_FLOOR_MATERIAL)));
    Con_Message("  CeilingZ:%g Material:%s\n",
                P_GetFloatp(sub, DMU_CEILING_HEIGHT),
                Materials_GetName(P_GetPtrp(sub, DMU_CEILING_MATERIAL)));
    Con_Message("Player height:%g   Player radius:%g\n",
                plr->plr->mo->height, plr->plr->mo->radius);
}

int Cht_WhereFunc(const int* args, int player)
{
    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(!userGame)
        return false;

    printDebugInfo(player);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_HealthFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    plr->update |= PSF_HEALTH;
    if(plr->morphTics)
    {
        plr->health = plr->plr->mo->health = MAXMORPHHEALTH;
    }
    else
    {
        plr->health = plr->plr->mo->health = maxHealth;
    }
    P_SetMessage(plr, TXT_CHEATHEALTH, false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_InventoryFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    int i, j;

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    for(i = IIT_NONE + 1; i < IIT_FIRSTPUZZITEM; ++i)
    {
        for(j = 0; j < 25; ++j)
        {
            P_InventoryGive(player, i, false);
        }
    }

    P_SetMessage(plr, TXT_CHEATINVITEMS3, false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_PuzzleFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    int i;

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    for(i = IIT_FIRSTPUZZITEM; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        P_InventoryGive(player, i, false);
    }

    P_SetMessage(plr, TXT_CHEATINVITEMS3, false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_InitFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    G_DeferedInitNew(gameSkill, gameEpisode, gameMap);
    P_SetMessage(plr, TXT_CHEATWARP, false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_PigFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    if(plr->morphTics)
    {
        P_UndoPlayerMorph(plr);
    }
    else
    {
        P_MorphPlayer(plr);
    }

    P_SetMessage(plr, "SQUEAL!!", false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_MassacreFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    int count;
    char buf[80];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    count = P_Massacre();
    sprintf(buf, "%d MONSTERS KILLED\n", count);
    P_SetMessage(plr, buf, false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_IDKFAFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    int i;

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    if(plr->morphTics)
    {
        return false;
    }

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        plr->weapons[i].owned = false;
    }

    plr->pendingWeapon = WT_FIRST;
    P_SetMessage(plr, TXT_CHEATIDKFA, false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_QuickenFunc1(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_SetMessage(plr, "TRYING TO CHEAT?  THAT'S ONE....", false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_QuickenFunc2(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_SetMessage(plr, "THAT'S TWO....", false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_QuickenFunc3(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_DamageMobj(plr->plr->mo, NULL, plr->plr->mo, 10000, false);
    P_SetMessage(plr, "THAT'S THREE!  TIME TO DIE.", false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_ClassFunc1(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_SetMessage(plr, "ENTER NEW PLAYER CLASS NUMBER", false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_ClassFunc2(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_PlayerChangeClass(plr, args[0] - '0');
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_VersionFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    DD_Execute(false, "version");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_ScriptFunc1(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_SetMessage(plr, "RUN WHICH SCRIPT(01-99)?", false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_ScriptFunc2(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_SetMessage(plr, "RUN WHICH SCRIPT(01-99)?", false);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_ScriptFunc3(const int* args, int player)
{
    player_t* plr = &players[player];
    int script, tens, ones;
    byte scriptArgs[3];
    char textBuffer[40];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    tens = args[0] - '0';
    ones = args[1] - '0';
    script = tens * 10 + ones;
    if(script < 1)
        return false;
    if(script > 99)
        return false;
    scriptArgs[0] = scriptArgs[1] = scriptArgs[2] = 0;

    if(P_StartACS(script, 0, scriptArgs, plr->plr->mo, NULL, 0))
    {
        sprintf(textBuffer, "RUNNING SCRIPT %.2d", script);
        P_SetMessage(plr, textBuffer, false);
    }
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

int Cht_RevealFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    automapid_t map;

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    map = AM_MapForPlayer(plr - players);
    if(!AM_IsActive(map))
        return false;

    AM_IncMapCheatLevel(map);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

/**
 * This is the multipurpose cheat ccmd.
 */
D_CMD(Cheat)
{
    size_t i;

    // Give each of the characters in argument two to the SB event handler.
    for(i = 0; i < strlen(argv[1]); ++i)
    {
        event_t ev;

        ev.type = EV_KEY;
        ev.state = EVS_DOWN;
        ev.data1 = argv[1][i];
        ev.data2 = ev.data3 = 0;
        G_EventSequenceResponder(&ev);
    }
    return true;
}

D_CMD(CheatGod)
{
    if(G_GetGameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("god");
        }
        else
        {
            int player = CONSOLEPLAYER;

            if(IS_NETGAME && !netSvAllowCheats)
                return false;

            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS)
                    return false;
            }

            if(!players[player].plr->inGame)
                return false;

            Cht_GodFunc(NULL, player);
        }
    }
    return true;
}

D_CMD(CheatNoClip)
{
    if(G_GetGameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("noclip");
        }
        else
        {
            int player = CONSOLEPLAYER;

            if(IS_NETGAME && !netSvAllowCheats)
                return false;

            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS)
                    return false;
            }

            if(!players[player].plr->inGame)
                return false;

            Cht_NoClipFunc(NULL, player);
        }
    }
    return true;
}

static int suicideResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        if(IS_NETGAME && IS_CLIENT)
        {
            NetCl_CheatRequest("suicide");
        }
        else
        {
            player_t* plr = &players[CONSOLEPLAYER];
            P_DamageMobj(plr->plr->mo, NULL, NULL, 10000, false);
        }
    }
    return true;
}

D_CMD(CheatSuicide)
{
    if(G_GetGameState() == GS_MAP)
    {
        player_t* plr;

        if(IS_NETGAME && !netSvAllowCheats)
            return false;

        if(argc == 2)
        {
            int i = atoi(argv[1]);
            if(i < 0 || i >= MAXPLAYERS)
                return false;
            plr = &players[i];
        }
        else
            plr = &players[CONSOLEPLAYER];

        if(!plr->plr->inGame)
            return false;

        if(plr->playerState == PST_DEAD)
            return false;

        if(!IS_NETGAME || IS_CLIENT)
        {
            Hu_MsgStart(MSG_YESNO, SUICIDEASK, suicideResponse, NULL);
            return true;
        }

        P_DamageMobj(plr->plr->mo, NULL, NULL, 10000, false);
        return true;
    }
    else
    {
        Hu_MsgStart(MSG_ANYKEY, SUICIDEOUTMAP, NULL, NULL);
    }

    return true;
}

D_CMD(CheatWarp)
{
    int num, args[2];

    if(IS_NETGAME)
        return false;

    if(argc != 2)
    {
        Con_Printf("Usage: warp (num)\n");
        return true;
    }

    num = atoi(argv[1]);
    args[0] = num / 10 + '0';
    args[1] = num % 10 + '0';

    Cht_WarpFunc(args, CONSOLEPLAYER);
    return true;
}

D_CMD(CheatReveal)
{
    int option;
    automapid_t map;

    if(!cheatsEnabled())
        return false;

    map = AM_MapForPlayer(CONSOLEPLAYER);
    AM_SetCheatLevel(map, 0);
    AM_RevealMap(map, false);

    option = atoi(argv[1]);
    if(option < 0 || option > 3)
        return false;

    if(option == 1)
        AM_RevealMap(map, true);
    else if(option != 0)
        AM_SetCheatLevel(map, option -1);

    return true;
}

D_CMD(CheatGive)
{
    int player = CONSOLEPLAYER;
    size_t i, stuffLen;
    player_t* plr;
    char buf[100];

    if(IS_CLIENT)
    {
        if(argc != 2)
            return false;

        sprintf(buf, "give %s", argv[1]);
        NetCl_CheatRequest(buf);
        return true;
    }

    if(IS_NETGAME && !netSvAllowCheats)
        return false;

    if(argc != 2 && argc != 3)
    {
        Con_Printf("Usage:\n  give (stuff)\n");
        Con_Printf("  give (stuff) (plr)\n");
        Con_Printf("Stuff consists of one or more of (type:id). "
                   "If no id; give all of type:\n");
        Con_Printf(" a - ammo\n");
        Con_Printf(" i - items\n");
        Con_Printf(" h - health\n");
        Con_Printf(" k - keys\n");
        Con_Printf(" p - puzzle\n");
        Con_Printf(" w - weapons\n");
        Con_Printf("Example: 'give ikw' gives items, keys and weapons.\n");
        Con_Printf("Example: 'give w2k1' gives weapon two and key one.\n");
        return true;
    }

    if(argc == 3)
    {
        player = atoi(argv[2]);
        if(player < 0 || player >= MAXPLAYERS)
            return false;
    }

    if(G_GetGameState() != GS_MAP)
    {
        Con_Printf("Can only \"give\" when in a game!\n");
        return true;
    }

    if(!players[player].plr->inGame)
        return true; // Can't give to a plr who's not playing.
    plr = &players[player];

    strcpy(buf, argv[1]); // Stuff is the 2nd arg.
    strlwr(buf);
    stuffLen = strlen(buf);
    for(i = 0; buf[i]; ++i)
    {
        switch(buf[i])
        {
        case 'a':
            if(i < stuffLen)
            {
                char* end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < AT_FIRST || idx >= NUM_AMMO_TYPES)
                    {
                        Con_Printf("Unknown ammo #%d (valid range %d-%d).\n",
                                   (int)idx, AT_FIRST, NUM_AMMO_TYPES-1);
                        break;
                    }

                    // Give one specific ammo type.
                    plr->update |= PSF_AMMO;
                    plr->ammo[idx].owned = MAX_MANA;
                    break;
                }
            }

            // Give all ammo.
            plr->update |= PSF_AMMO;
            { int j;
            for(j = 0; j < NUM_AMMO_TYPES; ++j)
                plr->ammo[j].owned = MAX_MANA;
            }
            break;

        case 'i':
            Cht_InventoryFunc(NULL, player);
            break;

        case 'h':
            Cht_HealthFunc(NULL, player);
            break;

        case 'k':
            if(i < stuffLen)
            {
                char* end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < KT_FIRST || idx >= NUM_KEY_TYPES)
                    {
                        Con_Printf("Unknown key #%d (valid range %d-%d).\n",
                                   (int)idx, KT_FIRST, NUM_KEY_TYPES-1);
                        break;
                    }

                    // Give one specific key.
                    plr->update |= PSF_KEYS;
                    plr->keys |= (1 << idx);
                    break;
                }
            }

            // Give all keys.
            Cht_GiveKeysFunc(NULL, player);
            break;

        case 'p':
            Cht_PuzzleFunc(NULL, player);
            break;

        case 'w':
            if(i < stuffLen)
            {
                char* end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < WT_FIRST || idx >= NUM_WEAPON_TYPES)
                    {
                        Con_Printf("Unknown weapon #%d (valid range %d-%d).\n",
                                   (int)idx, WT_FIRST, NUM_WEAPON_TYPES-1);
                        break;
                    }

                    // Give one specific weapon.
                    plr->update |= PSF_OWNED_WEAPONS;
                    plr->weapons[idx].owned = true;
                    break;
                }
            }

            // Give all weapons.
            giveWeapons(plr);
            break;

        default: // Unrecognized.
            Con_Printf("What do you mean, '%c'?\n", buf[i]);
            break;
        }
    }

    return true;
}

D_CMD(CheatMassacre)
{
    Cht_MassacreFunc(NULL, CONSOLEPLAYER);
    return true;
}

D_CMD(CheatWhere)
{
    Cht_WhereFunc(NULL, CONSOLEPLAYER);
    return true;
}

D_CMD(CheatPig)
{
    if(IS_NETGAME)
        return false;

    if(!userGame || gameSkill == SM_NIGHTMARE || players[CONSOLEPLAYER].health <= 0)
        return false;

    Cht_PigFunc(NULL, CONSOLEPLAYER);
    return true;
}

D_CMD(CheatShadowcaster)
{
    int args[2];

    if(IS_NETGAME)
        return false;

    if(!userGame || gameSkill == SM_NIGHTMARE || players[CONSOLEPLAYER].health <= 0)
        return false;

    args[0] = atoi(argv[1]) + '0';
    Cht_ClassFunc2(args, CONSOLEPLAYER);
    return true;
}

D_CMD(CheatRunScript)
{
    int num, args[2];

    if(IS_NETGAME)
        return false;
    if(!userGame)
        return false;

    num = atoi(argv[1]);
    args[0] = num / 10 + '0';
    args[1] = num % 10 + '0';
    Cht_ScriptFunc3(args, CONSOLEPLAYER);
    return true;
}
