/** @file m_cheat.cpp  Cheat code sequences
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "jheretic.h"
#include "m_cheat.h"

#include <cerrno>
#include <cstdlib>
#include "d_net.h"
#include "d_netcl.h"
#include "d_netsv.h"
#include "dmu_lib.h"
#include "hu_msg.h"
#include "g_eventsequence.h"
#include "gamesession.h"
#include "p_inventory.h"
#include "p_user.h"
#include "player.h"
#include "p_inter.h"

typedef eventsequencehandler_t cheatfunc_t;

/// Helper macro for forming cheat callback function names.
#define CHEAT(x) G_Cheat##x

/// Helper macro for declaring cheat callback functions.
#define CHEAT_FUNC(x) int G_Cheat##x(int player, const EventSequenceArg* args, int numArgs)

/// Helper macro for registering new cheat event sequence handlers.
#define ADDCHEAT(name, callback) G_AddEventSequence((name), CHEAT(callback))

/// Helper macro for registering new cheat event sequence command handlers.
#define ADDCHEATCMD(name, cmdTemplate) G_AddEventSequenceCommand((name), cmdTemplate)

CHEAT_FUNC(InvItem);
CHEAT_FUNC(InvItem2);
CHEAT_FUNC(InvItem3);
CHEAT_FUNC(IDKFA);
CHEAT_FUNC(IDDQD);
CHEAT_FUNC(Reveal);

void G_RegisterCheats(void)
{
    ADDCHEATCMD("cockadoodledoo",   "chicken %p");
    ADDCHEATCMD("engage%1%2",       "warp %1 %2");
    ADDCHEAT("gimme%1%2",           InvItem3); // Final stage.
    ADDCHEAT("gimme%1",             InvItem2); // 2nd stage (ask for count).
    ADDCHEAT("gimme",               InvItem);  // 1st stage (ask for type).
    ADDCHEAT("iddqd",               IDDQD);
    ADDCHEAT("idkfa",               IDKFA);
    ADDCHEATCMD("kitty",            "noclip %p");
    ADDCHEATCMD("massacre",         "kill");
    ADDCHEATCMD("noise",            "playsound dorcls"); // ignored, play sound
    ADDCHEATCMD("ponce",            "give h %p");
    ADDCHEATCMD("quicken",          "god %p");
    ADDCHEATCMD("rambo",            "give wpar2 %p");
    ADDCHEAT("ravmap",              Reveal);
    ADDCHEATCMD("shazam",           "give t %p");
    ADDCHEATCMD("skel",             "give k %p");
    ADDCHEATCMD("ticker",           "playsound dorcls"); // ignored, play sound
}

CHEAT_FUNC(InvItem)
{
    DE_UNUSED(args, numArgs);
    DE_ASSERT(player >= 0 && player < MAXPLAYERS);

    P_SetMessageWithFlags(&players[player], TXT_CHEATINVITEMS1, LMF_NO_HIDE);
    S_LocalSound(SFX_DORCLS, NULL);

    return true;
}

CHEAT_FUNC(InvItem2)
{
    DE_UNUSED(args, numArgs);
    DE_ASSERT(player >= 0 && player < MAXPLAYERS);

    P_SetMessageWithFlags(&players[player], TXT_CHEATINVITEMS2, LMF_NO_HIDE);
    S_LocalSound(SFX_DORCLS, NULL);

    return true;
}

CHEAT_FUNC(InvItem3)
{
    DE_UNUSED(numArgs);
    DE_ASSERT(player >= 0 && player < MAXPLAYERS);

    player_t *plr = &players[player];

    if(gfw_Rule(skill) == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    bool didGive = false;
    inventoryitemtype_t type  = inventoryitemtype_t(args[0] - 'a' + 1);
    int count                 = args[1] - '0';
    if(type > IIT_NONE && type < NUM_INVENTORYITEM_TYPES && count > 0 && count < 10)
    {
        for(int i = 0; i < count; ++i)
        {
            if(P_InventoryGive(player, type, false))
                didGive = true;
        }
    }

    if(!didGive)
    {
        P_SetMessageWithFlags(plr, TXT_CHEATITEMSFAIL, LMF_NO_HIDE);
        return false;
    }

    P_SetMessageWithFlags(plr, TXT_CHEATINVITEMS3, LMF_NO_HIDE);
    S_LocalSound(SFX_DORCLS, NULL);
    return true;
}

CHEAT_FUNC(IDKFA)
{
    player_t *plr = &players[player];

    DE_UNUSED(args, numArgs);
    DE_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(gfw_Rule(skill) == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;
    if(plr->morphTics) return false;

    plr->update |= PSF_OWNED_WEAPONS;
    for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        plr->weapons[i].owned = false;
    }

    //plr->pendingWeapon = WT_FIRST;
    P_MaybeChangeWeapon(plr, WT_FIRST, AT_NOAMMO, true /*force*/);

    P_SetMessageWithFlags(plr, TXT_CHEATIDKFA, LMF_NO_HIDE);
    S_LocalSound(SFX_DORCLS, NULL);

    return true;
}

