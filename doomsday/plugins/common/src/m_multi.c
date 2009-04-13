/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * m_multi.c: Multiplayer Menu (for jDoom, jHeretic and jHexen).
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "hu_stuff.h"
#include "hu_menu.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void            DrawMultiplayerMenu(void);
void            DrawGameSetupMenu(void);

void            SCEnterHostMenu(int option, void* data);
void            SCEnterJoinMenu(int option, void* data);
void            SCEnterGameSetup(int option, void* data);
void            SCSetProtocol(int option, void* data);
void            SCGameSetupFunc(int option, void* data);
void            SCGameSetupEpisode(int option, void* data);
void            SCGameSetupMap(int option, void* data);
void            SCGameSetupSkill(int option, void* data);
void            SCGameSetupDeathmatch(int option, void* data);
void            SCGameSetupDamageMod(int option, void* data);
void            SCGameSetupHealthMod(int option, void* data);
void            SCGameSetupGravity(int option, void* data);
void            SCOpenServer(int option, void* data);
void            SCCloseServer(int option, void* data);
void            SCChooseHost(int option, void* data);
void            SCStartStopDisconnect(int option, void* data);

void            ResetJoinMenuItems();

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

menuitem_t MultiplayerItems[] = {
    {ITT_EFUNC, 0, "join game", SCEnterJoinMenu, 0 },
    {ITT_EFUNC, 0, "host game", SCEnterHostMenu, 0 },
};

menuitem_t MultiplayerServerItems[] = {
    {ITT_EFUNC, 0, "game setup", SCEnterHostMenu, 0 },
    {ITT_EFUNC, 0, "close server", SCCloseServer, 0 }
};

menuitem_t MultiplayerClientItems[] = {
    {ITT_EFUNC, 0, "disconnect", SCEnterJoinMenu, 0 }
};

menu_t MultiplayerMenu = {
    0,
    116, 70,
    DrawMultiplayerMenu,
    2, MultiplayerItems,
    0, MENU_NEWGAME,
    huFontA,gs.cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A,
    0, 2
};

#if __JHEXEN__ || __JSTRIFE__

#  define NUM_GAMESETUP_ITEMS       11

menuitem_t GameSetupItems1[] = {
    {ITT_LRFUNC, 0, "MAP:", SCGameSetupMap, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_LRFUNC, 0, "SKILL:", SCGameSetupSkill, 0},
    {ITT_EFUNC, 0, "DEATHMATCH:", SCGameSetupFunc, 0, NULL, &GAMERULES.deathmatch },
    {ITT_EFUNC, 0, "MONSTERS:", SCGameSetupFunc, 0, NULL, &GAMERULES.noMonsters },
    {ITT_EFUNC, 0, "RANDOM CLASSES:", SCGameSetupFunc, 0, NULL, &GAMERULES.randomClass },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &GAMERULES.noMaxZRadiusAttack },
    {ITT_LRFUNC, 0, "DAMAGE MULTIPLIER:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MULTIPLIER:", SCGameSetupHealthMod, 0},
    {ITT_LRFUNC, 0, "GRAVITY MULTIPLIER:", SCGameSetupGravity, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0}
};

#else

#  if __JHERETIC__

#    define NUM_GAMESETUP_ITEMS     14

menuitem_t GameSetupItems1[] =  // for Heretic
{
    {ITT_LRFUNC, 0, "EPISODE :", SCGameSetupEpisode, 0},
    {ITT_LRFUNC, 0, "MAP :", SCGameSetupMap, 0},
    {ITT_LRFUNC, 0, "SKILL :", SCGameSetupSkill, 0},
    {ITT_LRFUNC, 0, "DEATHMATCH :", SCGameSetupDeathmatch, 0},
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL, &GAMERULES.noMonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &GAMERULES.respawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &GAMERULES.jumpAllow },
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &GAMERULES.noCoopDamage },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &GAMERULES.noTeamDamage },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &GAMERULES.noMaxZRadiusAttack },
    {ITT_LRFUNC, 0, "DAMAGE MULTIPLIER:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MULTIPLIER:", SCGameSetupHealthMod, 0},
    {ITT_LRFUNC, 0, "GRAVITY MULTIPLIER:", SCGameSetupGravity, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0 }
};

#  elif __JDOOM__

#    define NUM_GAMESETUP_ITEMS     19

