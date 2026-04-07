/** @file m_cheat.cpp  Hexen cheat code sequences.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "jhexen.h"
#include "m_cheat.h"
#include "d_net.h"
#include "d_netcl.h"
#include "d_netsv.h"
#include "dmu_lib.h"
#include "g_common.h"
#include "g_eventsequence.h"
#include "gamesession.h"
#include "hu_msg.h"
#include "p_inventory.h"
#include "p_user.h"
#include "player.h"
#include "acs/system.h"

#include <cerrno>
#include <cstdlib>

typedef eventsequencehandler_t cheatfunc_t;

/// Helper macro for forming cheat callback function names.
#define CHEAT(x) G_Cheat##x

/// Helper macro for declaring cheat callback functions.
#define CHEAT_FUNC(x) int G_Cheat##x(int player, const EventSequenceArg *args, int numArgs)

/// Helper macro for registering new cheat event sequence handlers.
#define ADDCHEAT(name, callback) G_AddEventSequence((name), CHEAT(callback))

/// Helper macro for registering new cheat event sequence command handlers.
#define ADDCHEATCMD(name, cmdTemplate) G_AddEventSequenceCommand((name), cmdTemplate)

static inline acs::System &acscriptSys()
{
    return gfw_Session()->acsSystem();
}

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

void G_RegisterCheats()
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
    DE_UNUSED(args, numArgs);

    player_t *plr = &players[player];

    if(IS_NETGAME) return false;
    if(gfw_Rule(skill) == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    G_SetGameAction(GA_RESTARTMAP);
    P_SetMessageWithFlags(plr, TXT_CHEATWARP, LMF_NO_HIDE);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(IDKFA)
{
    DE_UNUSED(args, numArgs);

    player_t *plr = &players[player];

    if(gfw_Rule(skill) == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;
    if(plr->morphTics) return false;

    for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        plr->weapons[i].owned = false;
    }

    plr->pendingWeapon = WT_FIRST;
    P_SetMessageWithFlags(plr, TXT_CHEATIDKFA, LMF_NO_HIDE);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Quicken)
{
    DE_UNUSED(args, numArgs);

    P_SetMessageWithFlags(&players[player], "Trying to cheat? That's one...", LMF_NO_HIDE);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Quicken2)
{
    DE_UNUSED(args, numArgs);

    P_SetMessageWithFlags(&players[player], "That's two...", LMF_NO_HIDE);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Quicken3)
{
    DE_UNUSED(args, numArgs);

    player_t *plr = &players[player];

    if(gfw_Rule(skill) == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    P_DamageMobj(plr->plr->mo, NULL, plr->plr->mo, 10000, false);
    P_SetMessageWithFlags(plr, "That's three! Time to die.", LMF_NO_HIDE);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Class)
{
    DE_UNUSED(args, numArgs);

    P_SetMessageWithFlags(&players[player], "Enter new player class number", LMF_NO_HIDE);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Script)
{
    DE_UNUSED(args, numArgs);

    P_SetMessageWithFlags(&players[player], "Run which script (01-99)?", LMF_NO_HIDE);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Script2)
{
    DE_UNUSED(args, numArgs);

    P_SetMessageWithFlags(&players[player], "Run which script (01-99)?", LMF_NO_HIDE);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);

    return true;
}

CHEAT_FUNC(Reveal)
{
    DE_UNUSED(args, numArgs);

    player_t *plr = &players[player];

    if(IS_NETGAME && gfw_Rule(deathmatch)) return false;
    if(gfw_Rule(skill) == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    if(ST_AutomapIsOpen(player))
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
    DE_UNUSED(src, argc);

    // Give each of the characters in argument two to the SB event handler.
    const int len = (int) strlen(argv[1]);
    for(int i = 0; i < len; ++i)
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
    DE_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("god");
        }
        else if((IS_NETGAME && !netSvAllowCheats) || gfw_Rule(skill) == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            player_t *plr = &players[player];
            if(!plr->plr->inGame) return false;

            // Dead players can't cheat.
            if(plr->health <= 0) return false;

            plr->cheats ^= CF_GODMODE;
            plr->update |= PSF_STATE;

            P_SetMessageWithFlags(plr, ((P_GetPlayerCheats(plr) & CF_GODMODE) ? TXT_CHEATGODON : TXT_CHEATGODOFF), LMF_NO_HIDE);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
        }
    }
    return true;
}

D_CMD(CheatNoClip)
{
    DE_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("noclip");
        }
        else if((IS_NETGAME && !netSvAllowCheats) || gfw_Rule(skill) == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            player_t *plr = &players[player];
            if(!plr->plr->inGame) return false;

            // Dead players can't cheat.
            if(plr->health <= 0) return false;

            plr->cheats ^= CF_NOCLIP;
            plr->update |= PSF_STATE;

            P_SetMessageWithFlags(plr, ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? TXT_CHEATNOCLIPON : TXT_CHEATNOCLIPOFF), LMF_NO_HIDE);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
        }
    }
    return true;
}

static int suicideResponse(msgresponse_t response, int /*userValue*/, void * /*userPointer*/)
{
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
    DE_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        if(IS_NETGAME && !netSvAllowCheats) return false;

        player_t *plr;
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
    DE_UNUSED(src, argc);

    // Server operator can always reveal.
    if(IS_NETGAME && !IS_NETWORK_SERVER)
        return false;

    int option = atoi(argv[1]);
    if(option < 0 || option > 3) return false;

    for(int i = 0; i < MAXPLAYERS; ++i)
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
    P_GiveWeaponPiece(plr, WEAPON_FOURTH_PIECE_COUNT /*all pieces*/);
}

