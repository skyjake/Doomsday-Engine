/** @file m_cheat.c Cheat code sequences
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <stdlib.h>
#include <errno.h>

#include "jhexen.h"

#include "d_net.h"
#include "g_common.h"
#include "p_player.h"
#include "am_map.h"
#include "hu_msg.h"
#include "dmu_lib.h"
#include "p_user.h"
#include "p_inventory.h"
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

CHEAT_FUNC(Class);
CHEAT_FUNC(IDKFA);
CHEAT_FUNC(Init);
CHEAT_FUNC(Quicken);
CHEAT_FUNC(Quicken2);
CHEAT_FUNC(Quicken3);
CHEAT_FUNC(Reveal);
CHEAT_FUNC(Script);
CHEAT_FUNC(Script2);
CHEAT_FUNC(Weapons);

void G_RegisterCheats(void)
{
    ADDCHEATCMD("butcher",          "kill");
    ADDCHEATCMD("casper",           "noclip %p");
    ADDCHEATCMD("clubmed",          "give h %p");
    ADDCHEAT("conan",               IDKFA);
    ADDCHEATCMD("deliverance",      "pig %p");
    ADDCHEATCMD("indiana",          "give i %p");
    ADDCHEAT("init",                Init);
    ADDCHEATCMD("locksmith",        "give k %p");
    ADDCHEAT("mapsco",              Reveal);
    ADDCHEAT("martekmartekmartek",  Quicken3);
    ADDCHEAT("martekmartek",        Quicken2);
    ADDCHEAT("martek",              Quicken);
    ADDCHEATCMD("mrjones",          "playsound PLATFORM_STOP;taskbar;version");
    ADDCHEATCMD("nra",              "give war %p");
    ADDCHEATCMD("noise",            "playsound PLATFORM_STOP"); // ignored, play sound
    ADDCHEATCMD("puke%1%2",         "runscript %1%2 %p");
    ADDCHEAT("puke%1",              Script2);
    ADDCHEAT("puke",                Script);
    ADDCHEATCMD("satan",            "god %p");
    ADDCHEATCMD("shadowcaster%1",   "class %1 %p");
    ADDCHEAT("shadowcaster",        Class);
    ADDCHEATCMD("sherlock",         "give p %p");
    ADDCHEATCMD("visit%1%2",        "warp %1%2");
    ADDCHEATCMD("where",            "where");
}

CHEAT_FUNC(Init)
{
    player_t *plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    G_SetGameAction(GA_RESTARTMAP);
    P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATWARP);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(IDKFA)
{
    player_t *plr = &players[player];
    int i;

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;
    if(plr->morphTics) return false;

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        plr->weapons[i].owned = false;
    }

    plr->pendingWeapon = WT_FIRST;
    P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATIDKFA);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Quicken)
{
    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    P_SetMessage(&players[player], LMF_NO_HIDE, "Trying to cheat? That's one...");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Quicken2)
{
    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    P_SetMessage(&players[player], LMF_NO_HIDE, "That's two...");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Quicken3)
{
    player_t *plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    P_DamageMobj(plr->plr->mo, NULL, plr->plr->mo, 10000, false);
    P_SetMessage(plr, LMF_NO_HIDE, "That's three! Time to die.");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Class)
{
    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    P_SetMessage(&players[player], LMF_NO_HIDE, "Enter new player class number");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Script)
{
    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    P_SetMessage(&players[player], LMF_NO_HIDE, "Run which script (01-99)?");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Script2)
{
    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    P_SetMessage(&players[player], LMF_NO_HIDE, "Run which script (01-99)?");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Reveal)
{
    player_t *plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME && deathmatch) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    if(ST_AutomapIsActive(player))
    {
        ST_CycleAutomapCheatLevel(player);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }

    return true;
}

/**
 * The multipurpose cheat ccmd.
 */
