/**\file m_cheat.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Cheat sequence checking.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef UNIX
# include <errno.h>
#endif

#include "jheretic.h"

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
#include "g_eventsequence.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int Cht_GodFunc(const int* args, int player);
int Cht_NoClipFunc(const int* args, int player);
int Cht_WeaponsFunc(const int* args, int player);
int Cht_PowerupFunc(const int* args, int player);
int Cht_HealthFunc(const int* args, int player);
int Cht_GiveKeysFunc(const int* args, int player);
int Cht_InvItem1Func(const int* args, int player);
int Cht_InvItem2Func(const int* args, int player);
int Cht_InvItem3Func(const int* args, int player);
int Cht_WarpFunc(const int* args, int player);
int Cht_ChickenFunc(const int* args, int player);
int Cht_MassacreFunc(const int* args, int player);
int Cht_IDKFAFunc(const int* args, int player);
int Cht_IDDQDFunc(const int* args, int player);
int Cht_SoundFunc(const int* args, int player);
int Cht_TickerFunc(const int* args, int player);
int Cht_RevealFunc(const int* args, int player);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Toggle god mode.
static char cheatGodSeq[] = {
    'q', 'u', 'i', 'c', 'k', 'e', 'n'
};

// Toggle no clipping mode.
static char cheatNoClipSeq[] = {
    'k', 'i', 't', 't', 'y'
};

// Get all weapons and ammo.
static char cheatWeaponsSeq[] = {
    'r', 'a', 'm', 'b', 'o'
};

// Toggle tome of power.
static char cheatPowerSeq[] = {
    's', 'h', 'a', 'z', 'a', 'm'
};

// Get full health.
static char cheatHealthSeq[] = {
    'p', 'o', 'n', 'c', 'e'
};

// Get all keys.
static char cheatKeysSeq[] = {
    's', 'k', 'e', 'l'
};

// Toggle sound debug info.
static char cheatSoundSeq[] = {
    'n', 'o', 'i', 's', 'e'
};

// Toggle ticker.
static char cheatTickerSeq[] = {
    't', 'i', 'c', 'k', 'e', 'r'
};

// Get an inventory item 1st stage (ask for type).
static char cheatInvItem1Seq[] = {
    'g', 'i', 'm', 'm', 'e'
};

// Get an inventory item 2nd stage (ask for count).
static char cheatInvItem2Seq[] = {
    'g', 'i', 'm', 'm', 'e', 1, 0
};

// Get an inventory item final stage.
static char cheatInvItem3Seq[] = {
    'g', 'i', 'm', 'm', 'e', 1, 0, 0
};

// Warp to new level.
static char cheatWarpSeq[] = {
    'e', 'n', 'g', 'a', 'g', 'e', 1, 0, 0
};

// Save a screenshot.
static char cheatChickenSeq[] = {
    'c', 'o', 'c', 'k', 'a', 'd', 'o', 'o', 'd', 'l', 'e', 'd', 'o', 'o'
};

// Kill all monsters.
static char cheatMassacreSeq[] = {
    'm', 'a', 's', 's', 'a', 'c', 'r', 'e'
};

static char cheatIDKFASeq[] = {
    'i', 'd', 'k', 'f', 'a'
};

static char cheatIDDQDSeq[] = {
    'i', 'd', 'd', 'q', 'd'
};

static char cheatAutomapSeq[] = {
    'r', 'a', 'v', 'm', 'a', 'p'
};

// CODE --------------------------------------------------------------------

static boolean cheatsEnabled(void)
{
    return !IS_NETGAME;
}

void Cht_Init(void)
{
    G_AddEventSequence(cheatAutomapSeq, sizeof(cheatAutomapSeq), Cht_RevealFunc);
    G_AddEventSequence(cheatGodSeq, sizeof(cheatGodSeq), Cht_GodFunc);
    G_AddEventSequence(cheatNoClipSeq, sizeof(cheatNoClipSeq), Cht_NoClipFunc);
    G_AddEventSequence(cheatWeaponsSeq, sizeof(cheatWeaponsSeq), Cht_WeaponsFunc);
    G_AddEventSequence(cheatPowerSeq, sizeof(cheatPowerSeq), Cht_PowerupFunc);
    G_AddEventSequence(cheatHealthSeq, sizeof(cheatHealthSeq), Cht_HealthFunc);
    G_AddEventSequence(cheatKeysSeq, sizeof(cheatKeysSeq), Cht_GiveKeysFunc);
    G_AddEventSequence(cheatSoundSeq, sizeof(cheatSoundSeq), Cht_SoundFunc);
    G_AddEventSequence(cheatTickerSeq, sizeof(cheatTickerSeq), Cht_TickerFunc);
    G_AddEventSequence(cheatInvItem3Seq, sizeof(cheatInvItem3Seq), Cht_InvItem3Func);
    G_AddEventSequence(cheatInvItem2Seq, sizeof(cheatInvItem2Seq), Cht_InvItem2Func);
    G_AddEventSequence(cheatInvItem1Seq, sizeof(cheatInvItem1Seq), Cht_InvItem1Func);
    G_AddEventSequence(cheatWarpSeq, sizeof(cheatWarpSeq), Cht_WarpFunc);
    G_AddEventSequence(cheatChickenSeq, sizeof(cheatChickenSeq), Cht_ChickenFunc);
    G_AddEventSequence(cheatMassacreSeq, sizeof(cheatMassacreSeq), Cht_MassacreFunc);
    G_AddEventSequence(cheatIDKFASeq, sizeof(cheatIDKFASeq), Cht_IDKFAFunc);
    G_AddEventSequence(cheatIDDQDSeq, sizeof(cheatIDDQDSeq), Cht_IDDQDFunc);
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
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

static void giveArmor(player_t* plr)
{
    plr->update |= PSF_ARMOR_POINTS | PSF_STATE;
    plr->armorPoints = 200;
    plr->armorType = 2;
}

static void giveWeapons(player_t* plr)
{
    int i;

    plr->update |= PSF_OWNED_WEAPONS;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        if(weaponInfo[i][0].mode[0].gameModeBits & gameModeBits)
            plr->weapons[i].owned = true;
    }
}

static void giveAmmo(player_t* plr)
{
    int i;

    plr->update |= PSF_MAX_AMMO | PSF_AMMO;
    if(!plr->backpack)
    {
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            plr->ammo[i].max *= 2;
        }
        plr->backpack = true;
    }

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        plr->ammo[i].owned = plr->ammo[i].max;
    }
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
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
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
    plr->keys[KT_YELLOW] = true;
    plr->keys[KT_GREEN] = true;
    plr->keys[KT_BLUE] = true;
    P_SetMessage(plr, TXT_CHEATKEYS, false);
    S_LocalSound(SFX_DORCLS, NULL);
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
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

int Cht_WarpFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    uint epsd, map;

    if(IS_NETGAME)
        return false;

    epsd = (args[0] > '0')? args[0] - '1' : 0;
    map = (args[1] > '0')? args[1] - '1' : 0;

    // Catch invalid maps.
    if(!G_ValidateMap(&epsd, &map))
        return false;

    P_SetMessage(plr, TXT_CHEATWARP, false);
    S_LocalSound(SFX_DORCLS, NULL);

    // Clear the menu if open.
    Hu_MenuCommand(MCMD_CLOSEFAST);

    // So be it.
    briefDisabled = true;
    G_DeferedInitNew(gameSkill, epsd, map, 0/* default*/);

    return true;
}

