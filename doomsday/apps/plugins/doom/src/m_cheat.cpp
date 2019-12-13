/** @file m_cheat.c  Cheat code sequences.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "m_cheat.h"

#include <de/Log>
#include <de/Range>
#include <de/String>
#include <de/Vector>

#include "jdoom.h"
#include "d_net.h"
#include "d_netcl.h"
#include "d_netsv.h"
#include "dmu_lib.h"
#include "g_eventsequence.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hu_msg.h"
#include "p_user.h"
#include "p_sound.h"
#include "player.h"

using namespace de;

typedef eventsequencehandler_t cheatfunc_t;

/// Helper macro for forming cheat callback function names.
#define CHEAT(x) G_Cheat##x

/// Helper macro for declaring cheat callback functions.
#define CHEAT_FUNC(x) int G_Cheat##x(int player, EventSequenceArg const *args, int numArgs)

CHEAT_FUNC(Music)
{
    DENG2_UNUSED(numArgs);

    if(player < 0 || player >= MAXPLAYERS)
        return false;

    player_t *plr = &players[player];

    int const numEpisodes = PlayableEpisodeCount();
    if(!numEpisodes) return false;

    // The number of episodes determines how to interpret the arguments.
    /// @note Logic here aims to be somewhat vanilla compatible, yet offer
    /// a limited degree of support for custom episodes. The "playmusic"
    /// cmd is a far more flexible method of changing music.
    String episodeId;
    int warpNumber;
    if(numEpisodes > 1)
    {
        episodeId  = String::number(args[0] - '0');
        warpNumber = args[1] - '0';
    }
    else
    {
        episodeId  = FirstPlayableEpisodeId();
        warpNumber = (args[0] - '0') * 10 + (args[1] - '0');
    }

    // Lookup and try to enqueue the Music for the referenced episode and map.
    const auto mapUri = TranslateMapWarpNumber(episodeId, warpNumber);
    if (S_MapMusic(mapUri))
    {
        P_SetMessageWithFlags(plr, STSTR_MUS, LMF_NO_HIDE);
        return true;
    }

    P_SetMessageWithFlags(plr, STSTR_NOMUS, LMF_NO_HIDE);
    return false;
}

CHEAT_FUNC(Reveal)
{
    DENG2_UNUSED2(args, numArgs);

    if(IS_NETGAME && gfw_Rule(deathmatch))
        return false;

    if(player < 0 || player >= MAXPLAYERS)
        return false;

    player_t *plr = &players[player];

    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    if(ST_AutomapIsOpen(player))
    {
        ST_CycleAutomapCheatLevel(player);
    }
    return true;
}

CHEAT_FUNC(Powerup)
{
    DENG2_UNUSED2(args, numArgs);
    if(player < 0 || player >= MAXPLAYERS)
        return false;

    P_SetMessageWithFlags(&players[player], STSTR_BEHOLD, LMF_NO_HIDE);
    return true;
}

CHEAT_FUNC(Powerup2)
{
    DENG2_UNUSED(numArgs);
    if(player < 0 || player >= MAXPLAYERS)
        return false;

    struct mnemonic_pair_s {
        char vanilla;
        char give;
    } static const mnemonics[] =
    {
        /*PT_INVULNERABILITY*/  { 'v', 'i' },
        /*PT_STRENGTH*/         { 's', 'b' },
        /*PT_INVISIBILITY*/     { 'i', 'v' },
        /*PT_IRONFEET*/         { 'r', 's' },
        /*PT_ALLMAP*/           { 'a', 'm' },
        /*PT_INFRARED*/         { 'l', 'g' }
    };
    static int const numMnemonics = int(sizeof(mnemonics) / sizeof(mnemonics[0]));

    for(int i = 0; i < numMnemonics; ++i)
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
    DENG2_UNUSED2(args, numArgs);
    if(player < 0 || player >= MAXPLAYERS)
        return false;

    mobj_t const *mob = players[CONSOLEPLAYER].plr->mo;
    String const text = String("angle:0x%1 position:%2")
                            .arg(mob->angle, 0, 16)
                            .arg(Vector3d(mob->origin).asText());
    P_SetMessageWithFlags(&players[player], text.toUtf8().constData(), LMF_NO_HIDE);
    return true;
}

/**
 * The multipurpose cheat ccmd.
 */
