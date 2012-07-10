/**\file m_cheat.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * Cheats - DOOM specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef UNIX
# include <errno.h>
#endif

#include "jdoom.h"

#include "d_net.h"
#include "g_common.h"
#include "p_player.h"
#include "am_map.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "dmu_lib.h"
#include "p_user.h"
#include "p_start.h"
#include "g_eventsequence.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int Cht_GodFunc(const int* args, int player);
int Cht_NoClipFunc(const int* args, int player);
int Cht_ChoppersFunc(const int* args, int player);
int Cht_MyPosFunc(const int* args, int player);
int Cht_GiveWeaponsAmmoArmorKeys(const int* args, int player);
int Cht_GiveWeaponsAmmoArmor(const int* args, int player);
int Cht_WarpFunc(const int* args, int player);
int Cht_MusicFunc(const int* args, int player);
int Cht_PowerupMessage(const int* args, int player);
int Cht_PowerupFunc(const int* args, int player);
int Cht_PowerupFunc1(const int* args, int player);
int Cht_PowerupFunc2(const int* args, int player);
int Cht_PowerupFunc3(const int* args, int player);
int Cht_PowerupFunc4(const int* args, int player);
int Cht_PowerupFunc5(const int* args, int player);
int Cht_PowerupFunc6(const int* args, int player);
int Cht_Reveal(const int* args, int player);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static boolean cheatsEnabled(void)
{
#ifdef _DEBUG
    if(IS_NETWORK_SERVER) return true; // Server operator can always cheat.
#endif
    return !IS_NETGAME;
}

void Cht_Init(void)
{
    switch(gameMode)
    {
    case doom2_hacx:
        { char seq[] = { 'w', 'a', 'r', 'p', 'm', 'e', 1, 0, 0 };
        G_AddEventSequence(seq, 9, Cht_WarpFunc); }

        { char seq[] = { 's', 'e', 'e', 'i', 't', 1, 0 };
        G_AddEventSequence(seq, 7, Cht_PowerupFunc); }

        { char seq[] = { 't', 'u', 'n', 'e', 's', 1, 0, 0 };
        G_AddEventSequence(seq, 8, Cht_MusicFunc); }

        G_AddEventSequence("show",          4, Cht_Reveal);
        G_AddEventSequence("wuss",          4, Cht_GodFunc);
        G_AddEventSequence("blast",         5, Cht_GiveWeaponsAmmoArmorKeys);
        G_AddEventSequence("walk",          4, Cht_NoClipFunc);
        G_AddEventSequence("zap",           3, Cht_ChoppersFunc);
        G_AddEventSequence("wheream",       7, Cht_MyPosFunc);
        G_AddEventSequence("superman",      8, Cht_PowerupFunc1);
        G_AddEventSequence("whacko",        6, Cht_PowerupFunc2);
        G_AddEventSequence("ghost",         5, Cht_PowerupFunc3);
        G_AddEventSequence("boots",         5, Cht_PowerupFunc4);
        // G_AddEventSequence(??,              ?, Cht_PowerupFunc5); Unknown??
        G_AddEventSequence("bright",        6, Cht_PowerupFunc6);
        break;

    case doom_chex:
        { char seq[] = { 'l', 'e', 'e', 's', 'n', 'y', 'd', 'e', 'r', 1, 0, 0 };
        G_AddEventSequence(seq, 12, Cht_WarpFunc); }

        { char seq[] = { 'i', 'd', 'm', 'u', 's', 1, 0, 0 };
        G_AddEventSequence(seq, 8, Cht_MusicFunc); }

        G_AddEventSequence("joelkoenigs",   11, Cht_ChoppersFunc);
        G_AddEventSequence("davidbrus",     9,  Cht_GodFunc);
        G_AddEventSequence("scottholman",   11, Cht_GiveWeaponsAmmoArmorKeys);
        G_AddEventSequence("mikekoenigs",   11, Cht_GiveWeaponsAmmoArmor);
        G_AddEventSequence("charlesjacobi", 13, Cht_NoClipFunc);
        G_AddEventSequence("kimhyers",      8,  Cht_MyPosFunc);
        G_AddEventSequence("sherrill",      8,  Cht_Reveal);
        G_AddEventSequence("andrewbenson",  12, Cht_PowerupFunc1);
        G_AddEventSequence("deanhyers",     9,  Cht_PowerupFunc2);
        G_AddEventSequence("marybregi",     9,  Cht_PowerupFunc3);
        G_AddEventSequence("allen",         5,  Cht_PowerupFunc4);
        G_AddEventSequence("digitalcafe",   11, Cht_PowerupFunc5);
        G_AddEventSequence("joshuastorms",  12, Cht_PowerupFunc6);
        break;

    default: // Doom
        { char seq[] = { 'i', 'd', 'c', 'l', 'e', 'v', 1, 0, 0 };
        G_AddEventSequence(seq, 9, Cht_WarpFunc); }

        { char seq[] = { 'i', 'd', 'b', 'e', 'h', 'o', 'l', 'd', 1, 0 };
        G_AddEventSequence(seq, 10, Cht_PowerupFunc); }

        { char seq[] = { 'i', 'd', 'm', 'u', 's', 1, 0, 0 };
        G_AddEventSequence(seq, 8, Cht_MusicFunc); }

        G_AddEventSequence("iddt",          4,  Cht_Reveal);
        G_AddEventSequence("iddqd",         5,  Cht_GodFunc);
        G_AddEventSequence("idfa",          4,  Cht_GiveWeaponsAmmoArmor);
        G_AddEventSequence("idkfa",         5,  Cht_GiveWeaponsAmmoArmorKeys);
        G_AddEventSequence("idclip",        6,  Cht_NoClipFunc);
        G_AddEventSequence("idspispopd",    10, Cht_NoClipFunc);
        G_AddEventSequence("idchoppers",    10, Cht_ChoppersFunc);
        G_AddEventSequence("idmypos",       7,  Cht_MyPosFunc);
        break;
    }
}

/**
 * 'iddqd' cheat for toggleable god mode.
 */
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

    if(P_GetPlayerCheats(plr) & CF_GODMODE)
    {
        if(plr->plr->mo)
            plr->plr->mo->health = maxHealth;
        plr->health = godModeHealth;
        plr->update |= PSF_HEALTH;
    }

    P_SetMessage(plr,
                 ((P_GetPlayerCheats(plr) & CF_GODMODE) ? STSTR_DQDON : STSTR_DQDOFF), false);
    return true;
}