CHEAT_FUNC(IDDQD)
{
    player_t *plr = &players[player];

    DE_UNUSED(args, numArgs);
    DE_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(gfw_Rule(skill) == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    P_DamageMobj(plr->plr->mo, NULL, plr->plr->mo, 10000, false);

    P_SetMessageWithFlags(plr, TXT_CHEATIDDQD, LMF_NO_HIDE);
    S_LocalSound(SFX_DORCLS, NULL);

    return true;
}

CHEAT_FUNC(Reveal)
{
    player_t *plr = &players[player];

    DE_UNUSED(args, numArgs);
    DE_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME && gfw_Rule(deathmatch)) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    if(ST_AutomapIsOpen(player))
    {
        ST_CycleAutomapCheatLevel(player);
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

            P_SetMessageWithFlags(plr, ((P_GetPlayerCheats(plr) & CF_GODMODE) ? TXT_CHEATGODON : TXT_CHEATGODOFF), LMF_NO_HIDE);
            S_LocalSound(SFX_DORCLS, NULL);
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

            P_SetMessageWithFlags(plr, ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? TXT_CHEATNOCLIPON : TXT_CHEATNOCLIPOFF), LMF_NO_HIDE);
            S_LocalSound(SFX_DORCLS, NULL);
        }
    }
    return true;
}

static int suicideResponse(msgresponse_t response, int /*userValue*/, void * /*context*/)
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
    DE_UNUSED(src, argc);

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

D_CMD(CheatTake)
{
    if (!common::GameSession::gameSession().hasBegun())
    {
        App_Log(DE2_SCR_ERROR, "Can only \"take\" when in a game!");
        return false;
    }

    if (IS_CLIENT)
    {
        App_Log(DE2_SCR_ERROR, "\"take\" not supported on multiplayer client");
        return false;
    }

    if (argc != 2 && argc != 3)
    {
        App_Log(DE2_SCR_NOTE, "Usage:\n  take (stuff)\n  take (stuff) (plr)");
        App_Log(DE2_LOG_SCR, "Stuff consists of one or more of (type:id). "
                             "If no id; take all of type:");
        App_Log(DE2_LOG_SCR, " a - ammo");
        App_Log(DE2_LOG_SCR, " w - weapons");
        App_Log(DE2_LOG_SCR, "Example: 'take a' takes away all ammo.");
        App_Log(DE2_LOG_SCR, "Example: 'take w2' takes weapon two.");
        return true;
    }

    int player = CONSOLEPLAYER;
    player_t *plr;

    if (argc == 3)
    {
        player = atoi(argv[2]);
        if (player < 0 || player >= MAXPLAYERS) return false;
    }

    if (src != CMDS_SCRIPT)
    {
        if ((IS_NETGAME && !netSvAllowCheats) || gfw_Rule(skill) == SM_NIGHTMARE)
            return false;
    }

    plr = &players[player];

    // Can't take frmo a player who's not in the game.
    if(!plr->plr->inGame) return false;

    // Can't take from a dead player.
    if(plr->health <= 0) return false;

    char buf[100];
    strcpy(buf, argv[1]); // Stuff is the 2nd arg.
    strlwr(buf);
    size_t stuffLen = strlen(buf);
    for (int i = 0; buf[i]; ++i)
    {
        const char code = buf[i];
        int parm = -1;
        {
            const char parmChar = buf[i + 1];
            if (parmChar >= '1' && parmChar <= '9')
            {
                parm = parmChar - '0';
                i++;
            }
        }
        switch (code)
        {
            case 'a': P_TakeAmmo(plr, parm < 0 ? NUM_AMMO_TYPES : ammotype_t(parm)); break;
            case 'w': P_TakeWeapon(plr, parm < 0 ? NUM_WEAPON_TYPES : weapontype_t(parm)); break;
            default: break;
        }
    }

    return true;
}