D_CMD(Cheat)
{
    // Give each of the characters in argument two to the SB event handler.
    int i, len = (int) strlen(argv[1]);
    for(i = 0; i < len; ++i)
    {
        event_t ev;
        ev.type  = EV_KEY;
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

            P_SetMessage(plr, LMF_NO_HIDE, ((P_GetPlayerCheats(plr) & CF_GODMODE) ? TXT_CHEATGODON : TXT_CHEATGODOFF));
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
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

            P_SetMessage(plr, LMF_NO_HIDE, ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? TXT_CHEATNOCLIPON : TXT_CHEATNOCLIPOFF));
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
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
        player_t *plr;

        if(IS_NETGAME && !netSvAllowCheats) return false;

        if(argc == 2)
        {
            int i = atoi(argv[1]);
            if(i < 0 || i >= MAXPLAYERS) return false;
            plr = &players[i];
        }
        else
        {
            plr = &players[CONSOLEPLAYER];
        }

        if(!plr->plr->inGame) return false;
        if(plr->playerState == PST_DEAD) return false;

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

static void giveAllWeaponsAndPieces(player_t *plr)
{
    P_GiveWeapon(plr, NUM_WEAPON_TYPES /*all types*/);
    P_GiveWeaponPiece(plr, WPIECE1);
    P_GiveWeaponPiece(plr, WPIECE2);
    P_GiveWeaponPiece(plr, WPIECE3);
}

D_CMD(CheatGive)
{
    char buf[100];
    int player = CONSOLEPLAYER;
    player_t *plr;
    size_t i, stuffLen;

    if(G_GameState() != GS_MAP)
    {
        App_Log(DE2_SCR_ERROR, "Can only \"give\" when in a game!\n");
        return true;
    }

    if(argc != 2 && argc != 3)
    {
        App_Log(DE2_SCR_NOTE, "Usage:\n  give (stuff)\n");
        App_Log(DE2_LOG_SCR, "  give (stuff) (plr)\n");
        App_Log(DE2_LOG_SCR, "Stuff consists of one or more of (type:id). "
                "If no id; give all of type:\n");
        App_Log(DE2_LOG_SCR, " a - ammo\n");
        App_Log(DE2_LOG_SCR, " h - health\n");
        App_Log(DE2_LOG_SCR, " i - items\n");
        App_Log(DE2_LOG_SCR, " k - keys\n");
        App_Log(DE2_LOG_SCR, " p - puzzle\n");
        App_Log(DE2_LOG_SCR, " r - armor\n");
        App_Log(DE2_LOG_SCR, " w - weapons\n");
        App_Log(DE2_LOG_SCR, "Example: 'give ikw' gives items, keys and weapons.\n");
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
                        App_Log(DE2_SCR_ERROR, "Unknown ammo #%d (valid range %d-%d)\n",
                                (int)idx, AT_FIRST, NUM_AMMO_TYPES-1);
                        break;
                    }

                    // Give one specific ammo type.
                    P_GiveAmmo(plr, (ammotype_t) idx, -1 /*fully replenish*/);
                    break;
                }
            }

            // Give all ammo.
            P_GiveAmmo(plr, NUM_AMMO_TYPES /*all types*/, -1 /*fully replenish*/);
            break;

        case 'h':
            P_GiveHealth(plr, -1 /*maximum amount*/);
            P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATHEALTH);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
            break;

        case 'i': {
            int k, m;

            for(k = IIT_NONE + 1; k < IIT_FIRSTPUZZITEM; ++k)
            for(m = 0; m < 25; ++m)
            {
                P_InventoryGive(player, k, false);
            }

            P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATINVITEMS3);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
            break; }

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
                        App_Log(DE2_SCR_ERROR, "Unknown key #%d (valid range %d-%d)\n",
                                (int)idx, KT_FIRST, NUM_KEY_TYPES-1);
                        break;
                    }

                    // Give one specific key.
                    P_GiveKey(plr, (keytype_t) idx);
                    break;
                }
            }

            // Give all keys.
            P_GiveKey(plr, NUM_KEY_TYPES /*all types*/);
            P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATKEYS);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
            break;

        case 'p': {
            int k;

            for(k = IIT_FIRSTPUZZITEM; k < NUM_INVENTORYITEM_TYPES; ++k)
            {
                P_InventoryGive(player, i, false);
            }

            P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATINVITEMS3);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
            break; }

        case 'r':
            if(i < stuffLen)
            {
                char *end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < ARMOR_FIRST || idx >= NUMARMOR)
                    {
                        App_Log(DE2_SCR_ERROR, "Unknown armor #%d (valid range %d-%d)\n",
                                (int)idx, ARMOR_FIRST, NUMARMOR-1);
                        break;
                    }

                    // Give one specific armor.
                    P_GiveArmor(plr, (armortype_t)idx);
                    break;
                }
            }

            // Give all armors.
            P_GiveArmor(plr, NUMARMOR /*all types*/);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
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
                        App_Log(DE2_SCR_ERROR, "Unknown weapon #%d (valid range %d-%d)\n",
                                (int)idx, WT_FIRST, NUM_WEAPON_TYPES-1);
                        break;
                    }

                    // Give one specific weapon.
                    P_GiveWeapon(plr, (weapontype_t)idx);
                    break;
                }
            }

            // Give all weapons.
            giveAllWeaponsAndPieces(plr);
            break;

        default: // Unrecognized.
            App_Log(DE2_SCR_ERROR, "Cannot give '%c': unknown letter\n", buf[i]);
            break;
        }
    }

    // If the give expression matches that of a vanilla cheat code print the
    // associated confirmation message to the player's log.
    /// @todo fixme: Somewhat of kludge...
    if(!strcmp(buf, "war"))
    {
        P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATWEAPONS);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
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
            int killCount = P_Massacre();
            AutoStr *msg = Str_Appendf(AutoStr_NewStd(), "%d monsters killed.", killCount);
            P_SetMessage(&players[CONSOLEPLAYER], LMF_NO_HIDE, Str_Text(msg));
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
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

    mapUri = G_CurrentMapUri();
    mapPath = Uri_ToString(mapUri);
    sprintf(textBuffer, "Map [%s]  x:%g  y:%g  z:%g",
            Str_Text(mapPath), plr->plr->mo->origin[VX], plr->plr->mo->origin[VY],
            plr->plr->mo->origin[VZ]);
    P_SetMessage(plr, LMF_NO_HIDE, textBuffer);
    Uri_Delete(mapUri);

    // Also print some information to the console.
    App_Log(DE2_MAP_NOTE, "%s", textBuffer);

    sector = Mobj_Sector(plr->plr->mo);
    uri = Materials_ComposeUri(P_GetIntp(sector, DMU_FLOOR_MATERIAL));
    path = Uri_ToString(uri);
    App_Log(DE2_MAP_MSG, "FloorZ:%g Material:%s", P_GetDoublep(sector, DMU_FLOOR_HEIGHT), Str_Text(path));
    Uri_Delete(uri);

    uri = Materials_ComposeUri(P_GetIntp(sector, DMU_CEILING_MATERIAL));
    path = Uri_ToString(uri);
    App_Log(DE2_MAP_MSG, "CeilingZ:%g Material:%s", P_GetDoublep(sector, DMU_CEILING_HEIGHT), Str_Text(path));
    Uri_Delete(uri);

    App_Log(DE2_MAP_MSG, "Player height:%g Player radius:%g",
            plr->plr->mo->height, plr->plr->mo->radius);

    return true;
}