static void giveArmor(int player, int val)
{
    player_t* plr = &players[player];

    // Support idfa/idkfa DEH Misc values
    val = MINMAX_OF(1, val, 3);
    plr->armorPoints = armorPoints[val];
    plr->armorType = armorClass[val];

    plr->update |= PSF_STATE | PSF_ARMOR_POINTS;
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
        plr->ammo[i].owned = plr->ammo[i].max;
}

static void giveKeys(player_t* plr)
{
    int i;

    plr->update |= PSF_KEYS;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
        plr->keys[i] = true;
}

/**
 * 'idfa' cheat for killer fucking arsenal.
 */
int Cht_GiveWeaponsAmmoArmor(const int* args, int player)
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
    giveArmor(player, 2);

    P_SetMessage(plr, STSTR_FAADDED, false);
    return true;
}

/**
 * 'idkfa' cheat for key full ammo.
 */
int Cht_GiveWeaponsAmmoArmorKeys(const int* args, int player)
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
    giveKeys(plr);
    giveArmor(player, 3);

    P_SetMessage(plr, STSTR_KFAADDED, false);
    return true;
}

/**
 * 'idmus' cheat for changing music.
 */
int Cht_MusicFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    int musnum;

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    if(gameModeBits & GM_ANY_DOOM2)
        musnum = (args[0] - '0') * 10 + (args[1] - '0');
    else
        musnum = (args[0] - '1') * 9  + (args[1] - '0');

    if(S_StartMusicNum(musnum, true))
    {
        P_SetMessage(plr, STSTR_MUS, false);
        return true;
    }

    P_SetMessage(plr, STSTR_NOMUS, false);
    return false;
}