menuitem_t GameSetupItems1[] =  // for Doom 1
{
    {ITT_LRFUNC, 0, "EPISODE :", SCGameSetupEpisode, 0},
    {ITT_LRFUNC, 0, "MAP :", SCGameSetupMap, 0},
    {ITT_LRFUNC, 0, "SKILL :", SCGameSetupSkill, 0},
    {ITT_LRFUNC, 0, "MODE :", SCGameSetupDeathmatch, 0},
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL, &GAMERULES.noMonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &GAMERULES.respawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &GAMERULES.jumpAllow },
    {ITT_EFUNC, 0, "ALLOW BFG AIMING :", SCGameSetupFunc, 0, NULL, &GAMERULES.freeAimBFG},
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &GAMERULES.noCoopDamage },
    {ITT_EFUNC, 0, "NO COOP WEAPONS :", SCGameSetupFunc, 0, NULL, &GAMERULES.noCoopWeapons },
    {ITT_EFUNC, 0, "NO COOP OBJECTS :", SCGameSetupFunc, 0, NULL, &GAMERULES.noCoopAnything },
    {ITT_EFUNC, 0, "COOP ITEMS RESPAWN :", SCGameSetupFunc, 0, NULL, &GAMERULES.coopRespawnItems },
    {ITT_EFUNC, 0, "NO BFG 9000 :", SCGameSetupFunc, 0, NULL, &GAMERULES.noBFG },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &GAMERULES.noTeamDamage },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &GAMERULES.noMaxZRadiusAttack },
    {ITT_LRFUNC, 0, "DAMAGE MULTIPLIER:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MULTIPLIER:", SCGameSetupHealthMod, 0},
    {ITT_LRFUNC, 0, "GRAVITY MULTIPLIER:", SCGameSetupGravity, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0 }
};

menuitem_t GameSetupItems2[] =  // for Doom 2
{
    {ITT_LRFUNC, 0, "MAP :", SCGameSetupMap, 0},
    {ITT_LRFUNC, 0, "SKILL :", SCGameSetupSkill, 0},
    {ITT_LRFUNC, 0, "MODE :", SCGameSetupDeathmatch, 0},
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL,  &GAMERULES.noMonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &GAMERULES.respawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &GAMERULES.jumpAllow },
    {ITT_EFUNC, 0, "ALLOW BFG AIMING :", SCGameSetupFunc, 0, NULL, &GAMERULES.freeAimBFG},
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &GAMERULES.noCoopDamage },
    {ITT_EFUNC, 0, "NO COOP WEAPONS :", SCGameSetupFunc, 0, NULL, &GAMERULES.noCoopWeapons },
    {ITT_EFUNC, 0, "NO COOP OBJECTS :", SCGameSetupFunc, 0, NULL, &GAMERULES.noCoopAnything },
    {ITT_EFUNC, 0, "COOP ITEMS RESPAWN :", SCGameSetupFunc, 0, NULL, &GAMERULES.coopRespawnItems },
    {ITT_EFUNC, 0, "NO BFG 9000 :", SCGameSetupFunc, 0, NULL, &GAMERULES.noBFG },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &GAMERULES.noTeamDamage },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &GAMERULES.noMaxZRadiusAttack },
    {ITT_LRFUNC, 0, "DAMAGE MULTIPLIER:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MULTIPLIER:", SCGameSetupHealthMod, 0},
    {ITT_LRFUNC, 0, "GRAVITY MULTIPLIER:", SCGameSetupGravity, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0 }
};

#  elif __JDOOM64__

#    define NUM_GAMESETUP_ITEMS     18

menuitem_t GameSetupItems1[] =
{
    {ITT_LRFUNC, 0, "MAP :", SCGameSetupMap, 0},
    {ITT_LRFUNC, 0, "SKILL :", SCGameSetupSkill, 0},
    {ITT_LRFUNC, 0, "MODE :", SCGameSetupDeathmatch, 0},
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL,  &GAMERULES.noMonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &GAMERULES.respawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &GAMERULES.jumpAllow },
    {ITT_EFUNC, 0, "ALLOW BFG AIMING :", SCGameSetupFunc, 0, NULL, &GAMERULES.freeAimBFG},
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &GAMERULES.noCoopDamage },
    {ITT_EFUNC, 0, "NO COOP WEAPONS :", SCGameSetupFunc, 0, NULL, &GAMERULES.noCoopWeapons },
    {ITT_EFUNC, 0, "NO COOP OBJECTS :", SCGameSetupFunc, 0, NULL, &GAMERULES.noCoopAnything },
    {ITT_EFUNC, 0, "COOP ITEMS RESPAWN :", SCGameSetupFunc, 0, NULL, &GAMERULES.coopRespawnItems },
    {ITT_EFUNC, 0, "NO BFG 9000 :", SCGameSetupFunc, 0, NULL, &GAMERULES.noBFG },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &GAMERULES.noTeamDamage },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &GAMERULES.noMaxZRadiusAttack },
    {ITT_LRFUNC, 0, "DAMAGE MULTIPLIER:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MULTIPLIER:", SCGameSetupHealthMod, 0},
    {ITT_LRFUNC, 0, "GRAVITY MULTIPLIER:", SCGameSetupGravity, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0 }
};

