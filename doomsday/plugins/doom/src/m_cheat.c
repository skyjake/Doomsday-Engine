/** @file m_cheat.c Cheat code sequences
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#include <stdlib.h>
#include <errno.h>

#include "jdoom.h"

#include "d_net.h"
#include "p_player.h"
#include "am_map.h"
#include "hu_msg.h"
#include "dmu_lib.h"
#include "p_user.h"
#include "g_eventsequence.h"

typedef eventsequencehandler_t cheatfunc_t;

/// Helper macro for forming cheat callback function names.
#define CHEAT(x) G_Cheat##x

/// Helper macro for declaring cheat callback functions.
#define CHEAT_FUNC(x) int G_Cheat##x(int player, const EventSequenceArg* args, int numArgs)

/// Helper macro for registering new cheat event sequence handlers.
#define ADDCHEAT(name, callback) G_AddEventSequence((name), CHEAT(callback))

/// Helper macro for registering new cheat event sequence command handlers.
#define ADDCHEATCMD(name, cmdTemplate) G_AddEventSequenceCommand((name), cmdTemplate)

CHEAT_FUNC(Music);
CHEAT_FUNC(MyPos);
CHEAT_FUNC(Powerup2);
CHEAT_FUNC(Powerup);
CHEAT_FUNC(Reveal);

void G_RegisterCheats(void)
{
    switch(gameMode)
    {
    case doom2_hacx:
        ADDCHEATCMD("blast",            "give wakr3 %p");
        ADDCHEATCMD("boots",            "give s %p");
        ADDCHEATCMD("bright",           "give g %p");
        ADDCHEATCMD("ghost",            "give v %p");
        ADDCHEAT("seeit%1",             Powerup2);
        ADDCHEAT("seeit",               Powerup);
        ADDCHEAT("show",                Reveal);
        ADDCHEATCMD("superman",         "give i %p");
        ADDCHEAT("tunes%1%2",           Music);
        ADDCHEATCMD("walk",             "noclip %p");
        ADDCHEATCMD("warpme%1%2",       "warp %1%2");
        ADDCHEATCMD("whacko",           "give b %p");
        ADDCHEAT("wheream",             MyPos);
        ADDCHEATCMD("wuss",             "god %p");
        ADDCHEATCMD("zap",              "give w7 %p");
        break;

    case doom_chex:
        ADDCHEATCMD("allen",            "give s %p");
        ADDCHEATCMD("andrewbenson",     "give i %p");
        ADDCHEATCMD("charlesjacobi",    "noclip %p");
        ADDCHEATCMD("davidbrus",        "god %p");
        ADDCHEATCMD("deanhyers",        "give b %p");
        ADDCHEATCMD("digitalcafe",      "give m %p");
        ADDCHEAT("idmus%1%2",           Music);
        ADDCHEATCMD("joelkoenigs",      "give w7 %p");
        ADDCHEATCMD("joshuastorms",     "give g %p");
        ADDCHEAT("kimhyers",            MyPos);
        ADDCHEATCMD("leesnyder%1%2",    "warp %1%2");
        ADDCHEATCMD("marybregi",        "give v %p");
        ADDCHEATCMD("mikekoenigs",      "give war2 %p");
        ADDCHEATCMD("scottholman",      "give wakr3 %p");
        ADDCHEAT("sherrill",            Reveal);
        break;

    default: // Doom
        ADDCHEAT("idbehold%1",          Powerup2);
        ADDCHEAT("idbehold",            Powerup);

        // Note that in vanilla this cheat enables invulnerability until the
        // end of the current tic.
        ADDCHEATCMD("idchoppers",       "give w7 %p");

        ADDCHEATCMD("idclev%1%2",       "warp %1%2");
        ADDCHEATCMD("idclip",           "noclip %p");
        ADDCHEATCMD("iddqd",            "god %p");
        ADDCHEAT("iddt",                Reveal);
        ADDCHEATCMD("idfa",             "give war2 %p");
        ADDCHEATCMD("idkfa",            "give wakr3 %p");
        ADDCHEAT("idmus%1%2",           Music);
        ADDCHEAT("idmypos",             MyPos);
        ADDCHEATCMD("idspispopd",       "noclip %p");
        break;
    }
}

CHEAT_FUNC(Music)
{
    player_t *plr = &players[player];
    int musnum;

    DENG_ASSERT(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(gameModeBits & GM_ANY_DOOM2)
        musnum = (args[0] - '0') * 10 + (args[1] - '0');
    else
        musnum = (args[0] - '1') * 9  + (args[1] - '0');

    if(S_StartMusicNum(musnum, true))
    {
        P_SetMessage(plr, LMF_NO_HIDE, STSTR_MUS);
        return true;
    }

    P_SetMessage(plr, LMF_NO_HIDE, STSTR_NOMUS);
    return false;
}

CHEAT_FUNC(Reveal)
{
    player_t *plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME && deathmatch) return false;

    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    if(ST_AutomapIsActive(player))
    {
        ST_CycleAutomapCheatLevel(player);
    }
    return true;
}

CHEAT_FUNC(Powerup)
{
    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    P_SetMessage(&players[player], LMF_NO_HIDE, STSTR_BEHOLD);
    return true;
}

CHEAT_FUNC(Powerup2)
{
    struct mnemonic_pair_s {
        char vanilla;
        char give;
    } mnemonics[] =
    {
        /*PT_INVULNERABILITY*/  { 'v', 'i' },
        /*PT_STRENGTH*/         { 's', 'b' },
        /*PT_INVISIBILITY*/     { 'i', 'v' },
        /*PT_IRONFEET*/         { 'r', 's' },
        /*PT_ALLMAP*/           { 'a', 'm' },
        /*PT_INFRARED*/         { 'l', 'g' }
    };
    static const int numMnemonics = (int)(sizeof(mnemonics) / sizeof(mnemonics[0]));
    int i;

    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    for(i = 0; i < numMnemonics; ++i)
    {
        if(args[0] == mnemonics[i].vanilla)
        {
            DD_Executef(true, "give %c %i", mnemonics[i].give, player);
            return true;
        }
    }
    return false;
}