/**
 * 'idclip' no clipping mode cheat.
 */
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
                 ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? STSTR_NCON : STSTR_NCOFF), false);
    return true;
}

/**
 * 'clev' change map cheat.
 */
int Cht_WarpFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    uint epsd, map;

    if(IS_NETGAME)
        return false;

    if(gameModeBits & GM_ANY_DOOM2)
    {
        epsd = 0;
        map = (args[0] - '0') * 10 + args[1] - '0';
        if(map != 0) map -= 1;
    }
    else
    {
        epsd = (args[0] > '0')? args[0] - '1' : 0;
        map  = (args[1] > '0')? args[1] - '1' : 0;
    }

    // Catch invalid maps.
    if(!G_ValidateMap(&epsd, &map))
        return false;

    P_SetMessage(plr, STSTR_CLEV, false);

    // Clear the menu if open.
    Hu_MenuCommand(MCMD_CLOSEFAST);

    // So be it.
    briefDisabled = true;
    G_DeferedInitNew(gameSkill, epsd, map);

    return true;
}

int Cht_Reveal(const int* args, int player)
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

/**
 * 'idbehold' power-up menu
 */
int Cht_PowerupMessage(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_SetMessage(plr, STSTR_BEHOLD, false);
    return true;
}

static void givePower(player_t* plr, powertype_t type)
{
    if(type < 0 && type >= NUM_POWER_TYPES)
        return;

    if(!plr->powers[type])
    {
        P_GivePower(plr, type);
    }
    else if(type == PT_STRENGTH || type == PT_FLIGHT || type == PT_ALLMAP)
    {
        P_TakePower(plr, type);
    }
}

/**
 * 'idbehold?' power-up cheats.
 */
int Cht_PowerupFunc(const int* args, int player)
{
    static const char values[] = { 'v', 's', 'i', 'r', 'a', 'l' };
    static const size_t numValues = sizeof(values) / sizeof(values[0]);

    player_t* plr = &players[player];
    size_t i;

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    for(i = 0; i < numValues; ++i)
    {
        if(args[0] != values[i])
            continue;
        givePower(plr, (powertype_t) i);
        P_SetMessage(plr, STSTR_BEHOLDX, false);
        return true;
    }

    return false;
}

int Cht_PowerupFunc1(const int* args, int player)
{
    int _args[1] = {'v'};
    return Cht_PowerupFunc(_args, player);
}

int Cht_PowerupFunc2(const int* args, int player)
{
    int _args[1] = {'s'};
    return Cht_PowerupFunc(_args, player);
}

int Cht_PowerupFunc3(const int* args, int player)
{
    int _args[1] = {'i'};
    return Cht_PowerupFunc(_args, player);
}

int Cht_PowerupFunc4(const int* args, int player)
{
    int _args[1] = {'r'};
    return Cht_PowerupFunc(_args, player);
}

int Cht_PowerupFunc5(const int* args, int player)
{
    int _args[1] = {'a'};
    return Cht_PowerupFunc(_args, player);
}

int Cht_PowerupFunc6(const int* args, int player)
{
    int _args[1] = {'l'};
    return Cht_PowerupFunc(_args, player);
}

/**
 * 'choppers' invulnerability & chainsaw
 */
int Cht_ChoppersFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    plr->weapons[WT_EIGHTH].owned = true;
    plr->powers[PT_INVULNERABILITY] = true;
    P_SetMessage(plr, STSTR_CHOPPERS, false);
    return true;
}

/**
 * 'mypos' for plr position
 */
int Cht_MyPosFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    char buf[80];

    if(IS_NETGAME)
        return false;
    if(gameSkill == SM_NIGHTMARE)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    sprintf(buf, "ang=0x%x;x,y,z=(%g,%g,%g)",
            players[CONSOLEPLAYER].plr->mo->angle,
            players[CONSOLEPLAYER].plr->mo->origin[VX],
            players[CONSOLEPLAYER].plr->mo->origin[VY],
            players[CONSOLEPLAYER].plr->mo->origin[VZ]);
    P_SetMessage(plr, buf, false);
    return true;
}