int Cht_PowerupFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    plr->update |= PSF_POWERS;
    if(plr->powers[PT_WEAPONLEVEL2])
    {
        plr->powers[PT_WEAPONLEVEL2] = 0;
        P_SetMessage(plr, TXT_CHEATPOWEROFF, false);
    }
    else
    {
        int plrnum = plr - players;

        P_InventoryGive(plrnum, IIT_TOMBOFPOWER, true);
        P_InventoryUse(plrnum, IIT_TOMBOFPOWER, true);
        P_SetMessage(plr, TXT_CHEATPOWERON, false);
    }
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

static void printDebugInfo(int player)
{
    player_t* plr = &players[player];
    char textBuffer[256];
    BspLeaf* sub;
    ddstring_t* path, *mapPath;
    Uri* uri, *mapUri;

    if(!plr->plr->mo || !userGame)
        return;

    mapUri = G_ComposeMapUri(gameEpisode, gameMap);
    mapPath = Uri_ToString(mapUri);
    sprintf(textBuffer, "MAP [%s]  X:%g  Y:%g  Z:%g",
            Str_Text(mapPath), plr->plr->mo->origin[VX], plr->plr->mo->origin[VY],
            plr->plr->mo->origin[VZ]);
    P_SetMessage(plr, textBuffer, false);
    Uri_Delete(mapUri);

    // Also print some information to the console.
    Con_Message("%s", textBuffer);
    sub = plr->plr->mo->bspLeaf;
    Con_Message("\nBspLeaf %i:\n", P_ToIndex(sub));

    uri = Materials_ComposeUri(P_GetIntp(sub, DMU_FLOOR_MATERIAL));
    path = Uri_ToString(uri);
    Con_Message("  FloorZ:%g Material:%s\n", P_GetDoublep(sub, DMU_FLOOR_HEIGHT), Str_Text(path));
    Str_Delete(path);
    Uri_Delete(uri);

    uri = Materials_ComposeUri(P_GetIntp(sub, DMU_CEILING_MATERIAL));
    path = Uri_ToString(uri);
    Con_Message("  CeilingZ:%g Material:%s\n", P_GetDoublep(sub, DMU_CEILING_HEIGHT), Str_Text(path));
    Str_Delete(path);
    Uri_Delete(uri);

    Con_Message("Player height:%g   Player radius:%g\n",
                plr->plr->mo->height, plr->plr->mo->radius);
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
        plr->health = plr->plr->mo->health = MAXCHICKENHEALTH;
    }
    else
    {
        plr->health = plr->plr->mo->health = maxHealth;
    }
    P_SetMessage(plr, TXT_CHEATHEALTH, false);
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