CHEAT_FUNC(MyPos)
{
    player_t *plr = &players[player];
    char buf[80];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    sprintf(buf, "ang=0x%x;x,y,z=(%g,%g,%g)",
            players[CONSOLEPLAYER].plr->mo->angle,
            players[CONSOLEPLAYER].plr->mo->origin[VX],
            players[CONSOLEPLAYER].plr->mo->origin[VY],
            players[CONSOLEPLAYER].plr->mo->origin[VZ]);
    P_SetMessage(plr, LMF_NO_HIDE, buf);

    return true;
}

/**
 * The multipurpose cheat ccmd.
 */
D_CMD(Cheat)
{
    // Give each of the characters in argument two to the ST event handler.
    int i, len = (int)strlen(argv[1]);
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
        else if((IS_NETGAME && !netSvAllowCheats) || gameSkill == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            player_t *plr;

            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            plr = &players[player];
            if(!plr->plr->inGame) return false;

            // Dead players can't cheat.
            if(plr->health <= 0) return false;

            plr->cheats ^= CF_GODMODE;
            plr->update |= PSF_STATE;

            if(P_GetPlayerCheats(plr) & CF_GODMODE)
            {
                if(plr->plr->mo)
                    plr->plr->mo->health = maxHealth;
                plr->health = godModeHealth;
                plr->update |= PSF_HEALTH;
            }

            P_SetMessage(plr, LMF_NO_HIDE, ((P_GetPlayerCheats(plr) & CF_GODMODE) ? STSTR_DQDON : STSTR_DQDOFF));
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
        else if((IS_NETGAME && !netSvAllowCheats) || gameSkill == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            player_t *plr;

            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            plr = &players[player];
            if(!plr->plr->inGame) return false;

            // Dead players can't cheat.
            if(plr->health <= 0) return false;

            plr->cheats ^= CF_NOCLIP;
            plr->update |= PSF_STATE;
            P_SetMessage(plr, LMF_NO_HIDE, ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? STSTR_NCON : STSTR_NCOFF));
        }
    }
    return true;
}

static int suicideResponse(msgresponse_t response, int userValue, void *userPointer)
{
    DENG_UNUSED(userValue);
    DENG_UNUSED(userPointer);
    if(response == MSG_YES)
    {
        if(IS_NETGAME && IS_CLIENT)
        {
            NetCl_CheatRequest("suicide");
        }
        else
        {
            player_t *plr = &players[CONSOLEPLAYER];
            P_DamageMobj(plr->plr->mo, NULL, NULL, 10000, false);
        }
    }
    return true;
}

D_CMD(CheatSuicide)
{
    if(G_GameState() == GS_MAP)
    {
        player_t *plr;

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

        if(!plr->plr->inGame) return false;
        if(plr->playerState == PST_DEAD) return false;

        if(!IS_NETGAME || IS_CLIENT)
        {
            Hu_MsgStart(MSG_YESNO, SUICIDEASK, suicideResponse, 0, NULL);
        }
        else
        {
            P_DamageMobj(plr->plr->mo, NULL, NULL, 10000, false);
        }
        return true;
    }

    Hu_MsgStart(MSG_ANYKEY, SUICIDEOUTMAP, NULL, 0, NULL);
    return true;
}

