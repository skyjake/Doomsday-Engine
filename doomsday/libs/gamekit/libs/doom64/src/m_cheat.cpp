/** @file m_cheat.cpp  Doom64 cheat code sequences
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

// TODO : Header cleanup. After the makeover, a lot of these are probably unused

#include "jdoom64.h"
#include "m_cheat.h"

#include <de/log.h>
#include <de/range.h>
#include <de/string.h>
#include <de/vector.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#ifdef UNIX
# include <cerrno>
#endif
#include "d_net.h"
#include "d_netcl.h"
#include "d_netsv.h"
#include "dmu_lib.h"
#include "g_common.h"
#include "gamesession.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "player.h"
#include "p_inventory.h"
#include "p_start.h"
#include "p_user.h"

/*
 * Doom64 Cheats.
 * Unlike the other doom games, Doom64 does not have `cheat xxx` style cheats, as it was released for
 * the N64, which did not have a keyboard.
 *
 * It did, however, have konami-style codes, though I doubt that it is within the realm of possibility
 * to implement them (in truest form) using a keyboard.
 */

using namespace de;

// God
// ===============================================================================================================

D_CMD(CheatGod)
{
    DE_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("god");
        }
        else if((IS_NETGAME && !netSvAllowCheats)
                || gfw_Rule(skill) == SM_HARD)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            if(argc == 2)
            {
                player = String(argv[1]).toInt();
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            player_t *plr = &players[player];
            if(!plr->plr->inGame) return false;

            // Prevent dead players from cheating
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

            P_SetMessageWithFlags(plr, ((P_GetPlayerCheats(plr) & CF_GODMODE) ? STSTR_DQDON : STSTR_DQDOFF), LMF_NO_HIDE);
        }
    }
    return true;
}

// NoClip
// ===============================================================================================================

D_CMD(CheatNoClip)
{
    DE_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("noclip");
        }
        else if((IS_NETGAME && !netSvAllowCheats)
                || gfw_Rule(skill) == SM_HARD)
        {
            return false;
        }
        else
        {
            int player = CONSOLEPLAYER;
            if(argc == 2)
            {
                player = String(argv[1]).toInt();
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            player_t *plr = &players[CONSOLEPLAYER];
            if(!plr->plr->inGame) return false;

            // Prevent dead from cheating
            if(plr->health <= 0) return false;

            plr->cheats ^= CF_NOCLIP;
            plr->update |= PSF_STATE;
            P_SetMessageWithFlags(plr, ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? STSTR_NCON : STSTR_NCOFF), LMF_NO_HIDE);
        }
    }
    return true;
}

// Suicide
// ===============================================================================================================

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
            P_DamageMobj(players[CONSOLEPLAYER].plr->mo, nullptr, nullptr, 10000, false);
        }
    }
    return true;
}

D_CMD(CheatSuicide)
{
    DE_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        int player = CONSOLEPLAYER;
        if(!IS_CLIENT || argc == 2)
        {
            player = String(argv[1]).toInt();
            if(player < 0 || player >= MAXPLAYERS) return false;
        }

        player_t *plr = &players[player];
        if(!plr->plr->inGame) return false;
        if(plr->playerState == PST_DEAD) return false;

        if(!IS_NETGAME || IS_CLIENT)
        {
            Hu_MsgStart(MSG_YESNO, SUICIDEASK, suicideResponse, 0, nullptr);
        }
        else
        {
            P_DamageMobj(plr->plr->mo, nullptr, nullptr, 10000, false);
        }
        return true;
    }
    else
    {
        Hu_MsgStart(MSG_ANYKEY, SUICIDEOUTMAP, nullptr, 0, nullptr);
        return true;
    }
}

// Reveal
// ===============================================================================================================