D_CMD(Cheat)
{
    DENG2_UNUSED2(src, argc);

    // Give each of the characters in argument two to the ST event handler.
    int const len = qstrlen(argv[1]);
    for(int i = 0; i < len; ++i)
    {
        event_t ev; de::zap(ev);
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
    DENG2_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("god");
        }
        else if((IS_NETGAME && !netSvAllowCheats) ||
                gfw_Rule(skill) == SM_NIGHTMARE)
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

            P_SetMessageWithFlags(plr, ((P_GetPlayerCheats(plr) & CF_GODMODE) ? STSTR_DQDON : STSTR_DQDOFF), LMF_NO_HIDE);
        }
    }
    return true;
}

D_CMD(CheatNoClip)
{
    DENG2_UNUSED(src);

    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("noclip");
        }
        else if((IS_NETGAME && !netSvAllowCheats) ||
                gfw_Rule(skill) == SM_NIGHTMARE)
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

            // Dead players can't cheat.
            if(plr->health <= 0) return false;

            plr->cheats ^= CF_NOCLIP;
            plr->update |= PSF_STATE;
            P_SetMessageWithFlags(plr, ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? STSTR_NCON : STSTR_NCOFF), LMF_NO_HIDE);
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
            P_DamageMobj(plr->plr->mo, nullptr, nullptr, 10000, false);
        }
    }
    return true;
}

D_CMD(CheatSuicide)
{
    DENG2_UNUSED(src);

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

    Hu_MsgStart(MSG_ANYKEY, SUICIDEOUTMAP, nullptr, 0, nullptr);
    return true;
}

D_CMD(CheatReveal)
{
    DENG2_UNUSED2(src, argc);
    // Server operator can always reveal.
    if(IS_NETGAME && !IS_NETWORK_SERVER)
        return false;

    int const option = String(argv[1]).toInt();
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
            ST_SetAutomapCheatLevel(i, option - 1);
        }
    }

    return true;
}

static void giveWeapon(player_t *player, weapontype_t weaponType)
{
    P_GiveWeapon(player, weaponType, false/*not dropped*/);
    if(weaponType == WT_EIGHTH)
    {
        P_SetMessageWithFlags(player, STSTR_CHOPPERS, LMF_NO_HIDE);
    }
}

static void togglePower(player_t *player, powertype_t powerType)
{
    P_TogglePower(player, powerType);
    P_SetMessageWithFlags(player, STSTR_BEHOLDX, LMF_NO_HIDE);
}