#  endif

#endif

menu_t GameSetupMenu = {
    0,
#  if __JDOOM__ || __JDOOM64__
    90, 54,
#  elif __JHERETIC__
    74, 64,
#  elif __JHEXEN__  || __JSTRIFE__
    90, 64,
#  endif
    DrawGameSetupMenu,
    NUM_GAMESETUP_ITEMS, GameSetupItems1,
    0, MENU_MULTIPLAYER,
    huFontA,                  //1, 0, 0,
    gs.cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A,
    0, NUM_GAMESETUP_ITEMS
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int Executef(int silent, char* format, ...)
{
    va_list             argptr;
    char                buffer[512];

    va_start(argptr, format);
    vsnprintf(buffer, sizeof(buffer), format, argptr);
    va_end(argptr);
    return DD_Execute(silent, buffer);
}

void DrANumber(int number, int x, int y)
{
    char                buff[40];

    sprintf(buff, "%i", number);

    M_WriteText2(x, y, buff, huFontA, 1, 1, 1, Hu_MenuAlpha());
}

void MN_DrCenterTextA_CS(char* text, int centerX, int y)
{
    M_WriteText2(centerX - M_StringWidth(text, huFontA) / 2, y, text,
                 huFontA, 1, 0, 0, Hu_MenuAlpha());
}

void MN_DrCenterTextB_CS(char *text, int centerX, int y)
{
    M_WriteText2(centerX - M_StringWidth(text, huFontB) / 2, y, text,
                 huFontB, 1, 0, 0, Hu_MenuAlpha());
}

void DrawMultiplayerMenu(void)
{
    M_DrawTitle(GET_TXT(TXT_MULTIPLAYER), MultiplayerMenu.y - 30);
}

void DrawGameSetupMenu(void)
{
    char*               boolText[] = {"NO", "YES"};
#if __JDOOM__
    char*               skillText[] = {"BABY", "EASY", "MEDIUM", "HARD", "NIGHTMARE"};
#else
    char*               skillText[] = {"BABY", "EASY", "MEDIUM", "HARD"};
#endif
#if __JDOOM__ || __JDOOM64__
    //char*             freeLookText[3] = {"NO", "NOT BFG", "ALL"};
    char*               dmText[] = {"COOPERATIVE", "DEATHMATCH 1", "DEATHMATCH 2"};
#else
    char*               dmText[] = {"NO", "YES", "YES"};
#endif
    char                buf[50];
    menu_t*             menu = &GameSetupMenu;
    int                 idx;
#if __JHEXEN__
    char*               mapName = P_GetMapName(P_TranslateMap(gs.netMap));
#elif __JSTRIFE__
    char*               mapName = "unnamed";
#endif

    M_DrawTitle(GET_TXT(TXT_GAMESETUP), menu->y - 20);

    idx = 0;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__

# if __JDOOM__ || __JHERETIC__
#  if __JDOOM__
    if(gs.gameMode != commercial)
#  endif
    {
        sprintf(buf, "%i", gs.netEpisode);
        M_WriteMenuText(menu, idx++, buf);
    }
# endif
    sprintf(buf, "%i", gs.netMap);
    M_WriteMenuText(menu, idx++, buf);
    M_WriteMenuText(menu, idx++, skillText[gs.netSkill]);
    M_WriteMenuText(menu, idx++, dmText[GAMERULES.deathmatch]);
    M_WriteMenuText(menu, idx++, boolText[!GAMERULES.noMonsters]);
    M_WriteMenuText(menu, idx++, boolText[GAMERULES.respawn]);
    M_WriteMenuText(menu, idx++, boolText[GAMERULES.jumpAllow]);

# if __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, boolText[GAMERULES.freeAimBFG]);
# endif // __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, boolText[GAMERULES.noCoopDamage]);
# if __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, boolText[GAMERULES.noCoopWeapons]);
    M_WriteMenuText(menu, idx++, boolText[GAMERULES.noCoopAnything]);
    M_WriteMenuText(menu, idx++, boolText[GAMERULES.coopRespawnItems]);
    M_WriteMenuText(menu, idx++, boolText[GAMERULES.noBFG]);
# endif // __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, boolText[GAMERULES.noTeamDamage]);
#elif __JHEXEN__ || __JSTRIFE__

    sprintf(buf, "%i", gs.netMap);
    M_WriteMenuText(menu, idx++, buf);
    M_WriteText2(160 - M_StringWidth(mapName, huFontA) / 2,
                 menu->y + menu->itemHeight, mapName,
                 huFontA, 1, 0.7f, 0.3f, Hu_MenuAlpha());

    idx++;
    M_WriteMenuText(menu, idx++, skillText[gs.netSkill]);
    M_WriteMenuText(menu, idx++, dmText[GAMERULES.deathmatch]);
    M_WriteMenuText(menu, idx++, boolText[!GAMERULES.noMonsters]);
    M_WriteMenuText(menu, idx++, boolText[GAMERULES.randomClass]);
#endif                          // __JHEXEN__

    M_WriteMenuText(menu, idx++, boolText[GAMERULES.noMaxZRadiusAttack]);
    sprintf(buf, "%i", GAMERULES.mobDamageModifier);
    M_WriteMenuText(menu, idx++, buf);
    sprintf(buf, "%i", GAMERULES.mobHealthModifier);
    M_WriteMenuText(menu, idx++, buf);

    if(GAMERULES.gravityModifier != -1)
        sprintf(buf, "%i", GAMERULES.gravityModifier);
    else
        sprintf(buf, "MAP DEFAULT");
    M_WriteMenuText(menu, idx++, buf);
}

void SCEnterMultiplayerMenu(int option, void* data)
{
    int                 count;

    // Choose the correct items for the Game Setup menu.
#if __JDOOM__
    if(gs.gameMode == commercial)
    {
        GameSetupMenu.items = GameSetupItems2;
        GameSetupMenu.itemCount = GameSetupMenu.numVisItems =
            NUM_GAMESETUP_ITEMS - 1;
    }
    else
#endif
    {
        GameSetupMenu.items = GameSetupItems1;
        GameSetupMenu.itemCount = GameSetupMenu.numVisItems =
            NUM_GAMESETUP_ITEMS;
    }

    // Show the appropriate menu.
    if(IS_NETGAME)
    {
        MultiplayerMenu.items =
            IS_SERVER ? MultiplayerServerItems : MultiplayerClientItems;
        count = IS_SERVER ? 2 : 1;
    }
    else
    {
        MultiplayerMenu.items = MultiplayerItems;
        count = 2;
    }
    MultiplayerMenu.itemCount = MultiplayerMenu.numVisItems = count;

    MultiplayerMenu.lastOn = 0;

    M_SetupNextMenu(&MultiplayerMenu);
}

void SCEnterHostMenu(int option, void* data)
{
    SCEnterGameSetup(0, NULL);
}

void SCEnterJoinMenu(int option, void* data)
{
    if(IS_NETGAME)
    {
        DD_Execute(false, "net disconnect");
        Hu_MenuCommand(MCMD_CLOSE);
        return;
    }

    DD_Execute(false, "net setup client");
}

void SCEnterGameSetup(int option, void* data)
{
    // See to it that the episode and map numbers are correct.
#if __JDOOM64__
    if(gs.netMap < 1)
        gs.netMap = 1;
    if(gs.netMap > 32)
        gs.netMap = 32;
#elif __JDOOM__
    if(gs.gameMode == commercial)
    {
        gs.netEpisode = 1;
    }
    else if(gs.gameMode == retail)
    {
        if(gs.netEpisode > 4)
            gs.netEpisode = 4;
        if(gs.netMap > 9)
            gs.netMap = 9;
    }
    else if(gs.gameMode == registered)
    {
        if(gs.netEpisode > 3)
            gs.netEpisode = 3;
        if(gs.netMap > 9)
            gs.netMap = 9;
    }
    else if(gs.gameMode == shareware)
    {
        gs.netEpisode = 1;
        if(gs.netMap > 9)
            gs.netMap = 9;
    }
#elif __JHERETIC__
    if(gs.netMap > 9)
        gs.netMap = 9;
    if(gs.netEpisode > 6)
        gs.netEpisode = 6;
    if(gs.netEpisode == 6 && gs.netMap > 3)
        gs.netMap = 3;
#elif __JHEXEN__
    if(gs.netMap < 1)
        gs.netMap = 1;
    if(gs.netMap > 31)
        gs.netMap = 31;
#endif
    M_SetupNextMenu(&GameSetupMenu);
}

void SCGameSetupFunc(int option, void* data)
{
    byte*               ptr = data;

    *ptr ^= 1;
}

void SCGameSetupDeathmatch(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
#if __JDOOM__ || __JDOOM64__
        if(GAMERULES.deathmatch < 2)
#else
        if(GAMERULES.deathmatch < 1)
#endif
            GAMERULES.deathmatch++;
    }
    else if(GAMERULES.deathmatch > 0)
    {
        GAMERULES.deathmatch--;
    }
}

