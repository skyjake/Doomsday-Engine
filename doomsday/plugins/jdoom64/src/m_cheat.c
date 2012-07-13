/**\file m_cheat.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * Cheats - Doom64 specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifdef UNIX
# include <errno.h>
#endif

#include "jdoom64.h"

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

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    unsigned char*  sequence;
    size_t          length, pos;
    int             args[2];
    int             currentArg;
} cheatseq_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Cht_LaserFunc(player_t* p);
void Cht_GodFunc(player_t* plr);
void Cht_GiveWeaponsFunc(player_t* plr);
void Cht_GiveAmmoFunc(player_t* plr);
void Cht_GiveKeysFunc(player_t* plr);
void Cht_NoClipFunc(player_t* plr);
void Cht_GiveArmorFunc(player_t* plr);
boolean Cht_WarpFunc(player_t* plr, cheatseq_t* cheat);
boolean Cht_PowerUpFunc(player_t* plr, cheatseq_t* cheat);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static boolean cheatsEnabled(void)
{
    return !IS_NETGAME;
}

void Cht_GodFunc(player_t* plr)
{
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
}

void Cht_SuicideFunc(player_t* plr)
{
    P_DamageMobj(plr->plr->mo, NULL, NULL, 10000, false);
}

void Cht_GiveArmorFunc(player_t* plr)
{
    plr->armorPoints = armorPoints[1];
    plr->armorType = armorClass[1];
    plr->update |= PSF_STATE | PSF_ARMOR_POINTS;
}

void Cht_GiveWeaponsFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_OWNED_WEAPONS;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
        plr->weapons[i].owned = true;
}

void Cht_GiveAmmoFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_AMMO;
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
        plr->ammo[i].owned = plr->ammo[i].max;
}

void Cht_GiveKeysFunc(player_t* plr)
{
    int i;

    plr->update |= PSF_KEYS;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
        plr->keys[i] = true;
}

void Cht_NoClipFunc(player_t* plr)
{
    plr->cheats ^= CF_NOCLIP;
    plr->update |= PSF_STATE;
    P_SetMessage(plr,
                 ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? STSTR_NCON : STSTR_NCOFF), false);
}

boolean Cht_WarpFunc(player_t* plr, cheatseq_t* cheat)
{
    uint epsd, map;

    epsd = 0;
    map = (cheat->args[0] - '0') * 10 + cheat->args[1] - '0';
    if(map != 0) map -= 1;

    // Catch invalid maps.
    if(!G_ValidateMap(&epsd, &map))
        return false;

    P_SetMessage(plr, STSTR_CLEV, false);

    // Clear the menu if open.
    Hu_MenuCommand(MCMD_CLOSEFAST);

    // So be it.
    briefDisabled = true;
    G_DeferredNewGame(gameSkill, epsd, map, 0/*default*/);

    return true;
}

boolean Cht_PowerUpFunc(player_t* plr, cheatseq_t* cheat)
{
    static const char args[] = { 'v', 's', 'i', 'r', 'a', 'l' };
    size_t i, numArgs = sizeof(args) / sizeof(args[0]);

    for(i = 0; i < numArgs; ++i)
    {
        powertype_t type;

        if(cheat->args[0] != args[i])
            continue;
        type = (powertype_t) i;

        if(!plr->powers[type])
        {
            P_GivePower(plr, type);
            P_SetMessage(plr, STSTR_BEHOLDX, false);
        }
        else if(type == PT_STRENGTH || type == PT_FLIGHT ||
                type == PT_ALLMAP)
        {
            P_TakePower(plr, type);
            P_SetMessage(plr, STSTR_BEHOLDX, false);
        }

        return true;
    }

    return false;
}

void printDebugInfo(player_t* plr)
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

    Con_Message("Player height:%g   Player radius:%g\n", plr->plr->mo->height, plr->plr->mo->radius);
}

/**
 * Laser powerup cheat code ddslia for all laser powerups.
 * Each time the plr enters the code, plr gains a powerup.
 * When entered again, plr recieves next powerup.
 */
void Cht_LaserFunc(player_t* p)
{
    if(P_InventoryGive(p - players, IIT_DEMONKEY1, true))
    {
        P_SetMessage(p, STSTR_BEHOLDX, false);
        return;
    }

    if(P_InventoryGive(p - players, IIT_DEMONKEY2, true))
    {
        P_SetMessage(p, STSTR_BEHOLDX, false);
        return;
    }

    if(P_InventoryGive(p - players, IIT_DEMONKEY3, true))
        P_SetMessage(p, STSTR_BEHOLDX, false);
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
            player_t* plr = &players[CONSOLEPLAYER];

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

            Cht_GodFunc(plr);
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
            player_t* plr = &players[CONSOLEPLAYER];

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

            Cht_NoClipFunc(&players[CONSOLEPLAYER]);
        }
    }
    return true;
}

static int suicideResponse(msgresponse_t response, int userValue, void* userPointer)
{
    if(response == MSG_YES)
    {
        if(IS_NETGAME && IS_CLIENT)
            NetCl_CheatRequest("suicide");
        else
            Cht_SuicideFunc(&players[CONSOLEPLAYER]);
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

        Cht_SuicideFunc(plr);
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
    cheatseq_t cheat;
    int num;

    if(!cheatsEnabled())
        return false;

    if(argc != 2)
        return false;

    num = atoi(argv[1]);
    cheat.args[0] = num / 10 + '0';
    cheat.args[1] = num % 10 + '0';

    Cht_WarpFunc(&players[CONSOLEPLAYER], &cheat);
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
            Cht_GiveAmmoFunc(plr);
            break;

        case 'b': {
            cheatseq_t cheat;
            cheat.args[0] = PT_STRENGTH;
            Cht_PowerUpFunc(plr, &cheat);
            break;
        }
        case 'f': {
            cheatseq_t cheat;
            cheat.args[0] = PT_FLIGHT;
            Cht_PowerUpFunc(plr, &cheat);
            break;
        }
        case 'g': {
            cheatseq_t cheat;
            cheat.args[0] = PT_INFRARED;
            Cht_PowerUpFunc(plr, &cheat);
            break;
        }
        case 'h':
            P_GiveBody(plr, healthLimit);
            break;

        case 'i': {
            cheatseq_t cheat;
            cheat.args[0] = PT_INVULNERABILITY;
            Cht_PowerUpFunc(plr, &cheat);
            break;
        }
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
            Cht_GiveKeysFunc(plr);
            break;

        case 'm': {
            cheatseq_t cheat;
            cheat.args[0] = PT_ALLMAP;
            Cht_PowerUpFunc(plr, &cheat);
            break;
        }
        case 'p':
            P_GiveBackpack(plr);
            break;

        case 'r':
            Cht_GiveArmorFunc(plr);
            break;

        case 's': {
            cheatseq_t cheat;
            cheat.args[0] = PT_IRONFEET;
            Cht_PowerUpFunc(plr, &cheat);
            break;
        }
        case 'v': {
            cheatseq_t cheat;
            cheat.args[0] = PT_INVISIBILITY;
            Cht_PowerUpFunc(plr, &cheat);
            break;
        }
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
                    P_GiveWeapon(plr, idx, false);
                    break;
                }
            }

            // Give all weapons.
            Cht_GiveWeaponsFunc(plr);
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