int Cht_InvItem1Func(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_SetMessage(plr, TXT_CHEATINVITEMS1, false);
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

int Cht_InvItem2Func(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_SetMessage(plr, TXT_CHEATINVITEMS2, false);
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

int Cht_InvItem3Func(const int* args, int player)
{
    player_t* plr = &players[player];
    int i, count;
    inventoryitemtype_t type;

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    type = args[0] - 'a' + 1;
    count = args[1] - '0';
    if(type > IIT_NONE && type < NUM_INVENTORYITEM_TYPES && count > 0 && count < 10)
    {
        if(gameMode == heretic_shareware && (type == IIT_SUPERHEALTH || type == IIT_TELEPORT))
        {
            P_SetMessage(plr, TXT_CHEATITEMSFAIL, false);
            return false;
        }

        for(i = 0; i < count; ++i)
        {
            P_InventoryGive(player, type, false);
        }
        P_SetMessage(plr, TXT_CHEATINVITEMS3, false);
    }
    else
    {   // Bad input
        P_SetMessage(plr, TXT_CHEATITEMSFAIL, false);
    }

    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

int Cht_ChickenFunc(const int* args, int player)
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
        if(P_UndoPlayerMorph(plr))
        {
            P_SetMessage(plr, TXT_CHEATCHICKENOFF, false);
        }
    }
    else if(P_MorphPlayer(plr))
    {
        P_SetMessage(plr, TXT_CHEATCHICKENON, false);
    }

    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

int Cht_MassacreFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_Massacre();
    P_SetMessage(plr, TXT_CHEATMASSACRE, false);
    S_LocalSound(SFX_DORCLS, NULL);
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
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

int Cht_IDDQDFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_DamageMobj(plr->plr->mo, NULL, plr->plr->mo, 10000, false);
    P_SetMessage(plr, TXT_CHEATIDDQD, false);
    S_LocalSound(SFX_DORCLS, NULL);
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

    // Otherwise ignored.
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

int Cht_TickerFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    // Otherwise ignored.
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

int Cht_RevealFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    if(IS_NETGAME && deathmatch)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.
    if(ST_AutomapIsActive(player))
    {
        ST_CycleAutomapCheatLevel(player);
    }
    return true;
}

// This is the multipurpose cheat ccmd.
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
    if(G_GameState() == GS_MAP)
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
    if(G_GameState() == GS_MAP)
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

static int suicideResponse(msgresponse_t response, int userValue, void* userPointer)
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
    if(G_GameState() == GS_MAP)
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
            Hu_MsgStart(MSG_YESNO, SUICIDEASK, suicideResponse, 0, NULL);
            return true;
        }

        P_DamageMobj(plr->plr->mo, NULL, NULL, 10000, false);
        return true;
    }
    else
    {
        Hu_MsgStart(MSG_ANYKEY, SUICIDEOUTMAP, NULL, 0, NULL);
    }

    return true;
}