D_CMD(CheatReveal)
{
    int option, i;

    // Server operator can always reveal.
    if(IS_NETGAME && !IS_NETWORK_SERVER)
        return false;

    option = atoi(argv[1]);
    if(option < 0 || option > 3) return false;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_SetAutomapCheatLevel(i, 0);
        ST_RevealAutomap(i, false);
        if(option == 1)
        {
            ST_RevealAutomap(i, true);
        }
        else if(option != 0)
        {
            ST_SetAutomapCheatLevel(i, option -1);
        }
    }

    return true;
}

static void giveWeapon(player_t *player, weapontype_t weaponType)
{
    P_GiveWeapon(player, weaponType, false/*not dropped*/);
    if(weaponType == WT_EIGHTH)
        P_SetMessage(player, LMF_NO_HIDE, STSTR_CHOPPERS);
}

static void togglePower(player_t *player, powertype_t powerType)
{
    P_TogglePower(player, powerType);
    P_SetMessage(player, LMF_NO_HIDE, STSTR_BEHOLDX);
}

D_CMD(CheatGive)
{
    char buf[100];
    int player = CONSOLEPLAYER;
    player_t *plr;
    size_t i, stuffLen;

    if(G_GameState() != GS_MAP)
    {
        App_Log(DE2_LOG_SCR_ERROR, "Can only \"give\" when in a game!\n");
        return true;
    }

    if(argc != 2 && argc != 3)
    {
        App_Log(DE2_LOG_SCR_NOTE, "Usage:\n  give (stuff)\n");
        App_Log(DE2_LOG_SCR, "  give (stuff) (plr)\n");
        App_Log(DE2_LOG_SCR, "Stuff consists of one or more of (type:id). "
                "If no id; give all of type:\n");
        App_Log(DE2_LOG_SCR, " a - ammo\n");
        App_Log(DE2_LOG_SCR, " b - berserk\n");
        App_Log(DE2_LOG_SCR, " f - the power of flight\n");
        App_Log(DE2_LOG_SCR, " g - light amplification visor\n");
        App_Log(DE2_LOG_SCR, " h - health\n");
        App_Log(DE2_LOG_SCR, " i - invulnerability\n");
        App_Log(DE2_LOG_SCR, " k - key cards/skulls\n");
        App_Log(DE2_LOG_SCR, " m - computer area map\n");
        App_Log(DE2_LOG_SCR, " p - backpack full of ammo\n");
        App_Log(DE2_LOG_SCR, " r - armor\n");
        App_Log(DE2_LOG_SCR, " s - radiation shielding suit\n");
        App_Log(DE2_LOG_SCR, " v - invisibility\n");
        App_Log(DE2_LOG_SCR, " w - weapons\n");
        App_Log(DE2_LOG_SCR, "Example: 'give arw' corresponds the cheat IDFA.\n");
        App_Log(DE2_LOG_SCR, "Example: 'give w2k1' gives weapon two and key one.\n");
        return true;
    }

    if(argc == 3)
    {
        player = atoi(argv[2]);
        if(player < 0 || player >= MAXPLAYERS) return false;
    }

    if(IS_CLIENT)
    {
        if(argc < 2) return false;

        sprintf(buf, "give %s", argv[1]);
        NetCl_CheatRequest(buf);
        return true;
    }

    if((IS_NETGAME && !netSvAllowCheats) || gameSkill == SM_NIGHTMARE)
        return false;

    plr = &players[player];

    // Can't give to a player who's not in the game.
    if(!plr->plr->inGame) return false;

    // Can't give to a dead player.
    if(plr->health <= 0) return false;

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
                char *end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < AT_FIRST || idx >= NUM_AMMO_TYPES)
                    {
                        App_Log(DE2_LOG_SCR_ERROR, "Unknown ammo #%d (valid range %d-%d)\n",
                                (int)idx, AT_FIRST, NUM_AMMO_TYPES-1);
                        break;
                    }

                    // Give one specific ammo type.
                    P_GiveAmmo(plr, (ammotype_t)idx, -1 /*max rounds*/);
                    break;
                }
            }

            // Give all ammo.
            P_GiveAmmo(plr, NUM_AMMO_TYPES /*all types*/, -1 /*max rounds*/);
            break;

        case 'b':
            togglePower(plr, PT_STRENGTH);
            break;

        case 'f':
            togglePower(plr, PT_FLIGHT);
            break;

        case 'g':
            togglePower(plr, PT_INFRARED);
            break;

        case 'h':
            P_GiveHealth(plr, healthLimit);
            break;

        case 'i':
            togglePower(plr, PT_INVULNERABILITY);
            break;

        case 'k':
            if(i < stuffLen)
            {
                char *end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < KT_FIRST || idx >= NUM_KEY_TYPES)
                    {
                        App_Log(DE2_LOG_SCR_ERROR, "Unknown key #%d (valid range %d-%d)\n",
                                (int)idx, KT_FIRST, NUM_KEY_TYPES-1);
                        break;
                    }

                    // Give one specific key.
                    P_GiveKey(plr, (keytype_t) idx);
                    break;
                }
            }

            // Give all keys.
            P_GiveKey(plr, NUM_KEY_TYPES /*all*/);
            break;

        case 'm':
            togglePower(plr, PT_ALLMAP);
            break;

        case 'p':
            P_GiveBackpack(plr);
            break;

        case 'r': {
            int armorType = 1;

            if(i < stuffLen)
            {
                char *end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < 0 || idx >= 4)
                    {
                        App_Log(DE2_LOG_SCR_ERROR, "Unknown armor type #%d (valid range %d-%d)\n",
                                (int)idx, 0, 4-1);
                        break;
                    }

                    armorType = idx;
                }
            }

            P_GiveArmor(plr, armorClass[armorType], armorPoints[armorType]);
            break; }

        case 's':
            togglePower(plr, PT_IRONFEET);
            break;

        case 'v':
            togglePower(plr, PT_INVISIBILITY);
            break;

        case 'w':
            if(i < stuffLen)
            {
                char *end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < WT_FIRST || idx >= NUM_WEAPON_TYPES)
                    {
                        App_Log(DE2_LOG_SCR_ERROR, "Unknown weapon #%d (valid range %d-%d)\n",
                                (int)idx, WT_FIRST, NUM_WEAPON_TYPES-1);
                        break;
                    }

                    // Give one specific weapon.
                    giveWeapon(plr, (weapontype_t)idx);
                    break;
                }
            }

            // Give all weapons.
            giveWeapon(plr, NUM_WEAPON_TYPES /*all types*/);
            break;

        default: // Unrecognized.
            App_Log(DE2_LOG_SCR_ERROR, "Cannot give '%c': unknown letter\n", buf[i]);
            break;
        }
    }

    // If the give expression matches that of a vanilla cheat code print the
    // associated confirmation message to the player's log.
    /// @todo fixme: Somewhat of kludge...
    if(!strcmp(buf, "war2"))
    {
        P_SetMessage(plr, LMF_NO_HIDE, STSTR_FAADDED);
    }
    else if(!strcmp(buf, "wakr3"))
    {
        P_SetMessage(plr, LMF_NO_HIDE, STSTR_KFAADDED);
    }

    return true;
}