D_CMD(CheatGive)
{
    DENG2_UNUSED(src);

    if(G_GameState() != GS_MAP)
    {
        LOG_SCR_ERROR("Can only \"give\" when in a game!");
        return true;
    }

    if(argc != 2 && argc != 3)
    {
        LOG_SCR_NOTE("Usage:\n  give (stuff)\n  give (stuff) (player number)");

#define TABBED(A, B) "\n" _E(Ta) _E(b) "  " << A << " " _E(.) _E(Tb) << B
        LOG_SCR_MSG("Where (stuff) is one or more type:id codes"
                    " (if no id, give all of that type):")
                << TABBED("a", "Ammo")
                << TABBED("b", "Berserk")
                << TABBED("f", "Flight ability")
                << TABBED("g", "Light amplification visor")
                << TABBED("h", "Health")
                << TABBED("i", "Invulnerability")
                << TABBED("k", "Keys")
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
        player = String(argv[2]).toInt();
        if(player < 0 || player >= MAXPLAYERS) return false;
    }

    if(IS_CLIENT)
    {
        if(argc < 2) return false;

        String const request = String("give ") + argv[1];
        NetCl_CheatRequest(request.toUtf8().constData());
        return true;
    }

    if(IS_NETGAME && !netSvAllowCheats) return false;
    if(gfw_Rule(skill) == SM_NIGHTMARE) return false;

    player_t *plr = &players[player];

    // Can't give to a player who's not in the game.
    if(!plr->plr->inGame) return false;
    // Can't give to a dead player.
    if(plr->health <= 0) return false;

    String const stuff = String(argv[1]).toLower(); // Stuff is the 2nd arg.
    for(int i = 0; i < stuff.length(); ++i)
    {
        QChar const mnemonic = stuff.at(i);
        switch(mnemonic.toLatin1())
        {
        // Ammo:
        case 'a': {
            ammotype_t ammos = NUM_AMMO_TYPES; // All types.

            // Give one specific ammo type?
            if((i + 1) < stuff.length() && stuff.at(i + 1).isDigit())
            {
                int const arg = stuff.at( ++i ).digitValue();
                if(arg < AT_FIRST || arg >= NUM_AMMO_TYPES)
                {
                    LOG_SCR_ERROR("Ammo #%d unknown. Valid range %s")
                            << arg << Rangei(AT_FIRST, NUM_AMMO_TYPES).asText();
                    break;
                }
                ammos = ammotype_t(arg);
            }
            P_GiveAmmo(plr, ammos, -1 /*max rounds*/);
            break; }

        // Armor:
        case 'r': {
            int armor = 1;

            if((i + 1) < stuff.length() && stuff.at(i + 1).isDigit())
            {
                int const arg = stuff.at( ++i ).digitValue();
                if(arg < 0 || arg >= 4)
                {
                    LOG_SCR_ERROR("Armor #%d unknown. Valid range %s")
                            << arg << Rangei(0, 4).asText();
                    break;
                }
                armor = arg;
            }
            P_GiveArmor(plr, armorClass[armor], armorPoints[armor]);
            break; }

        // Keys:
        case 'k': {
            keytype_t keys = NUM_KEY_TYPES; // All types.

            // Give one specific key type?
            if((i + 1) < stuff.length() && stuff.at(i + 1).isDigit())
            {
                int const arg = stuff.at( ++i ).digitValue();
                if(arg < KT_FIRST || arg >= NUM_KEY_TYPES)
                {
                    LOG_SCR_ERROR("Key #%d unknown. Valid range %s")
                            << arg << Rangei(KT_FIRST, NUM_KEY_TYPES).asText();
                    break;
                }
                keys = keytype_t(arg);
            }
            P_GiveKey(plr, keys);
            break; }

        // Misc:
        case 'p': P_GiveBackpack(plr);                  break;
        case 'h': P_GiveHealth(plr, healthLimit);       break;

        // Powers:
        case 'm': togglePower(plr, PT_ALLMAP);          break;
        case 'f': togglePower(plr, PT_FLIGHT);          break;
        case 'g': togglePower(plr, PT_INFRARED);        break;
        case 'v': togglePower(plr, PT_INVISIBILITY);    break;
        case 'i': togglePower(plr, PT_INVULNERABILITY); break;
        case 's': togglePower(plr, PT_IRONFEET);        break;
        case 'b': togglePower(plr, PT_STRENGTH);        break;

        // Weapons:
        case 'w': {
            weapontype_t weapons = NUM_WEAPON_TYPES; // All types.

            // Give one specific weapon type?
            if((i + 1) < stuff.length() && stuff.at(i + 1).isDigit())
            {
                int const arg = stuff.at( ++i ).digitValue();
                if(arg < WT_FIRST || arg >= NUM_WEAPON_TYPES)
                {
                    LOG_SCR_ERROR("Weapon #%d unknown. Valid range %s")
                            << arg << Rangei(WT_FIRST, NUM_WEAPON_TYPES).asText();
                    break;
                }
                weapons = weapontype_t(arg);
            }
            giveWeapon(plr, weapons);
            break; }

        default: // Unrecognized.
            LOG_SCR_ERROR("Mnemonic '%c' unknown, cannot give") << mnemonic.toLatin1();
            break;
        }
    }

    // If the give expression matches that of a vanilla cheat code print the
    // associated confirmation message to the player's log.
    /// @todo fixme: Somewhat of kludge...
    if(stuff == "war2")
    {
        P_SetMessageWithFlags(plr, STSTR_FAADDED, LMF_NO_HIDE);
    }
    else if(stuff == "wakr3")
    {
        P_SetMessageWithFlags(plr, STSTR_KFAADDED, LMF_NO_HIDE);
    }

    return true;
}

D_CMD(CheatMassacre)
{
    DENG2_UNUSED3(src, argc, argv);

    if(G_GameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("kill");
        }
        else if((IS_NETGAME && !netSvAllowCheats) ||
                gfw_Rule(skill) == SM_NIGHTMARE)
        {
            return false;
        }
        else
        {
            int const killCount = P_Massacre();
            LOG_SCR_MSG("%i monsters killed") << killCount;
        }
    }
    return true;
}

