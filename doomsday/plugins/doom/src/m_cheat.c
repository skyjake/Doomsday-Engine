/**
 * @file m_cheat.c
 * Cheats. @ingroup libdoom
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 1993-1996 by id Software, Inc.
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

typedef eventsequencehandler_t cheatfunc_t;

/// Helper macro for forming cheat callback function names.
#define CHEAT(x) G_Cheat##x

/// Helper macro for declaring cheat callback functions.
#define CHEAT_FUNC(x) int G_Cheat##x(int player, const EventSequenceArg* args, int numArgs)

/// Helper macro for registering new cheat event sequence handlers.
#define ADDCHEAT(name, callback) G_AddEventSequence((name), CHEAT(callback))

/// Helper macro for registering new cheat event sequence command handlers.
#define ADDCHEATCMD(name, cmdTemplate) G_AddEventSequenceCommand((name), cmdTemplate)

CHEAT_FUNC(GiveAllMap);
CHEAT_FUNC(GiveChainsaw);
CHEAT_FUNC(GiveInvisibility);
CHEAT_FUNC(GiveInvulnerability);
CHEAT_FUNC(GiveInfrared);
CHEAT_FUNC(GiveIronFeet);
CHEAT_FUNC(GiveStrength);
CHEAT_FUNC(GiveWeaponsAmmoArmor);
CHEAT_FUNC(GiveWeaponsAmmoArmorKeys);
CHEAT_FUNC(God);
CHEAT_FUNC(Music);
CHEAT_FUNC(MyPos);
CHEAT_FUNC(NoClip);
CHEAT_FUNC(Powerup2);
CHEAT_FUNC(Powerup);
CHEAT_FUNC(Reveal);

static boolean cheatsEnabled(void)
{
#ifdef _DEBUG
    if(IS_NETWORK_SERVER) return true; // Server operator can always cheat.
#endif
    return !IS_NETGAME;
}

void G_RegisterCheats(void)
{
    switch(gameMode)
    {
    case doom2_hacx:
        ADDCHEAT("blast",           GiveWeaponsAmmoArmorKeys);
        ADDCHEAT("boots",           GiveIronFeet);
        ADDCHEAT("bright",          GiveInfrared);
        ADDCHEAT("ghost",           GiveInvisibility);
        ADDCHEAT("seeit%1",         Powerup2);
        ADDCHEAT("seeit",           Powerup);
        ADDCHEAT("show",            Reveal);
        ADDCHEAT("superman",        GiveInvulnerability);
        ADDCHEAT("tunes%1%2",       Music);
        ADDCHEAT("walk",            NoClip);
        ADDCHEATCMD("warpme%1%2",   "warp %1%2");
        ADDCHEAT("whacko",          GiveStrength);
        ADDCHEAT("wheream",         MyPos);
        ADDCHEAT("wuss",            God);
        ADDCHEAT("zap",             GiveChainsaw);
        break;

    case doom_chex:
        ADDCHEAT("allen",           GiveIronFeet);
        ADDCHEAT("andrewbenson",    GiveInvulnerability);
        ADDCHEAT("charlesjacobi",   NoClip);
        ADDCHEAT("davidbrus",       God);
        ADDCHEAT("deanhyers",       GiveStrength);
        ADDCHEAT("digitalcafe",     GiveAllMap);
        ADDCHEAT("idmus%1%2",       Music);
        ADDCHEAT("joelkoenigs",     GiveChainsaw);
        ADDCHEAT("joshuastorms",    GiveInfrared);
        ADDCHEAT("kimhyers",        MyPos);
        ADDCHEATCMD("leesnyder%1%2", "warp %1%2");
        ADDCHEAT("marybregi",       GiveInvisibility);
        ADDCHEAT("mikekoenigs",     GiveWeaponsAmmoArmor);
        ADDCHEAT("scottholman",     GiveWeaponsAmmoArmorKeys);
        ADDCHEAT("sherrill",        Reveal);
        break;

    default: // Doom
        ADDCHEAT("idbehold%1",      Powerup2);
        ADDCHEAT("idbehold",        Powerup);
        ADDCHEAT("idchoppers",      GiveChainsaw);
        ADDCHEATCMD("idclev%1%2",   "warp %1%2");
        ADDCHEAT("idclip",          NoClip);
        ADDCHEAT("iddqd",           God);
        ADDCHEAT("iddt",            Reveal);
        ADDCHEAT("idfa",            GiveWeaponsAmmoArmor);
        ADDCHEAT("idkfa",           GiveWeaponsAmmoArmorKeys);
        ADDCHEAT("idmus%1%2",       Music);
        ADDCHEAT("idmypos",         MyPos);
        ADDCHEAT("idspispopd",      NoClip);
        break;
    }
}

CHEAT_FUNC(God)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
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
    return true;
}

static void giveArmor(int player, int val)
{
    player_t* plr = &players[player];

    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    // Support idfa/idkfa DEH Misc values
    val = MINMAX_OF(1, val, 3);
    plr->armorPoints = armorPoints[val];
    plr->armorType = armorClass[val];

    plr->update |= PSF_STATE | PSF_ARMOR_POINTS;
}

static void giveWeapons(player_t* plr)
{
    int i;

    DENG_ASSERT(plr);

    plr->update |= PSF_OWNED_WEAPONS;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        plr->weapons[i].owned = true;
    }
}

static void giveAmmo(player_t* plr)
{
    int i;

    DENG_ASSERT(plr);

    plr->update |= PSF_AMMO;
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        plr->ammo[i].owned = plr->ammo[i].max;
    }
}

static void giveKeys(player_t* plr)
{
    int i;

    DENG_ASSERT(plr);

    plr->update |= PSF_KEYS;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        plr->keys[i] = true;
    }
}

CHEAT_FUNC(GiveWeaponsAmmoArmor)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    giveWeapons(plr);
    giveAmmo(plr);
    giveArmor(player, 2);

    P_SetMessage(plr, LMF_NO_HIDE, STSTR_FAADDED);
    return true;
}

CHEAT_FUNC(GiveWeaponsAmmoArmorKeys)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    giveWeapons(plr);
    giveAmmo(plr);
    giveKeys(plr);
    giveArmor(player, 3);

    P_SetMessage(plr, LMF_NO_HIDE, STSTR_KFAADDED);
    return true;
}

CHEAT_FUNC(Music)
{
    player_t* plr = &players[player];
    int musnum;

    DENG_ASSERT(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

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

CHEAT_FUNC(NoClip)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    plr->cheats ^= CF_NOCLIP;
    plr->update |= PSF_STATE;
    P_SetMessage(plr, LMF_NO_HIDE, ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? STSTR_NCON : STSTR_NCOFF));
    return true;
}

CHEAT_FUNC(Reveal)
{
    player_t* plr = &players[player];

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
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    P_SetMessage(plr, LMF_NO_HIDE, STSTR_BEHOLD);
    return true;
}

static void givePower(player_t* plr, powertype_t type)
{
    if(type < 0 && type >= NUM_POWER_TYPES) return;

    if(!plr->powers[type])
    {
        P_GivePower(plr, type);
    }
    else if(type == PT_STRENGTH || type == PT_FLIGHT || type == PT_ALLMAP)
    {
        P_TakePower(plr, type);
    }
}

CHEAT_FUNC(Powerup2)
{
    static const char values[] = { 'v', 's', 'i', 'r', 'a', 'l' };
    static const int numValues = (int)(sizeof(values) / sizeof(values[0]));

    player_t* plr = &players[player];
    int i;

    DENG_ASSERT(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    for(i = 0; i < numValues; ++i)
    {
        if(args[0] != values[i]) continue;

        givePower(plr, (powertype_t) i);
        P_SetMessage(plr, LMF_NO_HIDE, STSTR_BEHOLDX);
        return true;
    }
    return false;
}

CHEAT_FUNC(GiveInvulnerability)
{
    EventSequenceArg _args[1] = {'v'};
    DENG_UNUSED(args);
    return CHEAT(Powerup)(player, _args, 1);
}

CHEAT_FUNC(GiveStrength)
{
    EventSequenceArg _args[1] = {'s'};
    DENG_UNUSED(args);
    return CHEAT(Powerup)(player, _args, 1);
}

CHEAT_FUNC(GiveInvisibility)
{
    EventSequenceArg _args[1] = {'i'};
    DENG_UNUSED(args);
    return CHEAT(Powerup)(player, _args, 1);
}

CHEAT_FUNC(GiveIronFeet)
{
    EventSequenceArg _args[1] = {'r'};
    DENG_UNUSED(args);
    return CHEAT(Powerup)(player, _args, 1);
}

CHEAT_FUNC(GiveAllMap)
{
    EventSequenceArg _args[1] = {'a'};
    DENG_UNUSED(args);
    return CHEAT(Powerup)(player, _args, 1);
}

CHEAT_FUNC(GiveInfrared)
{
    EventSequenceArg _args[1] = {'l'};
    DENG_UNUSED(args);
    return CHEAT(Powerup)(player, _args, 1);
}

CHEAT_FUNC(GiveChainsaw)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    plr->weapons[WT_EIGHTH].owned = true;
    plr->powers[PT_INVULNERABILITY] = true;
    P_SetMessage(plr, LMF_NO_HIDE, STSTR_CHOPPERS);
    return true;
}

CHEAT_FUNC(MyPos)
{
    player_t* plr = &players[player];
    char buf[80];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    sprintf(buf, "ang=0x%x;x,y,z=(%g,%g,%g)",
            players[CONSOLEPLAYER].plr->mo->angle,
            players[CONSOLEPLAYER].plr->mo->origin[VX],
            players[CONSOLEPLAYER].plr->mo->origin[VY],
            players[CONSOLEPLAYER].plr->mo->origin[VZ]);
    P_SetMessage(plr, LMF_NO_HIDE, buf);
    return true;
}

static void printDebugInfo(player_t* plr)
{
    char textBuffer[256];
    BspLeaf* sub;
    AutoStr* path, *mapPath;
    Uri* uri, *mapUri;

    if(!plr->plr->mo || !userGame) return;

    mapUri = G_ComposeMapUri(gameEpisode, gameMap);
    mapPath = Uri_ToString(mapUri);
    sprintf(textBuffer, "MAP [%s]  X:%g  Y:%g  Z:%g",
            Str_Text(mapPath), plr->plr->mo->origin[VX], plr->plr->mo->origin[VY],
            plr->plr->mo->origin[VZ]);
    P_SetMessage(plr, LMF_NO_HIDE, textBuffer);
    Uri_Delete(mapUri);

    // Also print some information to the console.
    Con_Message("%s", textBuffer);
    sub = plr->plr->mo->bspLeaf;
    Con_Message("BspLeaf %i / Sector %i:", P_ToIndex(sub), P_ToIndex(P_GetPtrp(sub, DMU_SECTOR)));

    uri = Materials_ComposeUri(P_GetIntp(sub, DMU_FLOOR_MATERIAL));
    path = Uri_ToString(uri);
    Con_Message("  FloorZ:%g Material:%s", P_GetDoublep(sub, DMU_FLOOR_HEIGHT), Str_Text(path));
    Uri_Delete(uri);

    uri = Materials_ComposeUri(P_GetIntp(sub, DMU_CEILING_MATERIAL));
    path = Uri_ToString(uri);
    Con_Message("  CeilingZ:%g Material:%s", P_GetDoublep(sub, DMU_CEILING_HEIGHT), Str_Text(path));
    Uri_Delete(uri);

    Con_Message("Player height:%g Player radius:%g",
                plr->plr->mo->height, plr->plr->mo->radius);
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
        else
        {
            int player = CONSOLEPLAYER;

            if(IS_NETGAME && !netSvAllowCheats) return false;

            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            if(!players[player].plr->inGame) return false;

            CHEAT(God)(player, 0/*no args*/, 0/*no args*/);
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

            if(IS_NETGAME && !netSvAllowCheats) return false;

            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS) return false;
            }

            if(!players[player].plr->inGame) return false;

            CHEAT(NoClip)(player, 0/*no args*/, 0/*no args*/);
        }
    }
    return true;
}

static int suicideResponse(msgresponse_t response, int userValue, void* userPointer)
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

    if(!cheatsEnabled()) return false;

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

D_CMD(CheatGive)
{
    char buf[100];
    int player = CONSOLEPLAYER;
    player_t* plr;
    size_t i, stuffLen;

    if(IS_CLIENT)
    {
        if(argc != 2) return false;

        sprintf(buf, "give %s", argv[1]);
        NetCl_CheatRequest(buf);
        return true;
    }

    if(IS_NETGAME && !netSvAllowCheats) return false;

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
        if(player < 0 || player >= MAXPLAYERS) return false;
    }

    if(G_GameState() != GS_MAP)
    {
        Con_Printf("Can only \"give\" when in a game!\n");
        return true;
    }

    // Can't give to a plr who's not in the game.
    if(!players[player].plr->inGame) return true;
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
    if(!cheatsEnabled()) return false;

    if(G_GameState() != GS_MAP)
    {
        S_LocalSound(SFX_OOF, NULL);
        Con_Printf("Can only exit a map when in a game!\n");
        return true;
    }

    G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
    return true;
}
