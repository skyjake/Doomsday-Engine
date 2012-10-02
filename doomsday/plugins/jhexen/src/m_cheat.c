/**
 * @file m_cheat.c
 * Cheats. @ingroup libhexen
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 1999 Activision
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
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef UNIX
# include <errno.h>
#endif

#include "jhexen.h"

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
#include "hu_inventory.h"
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

CHEAT_FUNC(Armor);
CHEAT_FUNC(Class);
CHEAT_FUNC(Class2);
CHEAT_FUNC(GiveKeys);
CHEAT_FUNC(God);
CHEAT_FUNC(Health);
CHEAT_FUNC(IDKFA);
CHEAT_FUNC(Init);
CHEAT_FUNC(Inventory);
CHEAT_FUNC(Massacre);
CHEAT_FUNC(NoClip);
CHEAT_FUNC(Pig);
CHEAT_FUNC(Puzzle);
CHEAT_FUNC(Quicken);
CHEAT_FUNC(Quicken2);
CHEAT_FUNC(Quicken3);
CHEAT_FUNC(Reveal);
CHEAT_FUNC(Script);
CHEAT_FUNC(Script2);
CHEAT_FUNC(Script3);
CHEAT_FUNC(Sound);
CHEAT_FUNC(Version);
CHEAT_FUNC(Weapons);
CHEAT_FUNC(Where);

static boolean cheatsEnabled(void)
{
    return !IS_NETGAME;
}

void G_RegisterCheats(void)
{
    ADDCHEAT("butcher",             Massacre);
    ADDCHEAT("casper",              NoClip);
    ADDCHEAT("clubmed",             Health);
    ADDCHEAT("conan",               IDKFA);
    ADDCHEAT("deliverance",         Pig);
    ADDCHEAT("indiana",             Inventory);
    ADDCHEAT("init",                Init);
    ADDCHEAT("locksmith",           GiveKeys);
    ADDCHEAT("mapsco",              Reveal);
    ADDCHEAT("martekmartekmartek",  Quicken3);
    ADDCHEAT("martekmartek",        Quicken2);
    ADDCHEAT("martek",              Quicken);
    ADDCHEAT("mrjones",             Version);
    ADDCHEAT("nra",                 Weapons);
    ADDCHEAT("noise",               Sound);
    ADDCHEAT("puke%1%2",            Script3);
    ADDCHEAT("puke%1",              Script2);
    ADDCHEAT("puke",                Script);
    ADDCHEAT("satan",               God);
    ADDCHEAT("shadowcaster%1",      Class2);
    ADDCHEAT("shadowcaster",        Class);
    ADDCHEAT("sherlock",            Puzzle);
    ADDCHEATCMD("visit%1%2",        "warp %1%2");
    ADDCHEAT("where",               Where);
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

    P_SetMessage(plr, LMF_NO_HIDE, ((P_GetPlayerCheats(plr) & CF_GODMODE) ? TXT_CHEATGODON : TXT_CHEATGODOFF));
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

static void giveArmor(player_t* plr)
{
    int i;
    plr->update |= PSF_ARMOR_POINTS;
    for(i = 0; i < NUMARMOR; ++i)
    {
        plr->armorPoints[i] = PCLASS_INFO(plr->class_)->armorIncrement[i];
    }
}

static void giveWeapons(player_t* plr)
{
    int i;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        plr->weapons[i].owned = true;
    }
    plr->pieces = WPIECE1|WPIECE2|WPIECE3;
    plr->update |= PSF_OWNED_WEAPONS;
}

static void giveAmmo(player_t* plr)
{
    int i;
    plr->update |= PSF_AMMO;
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        plr->ammo[i].owned = MAX_MANA;
    }
}

CHEAT_FUNC(GiveKeys)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    plr->update |= PSF_KEYS;
    plr->keys = 2047;
    P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATKEYS);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(GiveArmor)
{
    player_t* plr = &players[player];
    int i;

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    for(i = (int)ARMOR_FIRST; i < (int)NUMARMOR; ++i)
    {
        P_GiveArmor(plr, (armortype_t)i, PCLASS_INFO(plr->class_)->armorIncrement[i]);
    }
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Weapons)
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
    giveArmor(plr);

    P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATWEAPONS);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
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
    P_SetMessage(plr, LMF_NO_HIDE, ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? TXT_CHEATNOCLIPON : TXT_CHEATNOCLIPOFF));
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Sound)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    debugSound = !debugSound;
    P_SetMessage(plr, LMF_NO_HIDE, debugSound? TXT_CHEATSOUNDON : TXT_CHEATSOUNDOFF);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

static void printDebugInfo(int player)
{
    player_t* plr = &players[player];
    char textBuffer[256];
    BspLeaf* sub;
    AutoStr* path, *mapPath;
    Uri* uri, *mapUri;

    if(!plr->plr->mo) return;

    mapUri = G_ComposeMapUri(gameEpisode, gameMap);
    mapPath = Uri_ToString(mapUri);
    sprintf(textBuffer, "Map [%s]  x:%g  y:%g  z:%g",
            Str_Text(mapPath), plr->plr->mo->origin[VX], plr->plr->mo->origin[VY],
            plr->plr->mo->origin[VZ]);
    P_SetMessage(plr, LMF_NO_HIDE, textBuffer);
    Uri_Delete(mapUri);

    // Also print some information to the console.
    Con_Message("%s", textBuffer);
    sub = plr->plr->mo->bspLeaf;
    Con_Message("\nBspLeaf %i:\n", P_ToIndex(sub));

    uri = Materials_ComposeUri(P_GetIntp(sub, DMU_FLOOR_MATERIAL));
    path = Uri_ToString(uri);
    Con_Message("  FloorZ:%g Material:%s\n", P_GetDoublep(sub, DMU_FLOOR_HEIGHT), Str_Text(path));
    Uri_Delete(uri);

    uri = Materials_ComposeUri(P_GetIntp(sub, DMU_CEILING_MATERIAL));
    path = Uri_ToString(uri);
    Con_Message("  CeilingZ:%g Material:%s\n", P_GetDoublep(sub, DMU_CEILING_HEIGHT), Str_Text(path));
    Uri_Delete(uri);

    Con_Message("Player height:%g   Player radius:%g\n",
                plr->plr->mo->height, plr->plr->mo->radius);
}

CHEAT_FUNC(Where)
{
    DENG_UNUSED(args);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    if(!userGame) return false;

    printDebugInfo(player);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Health)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    plr->update |= PSF_HEALTH;
    if(plr->morphTics)
    {
        plr->health = plr->plr->mo->health = MAXMORPHHEALTH;
    }
    else
    {
        plr->health = plr->plr->mo->health = maxHealth;
    }
    P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATHEALTH);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Inventory)
{
    player_t* plr = &players[player];
    int i, j;

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    for(i = IIT_NONE + 1; i < IIT_FIRSTPUZZITEM; ++i)
    {
        for(j = 0; j < 25; ++j)
        {
            P_InventoryGive(player, i, false);
        }
    }

    P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATINVITEMS3);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Puzzle)
{
    player_t* plr = &players[player];
    int i;

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    for(i = IIT_FIRSTPUZZITEM; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        P_InventoryGive(player, i, false);
    }

    P_SetMessage(plr, LMF_NO_HIDE, TXT_CHEATINVITEMS3);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Init)
{
    player_t* plr = &players[player];

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

CHEAT_FUNC(Pig)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
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
    return true;
}

CHEAT_FUNC(Massacre)
{
    player_t* plr = &players[player];
    int count;
    char buf[80];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    count = P_Massacre();
    sprintf(buf, "%d monsters killed.", count);
    P_SetMessage(plr, LMF_NO_HIDE, buf);
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(IDKFA)
{
    player_t* plr = &players[player];
    int i;

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
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
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    P_SetMessage(plr, LMF_NO_HIDE, "Trying to cheat? That's one...");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Quicken2)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    P_SetMessage(plr, LMF_NO_HIDE, "That's two...");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Quicken3)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
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
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    P_SetMessage(plr, LMF_NO_HIDE, "Enter new player class number");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Class2)
{
    player_t* plr = &players[player];

    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    P_PlayerChangeClass(plr, args[0] - '0');
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Version)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    DD_Execute(false, "version");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Script)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    P_SetMessage(plr, LMF_NO_HIDE, "Run which script(01-99)?");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Script2)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    P_SetMessage(plr, LMF_NO_HIDE, "Run which script(01-99)?");
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Script3)
{
    player_t* plr = &players[player];
    int script, tens, ones;
    byte scriptArgs[3];
    char textBuffer[40];

    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
    if(gameSkill == SM_NIGHTMARE) return false;
    // Dead players can't cheat.
    if(plr->health <= 0) return false;

    tens = args[0] - '0';
    ones = args[1] - '0';
    script = tens * 10 + ones;
    if(script < 1 || script > 99) return false;

    scriptArgs[0] = scriptArgs[1] = scriptArgs[2] = 0;

    if(P_StartACS(script, 0, scriptArgs, plr->plr->mo, NULL, 0))
    {
        sprintf(textBuffer, "Running script %.2d", script);
        P_SetMessage(plr, LMF_NO_HIDE, textBuffer);
    }
    S_LocalSound(SFX_PLATFORM_STOP, NULL);
    return true;
}

CHEAT_FUNC(Reveal)
{
    player_t* plr = &players[player];

    DENG_UNUSED(args);
    DENG_ASSERT(player >= 0 && player < MAXPLAYERS);

    if(IS_NETGAME) return false;
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
    int player = CONSOLEPLAYER;
    size_t i, stuffLen;
    player_t* plr;
    char buf[100];

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
        Con_Printf(" h - health\n");
        Con_Printf(" i - items\n");
        Con_Printf(" k - keys\n");
        Con_Printf(" p - puzzle\n");
        Con_Printf(" r - armor\n");
        Con_Printf(" w - weapons\n");
        Con_Printf("Example: 'give ikw' gives items, keys and weapons.\n");
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
                    plr->ammo[idx].owned = MAX_MANA;
                    break;
                }
            }

            // Give all ammo.
            plr->update |= PSF_AMMO;
            { int j;
            for(j = 0; j < NUM_AMMO_TYPES; ++j)
                plr->ammo[j].owned = MAX_MANA;
            }
            break;

        case 'h':
            CHEAT(Health)(player, 0/*no args*/, 0/*no args*/);
            break;

        case 'i':
            CHEAT(Inventory)(player, 0/*no args*/, 0/*no args*/);
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
                    plr->keys |= (1 << idx);
                    break;
                }
            }

            // Give all keys.
            CHEAT(GiveKeys)(player, 0/*no args*/, 0/*no args*/);
            break;

        case 'p':
            CHEAT(Puzzle)(player, 0/*no args*/, 0/*no args*/);
            break;

        case 'r':
            if(i < stuffLen)
            {
                char* end;
                long idx;
                errno = 0;
                idx = strtol(&buf[i+1], &end, 0);
                if(end != &buf[i+1] && errno != ERANGE)
                {
                    i += end - &buf[i+1];
                    if(idx < ARMOR_FIRST || idx >= NUMARMOR)
                    {
                        Con_Printf("Unknown armor #%d (valid range %d-%d).\n",
                                   (int)idx, ARMOR_FIRST, NUMARMOR-1);
                        break;
                    }

                    // Give one specific armor.
                    P_GiveArmor(plr, (armortype_t)idx, PCLASS_INFO(plr->class_)->armorIncrement[idx]);
                    break;
                }
            }

            // Give all armors.
            CHEAT(GiveArmor)(player, 0/*no args*/, 0/*no args*/);
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
                    plr->update |= PSF_OWNED_WEAPONS;
                    plr->weapons[idx].owned = true;
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
    CHEAT(Massacre)(CONSOLEPLAYER, 0/*no args*/, 0/*no args*/);
    return true;
}

D_CMD(CheatWhere)
{
    CHEAT(Where)(CONSOLEPLAYER, 0/*no args*/, 0/*no args*/);
    return true;
}

D_CMD(CheatPig)
{
    if(IS_NETGAME || !userGame) return false;
    if(gameSkill == SM_NIGHTMARE || players[CONSOLEPLAYER].health <= 0) return false;

    CHEAT(Pig)(CONSOLEPLAYER, 0/*no args*/, 0/*no args*/);
    return true;
}

D_CMD(CheatShadowcaster)
{
    EventSequenceArg args[2];

    if(IS_NETGAME || !userGame) return false;
    if(gameSkill == SM_NIGHTMARE || players[CONSOLEPLAYER].health <= 0) return false;

    args[0] = atoi(argv[1]) + '0';
    CHEAT(Class2)(CONSOLEPLAYER, args, 1);
    return true;
}

D_CMD(CheatRunScript)
{
    EventSequenceArg args[2];
    int num;

    if(IS_NETGAME) return false;
    if(!userGame) return false;

    num = atoi(argv[1]);
    args[0] = num / 10 + '0';
    args[1] = num % 10 + '0';
    CHEAT(Script3)(CONSOLEPLAYER, args, 2);
    return true;
}