D_CMD(CheatWhere)
{
    DENG2_UNUSED3(src, argc, argv);

    if(G_GameState() != GS_MAP)
        return true;

    player_t *plr = &players[CONSOLEPLAYER];
    mobj_t *plrMo = plr->plr->mo;
    if(!plrMo) return true;

    String const text = String("Map:%1 position:%2")
                            .arg(gfw_Session()->mapUri().path().asText())
                            .arg(Vector3d(plrMo->origin).asText());
    P_SetMessageWithFlags(plr, text.toUtf8().constData(), LMF_NO_HIDE);

    // Also print the some information to the console.
    LOG_SCR_NOTE("%s") << text;

    Sector *sector = Mobj_Sector(plrMo);

    uri_s *matUri = Materials_ComposeUri(P_GetIntp(sector, DMU_FLOOR_MATERIAL));
    LOG_SCR_MSG("FloorZ:%f Material:%s")
            << P_GetDoublep(sector, DMU_FLOOR_HEIGHT)
            << Str_Text(Uri_ToString(matUri));
    Uri_Delete(matUri);

    matUri = Materials_ComposeUri(P_GetIntp(sector, DMU_CEILING_MATERIAL));
    LOG_SCR_MSG("CeilingZ:%f Material:%s")
            << P_GetDoublep(sector, DMU_CEILING_HEIGHT)
            << Str_Text(Uri_ToString(matUri));
    Uri_Delete(matUri);

    LOG_SCR_MSG("Player height:%f Player radius:%f")
            << plrMo->height << plrMo->radius;

    return true;
}

void G_RegisterCheats()
{
/// Helper macro for registering new cheat event sequence handlers.
#define ADDCHEAT(name, callback)       G_AddEventSequence((name), CHEAT(callback))

/// Helper macro for registering new cheat event sequence command handlers.
#define ADDCHEATCMD(name, cmdTemplate) G_AddEventSequenceCommand((name), cmdTemplate)

    switch(gameMode)
    {
    case doom2_hacx:
        ADDCHEATCMD("blast",            "give wakr3 %p");
        ADDCHEATCMD("boots",            "give s %p");
        ADDCHEATCMD("bright",           "give g %p");
        ADDCHEATCMD("ghost",            "give v %p");
        ADDCHEAT   ("seeit%1",          Powerup2);
        ADDCHEAT   ("seeit",            Powerup);
        ADDCHEAT   ("show",             Reveal);
        ADDCHEATCMD("superman",         "give i %p");
        ADDCHEAT   ("tunes%1%2",        Music);
        ADDCHEATCMD("walk",             "noclip %p");
        ADDCHEATCMD("warpme%1%2",       "warp %1%2");
        ADDCHEATCMD("whacko",           "give b %p");
        ADDCHEAT   ("wheream",          MyPos);
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
        ADDCHEAT   ("idmus%1%2",        Music);
        ADDCHEATCMD("joelkoenigs",      "give w7 %p");
        ADDCHEATCMD("joshuastorms",     "give g %p");
        ADDCHEAT   ("kimhyers",         MyPos);
        ADDCHEATCMD("leesnyder%1%2",    "warp %1 %2");
        ADDCHEATCMD("marybregi",        "give v %p");
        ADDCHEATCMD("mikekoenigs",      "give war2 %p");
        ADDCHEATCMD("scottholman",      "give wakr3 %p");
        ADDCHEAT   ("sherrill",         Reveal);
        break;

    default: // Doom
        ADDCHEAT   ("idbehold%1",       Powerup2);
        ADDCHEAT   ("idbehold",         Powerup);

        // Note that in vanilla this cheat enables invulnerability until the
        // end of the current tic.
        ADDCHEATCMD("idchoppers",       "give w7 %p");

        ADDCHEATCMD("idclev%1%2",       ((gameModeBits & GM_ANY_DOOM)? "warp %1 %2" : "warp %1%2"));
        ADDCHEATCMD("idclip",           "noclip %p");
        ADDCHEATCMD("iddqd",            "god %p");
        ADDCHEAT   ("iddt",             Reveal);
        ADDCHEATCMD("idfa",             "give war2 %p");
        ADDCHEATCMD("idkfa",            "give wakr3 %p");
        ADDCHEAT   ("idmus%1%2",        Music);
        ADDCHEAT   ("idmypos",          MyPos);
        ADDCHEATCMD("idspispopd",       "noclip %p");
        break;
    }

#undef ADDCHEATCMD
#undef ADDCHEAT
}