D_CMD(CheatWarp)
{
    int num, args[2];

    if(!cheatsEnabled())
        return false;

    if(argc == 2)
    {
        num = atoi(argv[1]);
        args[0] = num / 10 + '0';
        args[1] = num % 10 + '0';
    }
    else if(argc == 3)
    {
        args[0] = atoi(argv[1]) % 10 + '0';
        args[1] = atoi(argv[2]) % 10 + '0';
    }
    else
    {
        Con_Printf("Usage: warp (num)\n");
        return true;
    }

    Cht_WarpFunc(args, CONSOLEPLAYER);
    return true;
}

D_CMD(CheatReveal)
{
    int option, i;

    if(!cheatsEnabled())
        return false;

    option = atoi(argv[1]);
    if(option < 0 || option > 3)
        return false;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_SetAutomapCheatLevel(i, 0);
        ST_RevealAutomap(i, false);
        if(option == 1)
            ST_RevealAutomap(i, true);
        else if(option != 0)
            ST_SetAutomapCheatLevel(i, option -1);
    }

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
        Con_Printf(" p - backpack full of ammo\n");
        Con_Printf(" r - armor\n");
        Con_Printf(" t - tome of power\n");
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

    if(G_GameState() != GS_MAP)
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
                    plr->ammo[idx].owned = plr->ammo[idx].max;
                    break;
                }
            }

            // Give all ammo.
            plr->update |= PSF_AMMO;
            { int j;
            for(j = 0; j < NUM_AMMO_TYPES; ++j)
                plr->ammo[j].owned = plr->ammo[j].max;
            }
            break;

        case 'i': // Inventory items.
            if(i < stuffLen)
            {
                char* end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < IIT_FIRST || idx >= NUM_INVENTORYITEM_TYPES)
                    {
                        Con_Printf("Unknown item #%d (valid range %d-%d).\n",
                                   (int)idx, IIT_FIRST, NUM_INVENTORYITEM_TYPES-1);
                        break;
                    }

                    // Give one specific item type.
                    if(!(gameMode == heretic_shareware &&
                         (idx == IIT_SUPERHEALTH || idx == IIT_TELEPORT)))
                    {
                        int j;
                        for(j = 0; j < MAXINVITEMCOUNT; ++j)
                            P_InventoryGive(player, idx, false);
                    }
                    break;
                }
            }

            // Give all inventory items.
            { inventoryitemtype_t type;
            for(type = IIT_FIRST; type < NUM_INVENTORYITEM_TYPES; ++type)
            {
                if(gameMode == heretic_shareware &&
                   (type == IIT_SUPERHEALTH || type == IIT_TELEPORT))
                    continue;

                { int i;
                for(i = 0; i < MAXINVITEMCOUNT; ++i)
                    P_InventoryGive(player, type, false);
                }
            }}
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
                    plr->keys[idx] = true;
                    break;
                }
            }

            // Give all keys.
            Cht_GiveKeysFunc(NULL, player);
            break;

        case 'p':
            if(!plr->backpack)
            {
                int j;
                for(j = AT_FIRST; j < NUM_AMMO_TYPES; ++j)
                    plr->ammo[j].max *= 2;
                plr->backpack = true;
                plr->update |= PSF_MAX_AMMO;
            }

            // Set all ammo to max.
            plr->update |= PSF_AMMO;
            { int j;
            for(j = 0; j < NUM_AMMO_TYPES; ++j)
                plr->ammo[j].owned = plr->ammo[j].max;
            }
            break;

        case 'r':
            plr->update |= PSF_ARMOR_POINTS;
            plr->armorPoints = 200;
            plr->armorType = 2;
            break;

        case 't':
            Cht_PowerupFunc(NULL, player);
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
                    if(weaponInfo[idx][0].mode[0].gameModeBits & gameModeBits)
                    {
                        plr->update |= PSF_OWNED_WEAPONS;
                        plr->weapons[idx].owned = true;
                    }
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
    printDebugInfo(CONSOLEPLAYER);
    return true;
}

/**
 * Exit the current map and go to the intermission.
 */
D_CMD(CheatLeaveMap)
{
    if(!cheatsEnabled())
        return false;

    if(G_GameState() != GS_MAP)
    {
        S_LocalSound(SFX_CHAT, NULL);
        Con_Printf("Can only exit a map when in a game!\n");
        return true;
    }

    G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
    return true;
}

D_CMD(CheatPig)
{
    if(IS_NETGAME)
        return false;

    if(!userGame || gameSkill == SM_NIGHTMARE || players[CONSOLEPLAYER].health <= 0)
        return false;

    Cht_ChickenFunc(NULL, CONSOLEPLAYER);
    return true;
}