#if __JDOOM__ || __JHERETIC__
void SCGameSetupEpisode(int option, void* data)
{
# if __JDOOM__
    if(gs.gameMode == shareware)
    {
        gs.netEpisode = 1;
        return;
    }

    if(option == RIGHT_DIR)
    {
        if(gs.netEpisode < (gs.gameMode == retail ? 4 : 3))
            gs.netEpisode++;
    }
    else if(gs.netEpisode > 1)
    {
        gs.netEpisode--;
    }
# elif __JHERETIC__
    if(shareware)
    {
        gs.netEpisode = 1;
        return;
    }

    if(option == RIGHT_DIR)
    {
        if(gs.netEpisode < (gs.gameMode == extended? 6 : 3))
            gs.netEpisode++;
    }
    else if(gs.netEpisode > 1)
    {
        gs.netEpisode--;
    }
# endif
}
#endif

void SCGameSetupMap(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
#if __JDOOM64__
        if(gs.netMap < 32)
            gs.netMap++;
#elif __JDOOM__
        if(gs.netMap < (gs.gameMode == commercial ? 32 : 9))
            gs.netMap++;
#elif __JHERETIC__
        if(gs.netMap < (gs.netEpisode == 6? 3 : 9))
            gs.netMap++;
#elif __JHEXEN__
        if(gs.netMap < 31)
            gs.netMap++;
#endif
    }
    else if(gs.netMap > 1)
    {
        gs.netMap--;
    }
}