D_CMD(CheatReveal)
{
    DE_UNUSED(src, argc);

    if(IS_NETGAME && !IS_NETWORK_SERVER)
        return false;

    int option = String(argv[1]).toInt();
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

// Give
// ===============================================================================================================

static void giveWeapon(player_t *plr, weapontype_t weaponType)
{
    P_GiveWeapon(plr, weaponType, false /* not collecting a drop */);
    if(weaponType == WT_EIGHTH)
    {
        P_SetMessageWithFlags(plr, STSTR_CHOPPERS, LMF_NO_HIDE);
    }
}

static void giveLaserUpgrade(player_t *plr, inventoryitemtype_t upgrade)
{
    if(P_InventoryGive(plr - players, upgrade, true /* silent */))
    {
        P_SetMessageWithFlags(plr, STSTR_BEHOLDX, LMF_NO_HIDE);
    }
}

static void togglePower(player_t* player, powertype_t powerType)
{
    P_TogglePower(player, powerType);
    P_SetMessageWithFlags(player, STSTR_BEHOLDX, LMF_NO_HIDE);
}

D_CMD(CheatGive)
{
    DE_UNUSED(src);

    if(G_GameState() != GS_MAP)
    {
        LOG_SCR_ERROR("Can only \"give\" when in a game!");
        return true;
    }
    else if(argc != 2 && argc != 3)
    {
        LOG_SCR_NOTE("Usage:\n give (stuff) give (stuff) (player number)");

#define TABBED(A, B) "\n" _E(Ta) _E(b) "  " << A << " " _E(.) _E(Tb) << B
        LOG_SCR_MSG("Where (stuff) is one or more type:id codes"
                    "(id no id, give all of that type):")
                << TABBED("a", "Ammo")
                << TABBED("b", "Berserk")
                << TABBED("f", "Flight ability")
                << TABBED("g", "Light amplification visor")
                << TABBED("h", "Health")
                << TABBED("k", "Keys")
                << TABBED("l", "Laser Upgrades (1, 2, 3)")
                << TABBED("m", "Computer area map")
                << TABBED("p", "Backpack full of ammo")
                << TABBED("r", "Armor")
                << TABBED("s", "Radiation shielding suit")
                << TABBED("v", "Invisibility")
                << TABBED("w", "Weapons");
#undef TABBED

        LOG_SCR_MSG(_E(D) "Examples:");
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "give arw"  _E(.) " for full ammo and armor " _E(l) "(equivalent to cheat IDFA)");
        LOG_SCR_MSG("  " _E(>) "Enter " _E(b) "give w2k1" _E(.) " for weapon two and key one");
        return true;
    }

    int player = CONSOLEPLAYER;
    if(argc == 3)
    {
        player = String(argv[1]).toInt();
        if(player < 0 || player >= MAXPLAYERS) return false;
    }

    if(IS_CLIENT)
    {
        if(argc < 2) return false;

        const String request = String("give ") + argv[1];
        NetCl_CheatRequest(request);
        return true;
    }
    else if(IS_NETGAME && !netSvAllowCheats)
    {
        return false;
    }
    else if(gfw_Rule(skill) == SM_HARD)
    {
        return false;
    }

    player_t* plr = &players[player];

    // Can't give to a plr who's not playing
    if(!plr->plr->inGame) return false;
    // Can't give to a dead player
    if(plr->health <= 0) return false;

    // Stuff is the 2nd arg.
    const String stuff = String(argv[1]).lower();
    for (mb_iterator i = stuff.begin(); i != stuff.end(); ++i)
    {
        const Char mnemonic = *i;
        switch (mnemonic)
        {
        case 'a': { // Ammo
            ammotype_t ammos = NUM_AMMO_TYPES;

            if((i + 1) != stuff.end() && (*(i + 1)).isNumeric())
            {
                const int arg = (*(++i)).delta('0');
                if(arg < AT_FIRST || arg >= NUM_AMMO_TYPES)
                {
                    LOG_SCR_ERROR("Ammo #%d unknown. Valid range %s")
                            << arg << Rangei(AT_FIRST, NUM_AMMO_TYPES).asText();
                    break;
                }
                ammos = ammotype_t(arg);
            }

            P_GiveAmmo(plr, ammos, -1 /* max rounds */);
            break;
        }

        case 'r': { // Armor
            int armor = 1;

            if((i + 1) != stuff.end() && (*(i + 1)).isNumeric())
            {
                const int arg = (*(++i)).delta('0');
                if(arg < 0 || arg >= 4)
                {
                    LOG_SCR_ERROR("Armor #%d unknown. Valid range %s")
                            << arg << Rangei(0, 4).asText();
                    break;
                }
                armor = arg;
            }
            P_GiveArmor(plr, armorClass[armor], armorPoints[armor]);
            break;
        }

        case 'k': { // Keys
            keytype_t keys = NUM_KEY_TYPES;

            if((i + 1) != stuff.end() && (*(i + 1)).isNumeric())
            {
                const int arg = (*(++i)).delta('0');
                if(arg < KT_FIRST || arg >= NUM_KEY_TYPES)
                {
                    LOG_SCR_ERROR("Key #%d unknown. Valid range %s")
                            << arg << Rangei(KT_FIRST, NUM_KEY_TYPES).asText();
                    break;
                }
                keys = keytype_t(arg);
            }
            P_GiveKey(plr, keys);
            break;
        }

        case 'w': { // Weapons
            weapontype_t weapons = NUM_WEAPON_TYPES;

            if((i + 1) != stuff.end() && (*(i + 1)).isNumeric())
            {
                const int arg = (*(++i)).delta('0');
                if(arg < WT_FIRST || arg >= NUM_WEAPON_TYPES)
                {
                    LOG_SCR_ERROR("Weapon #%d unknown. Valid range %s")
                            << arg << Rangei(WT_FIRST, NUM_WEAPON_TYPES).asText();
                    break;
                }
                weapons = weapontype_t(arg);
            }
            giveWeapon(plr, weapons);
            break;
        }

        case 'l': { // Laser Upgrades
            if((i + 1) != stuff.end() && (*(i + 1)).isNumeric())
            {
                switch ((*(++i)).delta('0'))
                {
                    case 1: // DEMONKEY1
                        giveLaserUpgrade(plr, IIT_DEMONKEY1);
                        break;
                    case 2: // DEMONKEY2
                        giveLaserUpgrade(plr, IIT_DEMONKEY2);
                        break;
                    case 3: // DEMONKEY3
                        giveLaserUpgrade(plr, IIT_DEMONKEY3);
                        break;
                    default:
                        LOG_SCR_ERROR("That upgrade does not exist. Valid upgrades: %s")
                                 << Rangei(1, 3).asText();
                }
            }
            else
            {
                // All the laser upgrades!
                giveLaserUpgrade(plr, IIT_DEMONKEY1);
                giveLaserUpgrade(plr, IIT_DEMONKEY2);
                giveLaserUpgrade(plr, IIT_DEMONKEY3);
            }

           break;
        }

        // Other Items
        case 'p': P_GiveBackpack(plr); break;
        case 'h': P_GiveBody(plr, healthLimit); break;

        // Powers
        case 'm': togglePower(plr, PT_ALLMAP); break;
        case 'f': togglePower(plr, PT_FLIGHT); break;
        case 'g': togglePower(plr, PT_INFRARED); break;
        case 'v': togglePower(plr, PT_INVISIBILITY); break;
        case 'i': togglePower(plr, PT_INVULNERABILITY); break;
        case 's': togglePower(plr, PT_IRONFEET); break;
        case 'b': togglePower(plr, PT_STRENGTH); break;

        default: // Unrecognized.
            LOG_SCR_ERROR("No such cheat `%c` found.") << mnemonic;
            break;
        }
    }

    return true;
}