D_CMD(CheatMorph)
{
    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("pig");
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

            if(plr->morphTics)
            {
                P_UndoPlayerMorph(plr);
            }
            else
            {
                P_MorphPlayer(plr);
            }

            P_SetMessage(plr, LMF_NO_HIDE, "Squeal!!");
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
        }
    }
    return true;
}

D_CMD(CheatShadowcaster)
{
    if(G_GameState() == GS_MAP)
    {
        playerclass_t newClass = (playerclass_t)atoi(argv[1]);

        if(IS_CLIENT)
        {
            AutoStr *cmd = Str_Appendf(AutoStr_NewStd(), "class %i", (int)newClass);
            NetCl_CheatRequest(Str_Text(cmd));
        }
        else if((IS_NETGAME && !netSvAllowCheats) || gameSkill == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            player_t *plr;

            if(argc == 3)
            {
                player = atoi(argv[2]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            plr = &players[player];
            if(!plr->plr->inGame) return false;

            // Dead players can't cheat.
            if(plr->health <= 0) return false;

            P_PlayerChangeClass(plr, newClass);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
        }
    }
    return true;
}

D_CMD(CheatRunScript)
{
    if(G_GameState() == GS_MAP)
    {
        int scriptNum = atoi(argv[1]);

        if(IS_CLIENT)
        {
            AutoStr *cmd = Str_Appendf(AutoStr_NewStd(), "runscript %i", scriptNum);
            NetCl_CheatRequest(Str_Text(cmd));
        }
        else if((IS_NETGAME && !netSvAllowCheats) || gameSkill == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            byte scriptArgs[3];
            player_t *plr;

            if(argc == 3)
            {
                player = atoi(argv[2]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            plr = &players[player];
            if(!plr->plr->inGame) return false;

            // Dead players can't cheat.
            if(plr->health <= 0) return false;

            /// @todo Don't do this here.
            if(scriptNum < 1 || scriptNum > 99) return false;

            scriptArgs[0] = scriptArgs[1] = scriptArgs[2] = 0;
            if(Game_ACScriptInterpreter_StartScript(scriptNum, 0/*current-map*/,
                                                    scriptArgs, plr->plr->mo, NULL, 0))
            {
                AutoStr *cmd = Str_Appendf(AutoStr_NewStd(), "Running script %i", scriptNum);
                P_SetMessage(plr, LMF_NO_HIDE, Str_Text(cmd));
            }

            S_LocalSound(SFX_PLATFORM_STOP, NULL);
        }
    }
    return true;
}