void SCGameSetupSkill(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
        if(gs.netSkill < NUM_SKILL_MODES - 1)
            gs.netSkill++;
    }
    else if(gs.netSkill > 0)
    {
        gs.netSkill--;
    }
}

void SCOpenServer(int option, void* data)
{
    if(IS_NETGAME)
    {
        // Game already running, just change map.
#if __JHEXEN__
        Executef(false, "setmap %i", gs.netMap);
#elif __JDOOM64__
        Executef(false, "setmap 1 %i", gs.netMap);
#else
        Executef(false, "setmap %i %i", gs.netEpisode, gs.netMap);
#endif

        Hu_MenuCommand(MCMD_CLOSE);
        return;
    }

    // Go to DMI to setup server.
    DD_Execute(false, "net setup server");
}

void SCCloseServer(int option, void* data)
{
    DD_Execute(false, "net server close");
    Hu_MenuCommand(MCMD_CLOSE);
}

void SCGameSetupDamageMod(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
        if(GAMERULES.mobDamageModifier < 100)
            GAMERULES.mobDamageModifier++;
    }
    else if(GAMERULES.mobDamageModifier > 1)
        GAMERULES.mobDamageModifier--;
}

void SCGameSetupHealthMod(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
        if(GAMERULES.mobHealthModifier < 20)
            GAMERULES.mobHealthModifier++;
    }
    else if(GAMERULES.mobHealthModifier > 1)
        GAMERULES.mobHealthModifier--;
}

void SCGameSetupGravity(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
        if(GAMERULES.gravityModifier < 100)
            GAMERULES.gravityModifier++;
    }
    else if(GAMERULES.gravityModifier > -1) // -1 = map default, 0 = zero gravity.
        GAMERULES.gravityModifier--;
}