// Massacre
// ===============================================================================================================

D_CMD(CheatMassacre)
{
    DE_UNUSED(src, argc, argv);
    App_Log(DE2_LOG_MAP, "%i monsters killed", P_Massacre());
    return true;
}

// Where
// ===============================================================================================================

static void printDebugInfo(player_t *plr)
{
    DE_ASSERT(plr != 0);

    if(G_GameState() != GS_MAP)
        return;

    mobj_t *plrMo = plr->plr->mo;
    if(!plrMo) return;

    // Output debug info to HUD and console
    {
        char textBuffer[256];
        sprintf(textBuffer,
                "MAP [%s]  X:%g  Y:%g  Z:%g",
                gfw_Session()->mapUri().path().c_str(),
                plrMo->origin[VX],
                plrMo->origin[VY],
                plrMo->origin[VZ]);

        P_SetMessageWithFlags(plr, textBuffer, LMF_NO_HIDE);

        // Also print some information to the console.
        LOG_SCR_NOTE(textBuffer);
    }

    Sector *sector = Mobj_Sector(plrMo);

    ::Uri *matUri = Materials_ComposeUri(P_GetIntp(sector, DMU_FLOOR_MATERIAL));
    LOG_SCR_MSG("FloorZ:%g Material:%s")
            << P_GetDoublep(sector, DMU_FLOOR_HEIGHT)
            << Str_Text(Uri_ToString(matUri));

    Uri_Delete(matUri);

    matUri = Materials_ComposeUri(P_GetIntp(sector, DMU_CEILING_MATERIAL));
    App_Log(DE2_MAP_MSG, "CeilingZ:%g Material:%s",
                          P_GetDoublep(sector, DMU_CEILING_HEIGHT), Str_Text(Uri_ToString(matUri)));
    Uri_Delete(matUri);

    App_Log(DE2_MAP_MSG, "Player height:%g Player radius:%g",
                          plrMo->height, plrMo->radius);
}

D_CMD(CheatWhere)
{
    DE_UNUSED(src, argc, argv);
    printDebugInfo(&players[CONSOLEPLAYER]);
    return true;
}