static void printDebugInfo(player_t* plr)
{
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
    Str_Delete(mapPath);
    Uri_Delete(mapUri);

    // Also print some information to the console.
    Con_Message("%s", textBuffer);
    sub = plr->plr->mo->bspLeaf;
    Con_Message("\nBspLeaf %i / Sector %i:\n", P_ToIndex(sub), P_ToIndex(P_GetPtrp(sub, DMU_SECTOR)));

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

/**
 * This is the multipurpose cheat ccmd.
 */
D_CMD(Cheat)
{
    size_t i, len = strlen(argv[1]);

    // Give each of the characters in argument two to the ST event handler.
    for(i = 0; i < len; ++i)
    {
        event_t ev;
        memset(&ev, 0, sizeof(ev));
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

        /*
        if(IS_NETGAME && !netSvAllowCheats)
            return false;
        */

        if(IS_CLIENT || argc != 2)
        {
            plr = &players[CONSOLEPLAYER];
        }
        else
        {
            int i = atoi(argv[1]);
            if(i < 0 || i >= MAXPLAYERS) return false;
            plr = &players[i];
        }

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
    int args[2];

    if(!cheatsEnabled())
        return false;

    if(gameModeBits & GM_ANY_DOOM2)
    {
        int num = atoi(argv[1]);
        args[0] = num / 10 + '0';
        args[1] = num % 10 + '0';
    }
    else
    {
        if(argc == 3)
        {
            args[0] = (int) argv[1][0];
            args[1] = (int) argv[2][0];
        }
        else
        {
            args[0] = 0;
            args[1] = (int) argv[1][0];
        }
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
    char buf[100];
    int player = CONSOLEPLAYER;
    player_t* plr;
    size_t i, stuffLen;

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
        Con_Printf(" b - berserk\n");
        Con_Printf(" f - the power of flight\n");
        Con_Printf(" g - light amplification visor\n");
        Con_Printf(" h - health\n");
        Con_Printf(" i - invulnerability\n");
        Con_Printf(" k - key cards/skulls\n");
        Con_Printf(" m - computer area map\n");
        Con_Printf(" p - backpack full of ammo\n");
        Con_Printf(" r - armor\n");
        Con_Printf(" s - radiation shielding suit\n");
        Con_Printf(" v - invisibility\n");
        Con_Printf(" w - weapons\n");
        Con_Printf("Example: 'give arw' corresponds the cheat IDFA.\n");
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
        return true; // Can't give to a plr who's not playing
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
            giveAmmo(plr);
            break;

        case 'b':
            givePower(plr, PT_STRENGTH);
            break;

        case 'f':
            givePower(plr, PT_FLIGHT);
            break;

        case 'g':
            givePower(plr, PT_INFRARED);
            break;

        case 'h':
            P_GiveBody(plr, healthLimit);
            break;

        case 'i':
            givePower(plr, PT_INVULNERABILITY);
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
            giveKeys(plr);
            break;

        case 'm':
            givePower(plr, PT_ALLMAP);
            break;

        case 'p':
            P_GiveBackpack(plr);
            break;

        case 'r':
            giveArmor(player, 1);
            break;

        case 's':
            givePower(plr, PT_IRONFEET);
            break;

        case 'v':
            givePower(plr, PT_INVISIBILITY);
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
                    P_GiveWeapon(plr, idx, false, NULL, SFX_WPNUP);
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
    Con_Printf("%i monsters killed.\n", P_Massacre());
    return true;
}

D_CMD(CheatWhere)
{
    printDebugInfo(&players[CONSOLEPLAYER]);
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
        S_LocalSound(SFX_OOF, NULL);
        Con_Printf("Can only exit a map when in a game!\n");
        return true;
    }

    G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
    return true;
}