D_CMD(CheatGive)
{
    char buf[100];
    int player = CONSOLEPLAYER;
    player_t *plr;
    size_t i, stuffLen;

    if (!common::GameSession::gameSession().hasBegun())
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
        App_Log(DE2_LOG_SCR, " i - items");
        App_Log(DE2_LOG_SCR, " h - health");
        App_Log(DE2_LOG_SCR, " k - keys");
        App_Log(DE2_LOG_SCR, " p - backpack full of ammo");
        App_Log(DE2_LOG_SCR, " r - armor");
        App_Log(DE2_LOG_SCR, " t - tome of power");
        App_Log(DE2_LOG_SCR, " w - weapons");
        App_Log(DE2_LOG_SCR, "Example: 'give ikw' gives items, keys and weapons.");
        App_Log(DE2_LOG_SCR, "Example: 'give w2k1' gives weapon two and key one.");
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

    if (src != CMDS_SCRIPT)
    {
        if((IS_NETGAME && !netSvAllowCheats) || gfw_Rule(skill) == SM_NIGHTMARE)
            return false;
    }

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
            P_GiveAmmo(plr, NUM_AMMO_TYPES, -1 /*fully replenish*/);
            break;

        case 'i': // Inventory items.
            if(i < stuffLen)
            {
                char *end;
                errno = 0;
                long idx = strtol(&buf[i+1], &end, 0);

                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < IIT_FIRST || idx >= NUM_INVENTORYITEM_TYPES)
                    {
                        App_Log(DE2_SCR_ERROR, "Unknown item #%d (valid range %d-%d)",
                                   (int)idx, IIT_FIRST, NUM_INVENTORYITEM_TYPES-1);
                        break;
                    }

                    // Give one specific item type.
                    for(int j = 0; j < MAXINVITEMCOUNT; ++j)
                    {
                        P_InventoryGive(player, inventoryitemtype_t(idx), false);
                    }
                    break;
                }
            }

            // Give all inventory items.
            for(int type = IIT_FIRST; type < NUM_INVENTORYITEM_TYPES; ++type)
            {
                for(int i = 0; i < MAXINVITEMCOUNT; ++i)
                {
                    P_InventoryGive(player, inventoryitemtype_t(type), false);
                }
            }
            break;

        case 'h':
            P_GiveHealth(plr, -1 /*maximum amount*/);
            P_SetMessageWithFlags(plr, TXT_CHEATHEALTH, LMF_NO_HIDE);
            S_LocalSound(SFX_DORCLS, NULL);
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
                        App_Log(DE2_SCR_ERROR, "Unknown key #%d (valid range %d-%d)",
                                   (int)idx, KT_FIRST, NUM_KEY_TYPES-1);
                        break;
                    }

                    // Give one specific key.
                    P_GiveKey(plr, (keytype_t)idx);
                    break;
                }
            }

            // Give all keys.
            P_GiveKey(plr, NUM_KEY_TYPES /*all types*/);
            P_SetMessageWithFlags(plr, TXT_CHEATKEYS, LMF_NO_HIDE);
            S_LocalSound(SFX_DORCLS, NULL);
            break;

        case 'p':
            P_GiveBackpack(plr);
            break;

        case 'r': {
            int armorType = 2;

            if(i < stuffLen)
            {
                char *end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < 0 || idx >= 3)
                    {
                        App_Log(DE2_SCR_ERROR, "Unknown armor type #%d (valid range %d-%d)",
                                   (int)idx, 0, 3-1);
                        break;
                    }

                    armorType = idx;
                }
            }

            P_GiveArmor(plr, armorType, armorType * 100);
            break; }

        case 't':
            if(plr->powers[PT_WEAPONLEVEL2])
            {
                P_TakePower(plr, PT_WEAPONLEVEL2);
                P_SetMessageWithFlags(plr, TXT_CHEATPOWEROFF, LMF_NO_HIDE);
            }
            else
            {
                P_InventoryGive(player, IIT_TOMBOFPOWER, true /*silent*/);
                P_InventoryUse(player, IIT_TOMBOFPOWER, true /*silent*/);
                P_SetMessageWithFlags(plr, TXT_CHEATPOWERON, LMF_NO_HIDE);
            }
            S_LocalSound(SFX_DORCLS, NULL);
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
                        App_Log(DE2_SCR_ERROR, "Unknown weapon #%d (valid range %d-%d)",
                                   (int)idx, WT_FIRST, NUM_WEAPON_TYPES-1);
                        break;
                    }

                    // Give one specific weapon.
                    P_GiveWeapon(plr, (weapontype_t) idx);
                    break;
                }
            }

            // Give all weapons.
            P_GiveWeapon(plr, NUM_WEAPON_TYPES /*all types*/);
            break;

        default: // Unrecognized.
            App_Log(DE2_SCR_ERROR, "Cannot give '%c': unknown letter", buf[i]);
            break;
        }
    }

    // If the give expression matches that of a vanilla cheat code print the
    // associated confirmation message to the player's log.
    /// @todo fixme: Somewhat of kludge...
    if(!strcmp(buf, "wpar2"))
    {
        P_SetMessageWithFlags(plr, TXT_CHEATWEAPONS, LMF_NO_HIDE);
        S_LocalSound(SFX_DORCLS, NULL);
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
            P_Massacre();
            P_SetMessageWithFlags(&players[CONSOLEPLAYER], TXT_CHEATMASSACRE, LMF_NO_HIDE);
            S_LocalSound(SFX_DORCLS, NULL);
        }
    }
    return true;
}