D_CMD(CheatMassacre)
{
    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("kill");
        }
        else if((IS_NETGAME && !netSvAllowCheats) || gameSkill == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            App_Log(DE2_LOG_MAP, "%i monsters killed\n", P_Massacre());
        }
    }
    return true;
}

D_CMD(CheatWhere)
{
    player_t *plr = &players[CONSOLEPLAYER];
    char textBuffer[256];
    Sector *sector;
    AutoStr *path, *mapPath;
    Uri *uri, *mapUri;

    if(G_GameState() != GS_MAP || !plr->plr->mo)
        return true;

    mapUri = G_ComposeMapUri(gameEpisode, gameMap);
    mapPath = Uri_ToString(mapUri);
    sprintf(textBuffer, "MAP [%s]  X:%g  Y:%g  Z:%g",
            Str_Text(mapPath), plr->plr->mo->origin[VX], plr->plr->mo->origin[VY],
            plr->plr->mo->origin[VZ]);
    P_SetMessage(plr, LMF_NO_HIDE, textBuffer);
    Uri_Delete(mapUri);

    // Also print some information to the console.
    Con_Message("%s", textBuffer);

    sector = Mobj_Sector(plr->plr->mo);
    uri = Materials_ComposeUri(P_GetIntp(sector, DMU_FLOOR_MATERIAL));
    path = Uri_ToString(uri);
    Con_Message("  FloorZ:%g Material:%s", P_GetDoublep(sector, DMU_FLOOR_HEIGHT), Str_Text(path));
    Uri_Delete(uri);

    uri = Materials_ComposeUri(P_GetIntp(sector, DMU_CEILING_MATERIAL));
    path = Uri_ToString(uri);
    Con_Message("  CeilingZ:%g Material:%s", P_GetDoublep(sector, DMU_CEILING_HEIGHT), Str_Text(path));
    Uri_Delete(uri);

    Con_Message("Player height:%g Player radius:%g",
                plr->plr->mo->height, plr->plr->mo->radius);

    return true;
}

/**
 * Leave the current map and go to the intermission.
 */
D_CMD(CheatLeaveMap)
{
    // Only the server operator can end the map this way.
    if(IS_NETGAME && !IS_NETWORK_SERVER)
        return false;

    if(G_GameState() != GS_MAP)
    {
        S_LocalSound(SFX_OOF, NULL);
        App_Log(DE2_LOG_MAP | DE2_LOG_ERROR, "Can only exit a map when in a game!\n");
        return true;
    }

    G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
    return true;
}