D_CMD(CheatGive)
{
    DE_UNUSED(src);

    if(G_GameState() != GS_MAP)
    {
        App_Log(DE2_SCR_ERROR, "Can only \"give\" when in a game!");
        return true;
    }

    if(argc != 2 && argc != 3)
    {
        App_Log(DE2_SCR_NOTE, "Usage:\n  give (stuff)\n  give (stuff) (plr)");
        App_Log(DE2_LOG_SCR, "Stuff consists of one or more of (type:id). "
                             "If no id; give all of type:");
        App_Log(DE2_LOG_SCR, " a - ammo");
        App_Log(DE2_LOG_SCR, " h - health");
        App_Log(DE2_LOG_SCR, " i - items");
        App_Log(DE2_LOG_SCR, " k - keys");
        App_Log(DE2_LOG_SCR, " p - puzzle");
        App_Log(DE2_LOG_SCR, " r - armor");
        App_Log(DE2_LOG_SCR, " w - weapons");
        App_Log(DE2_LOG_SCR, "Example: 'give ikw' gives items, keys and weapons.");
        App_Log(DE2_LOG_SCR, "Example: 'give w2k1' gives weapon two and key one.");
        return true;
    }

    int player = CONSOLEPLAYER;
    if(argc == 3)
    {
        player = atoi(argv[2]);
        if(player < 0 || player >= MAXPLAYERS) return false;
    }

    if(IS_CLIENT)
    {
        if(argc < 2) return false;

        char buf[100]; sprintf(buf, "give %s", argv[1]);
        NetCl_CheatRequest(buf);
        return true;
    }

    if((IS_NETGAME && !netSvAllowCheats) || gfw_Rule(skill) == SM_NIGHTMARE)
        return false;

    player_t *plr = &players[player];

    // Can't give to a player who's not in the game.
    if(!plr->plr->inGame) return false;

    // Can't give to a dead player.
    if(plr->health <= 0) return false;

    char buf[100]; strcpy(buf, argv[1]); // Stuff is the 2nd arg.
    strlwr(buf);
    const size_t stuffLen = strlen(buf);
    for(size_t i = 0; buf[i]; ++i)
    {
        switch(buf[i])
        {
        case 'a':
            if(i < stuffLen)
            {
                char *end;
                errno = 0;
                long idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < AT_FIRST || idx >= NUM_AMMO_TYPES)
                    {
                        App_Log(DE2_SCR_ERROR, "Unknown ammo #%d (valid range %d-%d)",
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
            P_SetMessageWithFlags(plr, TXT_CHEATHEALTH, LMF_NO_HIDE);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
            break;

        case 'i':
            for(int k = IIT_NONE + 1; k < IIT_FIRSTPUZZITEM; ++k)
            for(int m = 0; m < 25; ++m)
            {
                P_InventoryGive(player, inventoryitemtype_t(k), false);
            }

            P_SetMessageWithFlags(plr, TXT_CHEATINVITEMS3, LMF_NO_HIDE);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
            break;

        case 'k':
            if(i < stuffLen)
            {
                char *end;
                errno = 0;
                long idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < KT_FIRST || idx >= NUM_KEY_TYPES)
                    {
                        App_Log(DE2_SCR_ERROR, "Unknown key #%d (valid range %d-%d)",
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
            P_SetMessageWithFlags(plr, TXT_CHEATKEYS, LMF_NO_HIDE);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
            break;

        case 'p':
            for(int k = IIT_FIRSTPUZZITEM; k < NUM_INVENTORYITEM_TYPES; ++k)
            {
                P_InventoryGive(player, inventoryitemtype_t(k), false);
            }

            P_SetMessageWithFlags(plr, TXT_CHEATINVITEMS3, LMF_NO_HIDE);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
            break;

        case 'r':
            if(i < stuffLen)
            {
                char *end;
                errno = 0;
                long idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < ARMOR_FIRST || idx >= NUMARMOR)
                    {
                        App_Log(DE2_SCR_ERROR, "Unknown armor #%d (valid range %d-%d)",
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
                errno = 0;
                long idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < WT_FIRST || idx >= NUM_WEAPON_TYPES)
                    {
                        App_Log(DE2_SCR_ERROR, "Unknown weapon #%d (valid range %d-%d)",
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
            App_Log(DE2_SCR_ERROR, "Cannot give '%c': unknown letter", buf[i]);
            break;
        }
    }

    // If the give expression matches that of a vanilla cheat code print the
    // associated confirmation message to the player's log.
    /// @todo fixme: Somewhat of kludge...
    if(!strcmp(buf, "war"))
    {
        P_SetMessageWithFlags(plr, TXT_CHEATWEAPONS, LMF_NO_HIDE);
        S_LocalSound(SFX_PLATFORM_STOP, NULL);
    }

    return true;
}

D_CMD(CheatMassacre)
{
    DE_UNUSED(src, argc, argv);

    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("kill");
        }
        else if((IS_NETGAME && !netSvAllowCheats) || gfw_Rule(skill) == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int killCount = P_Massacre();
            AutoStr *msg  = Str_Appendf(AutoStr_NewStd(), "%d monsters killed.", killCount);
            P_SetMessageWithFlags(&players[CONSOLEPLAYER], Str_Text(msg), LMF_NO_HIDE);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
        }
    }
    return true;
}

D_CMD(CheatWhere)
{
    DE_UNUSED(src, argc, argv);

    if(G_GameState() != GS_MAP)
        return true;

    player_t *plr = &players[CONSOLEPLAYER];
    mobj_t *plrMo = plr->plr->mo;
    if(!plrMo) return true;

    char textBuffer[256];
    sprintf(textBuffer, "MAP [%s]  X:%g  Y:%g  Z:%g",
                        gfw_Session()->mapUri().path().c_str(),
                        plrMo->origin[VX], plrMo->origin[VY], plrMo->origin[VZ]);
    P_SetMessageWithFlags(plr, textBuffer, LMF_NO_HIDE);

    // Also print some information to the console.
    App_Log(DE2_MAP_NOTE, "%s", textBuffer);

    Sector *sector = Mobj_Sector(plrMo);

    Uri *matUri = Materials_ComposeUri(P_GetIntp(sector, DMU_FLOOR_MATERIAL));
    App_Log(DE2_MAP_MSG, "FloorZ:%g Material:%s",
                         P_GetDoublep(sector, DMU_FLOOR_HEIGHT), Str_Text(Uri_ToString(matUri)));
    Uri_Delete(matUri);

    matUri = Materials_ComposeUri(P_GetIntp(sector, DMU_CEILING_MATERIAL));
    App_Log(DE2_MAP_MSG, "CeilingZ:%g Material:%s",
                          P_GetDoublep(sector, DMU_CEILING_HEIGHT), Str_Text(Uri_ToString(matUri)));
    Uri_Delete(matUri);

    App_Log(DE2_MAP_MSG, "Player height:%g Player radius:%g",
                          plrMo->height, plrMo->radius);

    return true;
}

D_CMD(CheatMorph)
{
    DE_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("pig");
        }
        else if((IS_NETGAME && !netSvAllowCheats) || gfw_Rule(skill) == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            player_t *plr = &players[player];
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

            P_SetMessageWithFlags(plr, "Squeal!!", LMF_NO_HIDE);
            S_LocalSound(SFX_PLATFORM_STOP, NULL);
        }
    }
    return true;
}

D_CMD(CheatShadowcaster)
{
    DE_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        playerclass_t newClass = (playerclass_t)atoi(argv[1]);

        if(IS_CLIENT)
        {
            AutoStr *cmd = Str_Appendf(AutoStr_NewStd(), "class %i", (int)newClass);
            NetCl_CheatRequest(Str_Text(cmd));
        }
        else if((IS_NETGAME && !netSvAllowCheats) || gfw_Rule(skill) == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            if(argc == 3)
            {
                player = atoi(argv[2]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            player_t *plr = &players[player];
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
    DE_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        int scriptNum = atoi(argv[1]);

        if(IS_CLIENT)
        {
            AutoStr *cmd = Str_Appendf(AutoStr_NewStd(), "runscript %i", scriptNum);
            NetCl_CheatRequest(Str_Text(cmd));
        }
        else if((IS_NETGAME && !netSvAllowCheats) || gfw_Rule(skill) == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            if(argc == 3)
            {
                player = atoi(argv[2]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            player_t *plr = &players[player];
            if(!plr->plr->inGame) return false;

            // Dead players can't cheat.
            if(plr->health <= 0) return false;

            /// @todo Don't do this here.
            if(scriptNum < 1 || scriptNum > 99) return false;

            if(acscriptSys().hasScript(scriptNum))
            {
                if(acscriptSys().script(scriptNum).start(acs::Script::Args()/*default args*/,
                                                         plr->plr->mo, nullptr, 0))
                {
                    de::String msg = de::Stringf("Running script %i", scriptNum);
                    P_SetMessageWithFlags(plr, msg, LMF_NO_HIDE);
                }
            }

            S_LocalSound(SFX_PLATFORM_STOP, NULL);
        }
    }
    return true;
}