D_CMD(CheatWhere)
{
    DE_UNUSED(src, argc, argv);

    player_t *plr = &players[CONSOLEPLAYER];
    char textBuffer[256];
    Sector *sector;
    mobj_t *plrMo;
    Uri *matUri;

    if(G_GameState() != GS_MAP)
        return true;

    plrMo = plr->plr->mo;
    if(!plrMo) return true;

    sprintf(textBuffer,
            "MAP [%s]  X:%g  Y:%g  Z:%g",
            gfw_Session()->mapUri().path().c_str(),
            plrMo->origin[VX],
            plrMo->origin[VY],
            plrMo->origin[VZ]);
    P_SetMessageWithFlags(plr, textBuffer, LMF_NO_HIDE);

    // Also print some information to the console.
    App_Log(DE2_MAP_NOTE, "%s", textBuffer);

    sector = Mobj_Sector(plrMo);

    matUri = Materials_ComposeUri(P_GetIntp(sector, DMU_FLOOR_MATERIAL));
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
            NetCl_CheatRequest("chicken");
        }
        else if((IS_NETGAME && !netSvAllowCheats) || gfw_Rule(skill) == SM_NIGHTMARE)
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
                if(P_UndoPlayerMorph(plr))
                {
                    P_SetMessageWithFlags(plr, TXT_CHEATCHICKENOFF, LMF_NO_HIDE);
                }
            }
            else if(P_MorphPlayer(plr))
            {
                P_SetMessageWithFlags(plr, TXT_CHEATCHICKENON, LMF_NO_HIDE);
            }
            S_LocalSound(SFX_DORCLS, NULL);
        }
    }
    return true;
}
