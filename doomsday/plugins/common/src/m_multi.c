/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

/*
 * Multiplayer Menu (for jDoom, jHeretic and jHexen)
 * Contains an extension for edit fields.
 * TODO: Remove unnecessary SC* declarations and other unused code.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "hu_stuff.h"

// MACROS ------------------------------------------------------------------

#define SLOT_WIDTH          200

#define IS_SERVER           Get(DD_SERVER)
#define IS_NETGAME          Get(DD_NETGAME)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    DrawMultiplayerMenu(void);
void    DrawGameSetupMenu(void);
void    DrawPlayerSetupMenu(void);

void    SCEnterHostMenu(int option, void *data);
void    SCEnterJoinMenu(int option, void *data);
void    SCEnterGameSetup(int option, void *data);
void    SCSetProtocol(int option, void *data);
void    SCGameSetupFunc(int option, void *data);
void    SCGameSetupEpisode(int option, void *data);
void    SCGameSetupMission(int option, void *data);
void    SCGameSetupSkill(int option, void *data);
void    SCGameSetupDeathmatch(int option, void *data);
void    SCGameSetupDamageMod(int option, void *data);
void    SCGameSetupHealthMod(int option, void *data);
void    SCGameSetupGravity(int option, void *data);
void    SCOpenServer(int option, void *data);
void    SCCloseServer(int option, void *data);
void    SCChooseHost(int option, void *data);
void    SCStartStopDisconnect(int option, void *data);
void    SCEnterPlayerSetupMenu(int option, void *data);
void    SCPlayerClass(int option, void *data);
void    SCPlayerColor(int option, void *data);
void    SCAcceptPlayer(int option, void *data);

void    ResetJoinMenuItems();

// Edit fields.
void    DrawEditField(menu_t * menu, int index, editfield_t * ef);
void    SCEditField(int efptr, void *data);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float menu_alpha;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

editfield_t *ActiveEdit = NULL; // No active edit field by default.
editfield_t plrNameEd;
int     CurrentPlrFrame = 0;

menuitem_t MultiplayerItems[] = {
    {ITT_EFUNC, 0, "player setup", SCEnterPlayerSetupMenu, 0 },
    {ITT_EFUNC, 0, "join game", SCEnterJoinMenu, 0 },
    {ITT_EFUNC, 0, "host game", SCEnterHostMenu, 0 },
};

menuitem_t MultiplayerServerItems[] = {
    {ITT_EFUNC, 0, "player setup", SCEnterPlayerSetupMenu, 0 },
    {ITT_EFUNC, 0, "game setup", SCEnterHostMenu, 0 },
    {ITT_EFUNC, 0, "close server", SCCloseServer, 0 }
};

menuitem_t MultiplayerClientItems[] = {
    {ITT_EFUNC, 0, "player setup", SCEnterPlayerSetupMenu, 0 },
    {ITT_EFUNC, 0, "disconnect", SCEnterJoinMenu, 0 }
};

menu_t  MultiplayerMenu = {
    0,
    116, 70,
    DrawMultiplayerMenu,
    3, MultiplayerItems,
    0, MENU_MAIN,
    hu_font_a,cfg.menuColor2,
    NULL,
    LINEHEIGHT_A,
    0, 3
};

#if __JHEXEN__ || __JSTRIFE__

#  define NUM_GAMESETUP_ITEMS       11

menuitem_t GameSetupItems1[] = {
    {ITT_LRFUNC, 0, "MAP:", SCGameSetupMission, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_LRFUNC, 0, "SKILL:", SCGameSetupSkill, 0},
    {ITT_EFUNC, 0, "DEATHMATCH:", SCGameSetupFunc, 0, NULL, &cfg.netDeathmatch },
    {ITT_EFUNC, 0, "MONSTERS:", SCGameSetupFunc, 0, NULL, &cfg.netNomonsters },
    {ITT_EFUNC, 0, "RANDOM CLASSES:", SCGameSetupFunc, 0, NULL, &cfg.netRandomclass },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &cfg.netNoMaxZRadiusAttack },
    {ITT_LRFUNC, 0, "DAMAGE MOD:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MOD:", SCGameSetupHealthMod, 0},
    {ITT_LRFUNC, 0, "GRAVITY MOD:", SCGameSetupGravity, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0}
};

#else

#  if __JHERETIC__

#    define NUM_GAMESETUP_ITEMS     12

menuitem_t GameSetupItems1[] =  // for Heretic
{
    {ITT_LRFUNC, 0, "EPISODE :", SCGameSetupEpisode, 0},
    {ITT_LRFUNC, 0, "MISSION :", SCGameSetupMission, 0},
    {ITT_LRFUNC, 0, "SKILL :", SCGameSetupSkill, 0},
    {ITT_LRFUNC, 0, "DEATHMATCH :", SCGameSetupDeathmatch, 0},
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netNomonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netRespawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &cfg.netJumping },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &cfg.netNoMaxZRadiusAttack },
    {ITT_LRFUNC, 0, "DAMAGE MOD:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MOD:", SCGameSetupHealthMod, 0},
    {ITT_LRFUNC, 0, "GRAVITY MOD:", SCGameSetupGravity, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0 }
};

#  elif __JDOOM__

#    define NUM_GAMESETUP_ITEMS     18

menuitem_t GameSetupItems1[] =  // for Doom 1
{
    {ITT_LRFUNC, 0, "EPISODE :", SCGameSetupEpisode, 0},
    {ITT_LRFUNC, 0, "MISSION :", SCGameSetupMission, 0},
    {ITT_LRFUNC, 0, "SKILL :", SCGameSetupSkill, 0},
    {ITT_LRFUNC, 0, "MODE :", SCGameSetupDeathmatch, 0},
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netNomonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netRespawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &cfg.netJumping },
    {ITT_EFUNC, 0, "ALLOW BFG AIMING :", SCGameSetupFunc, 0, NULL, &cfg.netBFGFreeLook},
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noCoopDamage },
    {ITT_EFUNC, 0, "NO COOP WEAPONS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopWeapons },
    {ITT_EFUNC, 0, "NO COOP OBJECTS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopAnything },
    {ITT_EFUNC, 0, "NO BFG 9000 :", SCGameSetupFunc, 0, NULL, &cfg.noNetBFG },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noTeamDamage },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &cfg.netNoMaxZRadiusAttack },
    {ITT_LRFUNC, 0, "DAMAGE MOD:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MOD:", SCGameSetupHealthMod, 0},
    {ITT_LRFUNC, 0, "GRAVITY MOD:", SCGameSetupGravity, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0 }
};

menuitem_t GameSetupItems2[] =  // for Doom 2
{
    {ITT_LRFUNC, 0, "LEVEL :", SCGameSetupMission, 0},
    {ITT_LRFUNC, 0, "SKILL :", SCGameSetupSkill, 0},
    {ITT_LRFUNC, 0, "MODE :", SCGameSetupDeathmatch, 0},
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL,  &cfg.netNomonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netRespawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &cfg.netJumping },
    {ITT_EFUNC, 0, "ALLOW BFG AIMING :", SCGameSetupFunc, 0, NULL, &cfg.netBFGFreeLook},
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noCoopDamage },
    {ITT_EFUNC, 0, "NO COOP WEAPONS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopWeapons },
    {ITT_EFUNC, 0, "NO COOP OBJECTS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopAnything },
    {ITT_EFUNC, 0, "NO BFG 9000 :", SCGameSetupFunc, 0, NULL, &cfg.noNetBFG },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noTeamDamage },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &cfg.netNoMaxZRadiusAttack },
    {ITT_LRFUNC, 0, "DAMAGE MOD:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MOD:", SCGameSetupHealthMod, 0},
    {ITT_LRFUNC, 0, "GRAVITY MOD:", SCGameSetupGravity, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0 }
};

#  endif

#endif

menu_t  GameSetupMenu = {
    0,
#  if __JDOOM__
    90, 54,
#  elif __JHERETIC__
    74, 64,
#  elif __JHEXEN__  || __JSTRIFE__
    90, 64,
#  endif
    DrawGameSetupMenu,
    NUM_GAMESETUP_ITEMS, GameSetupItems1,
    0, MENU_MULTIPLAYER,
    hu_font_a,                  //1, 0, 0,
    cfg.menuColor2,
    NULL,
    LINEHEIGHT_A,
    0, NUM_GAMESETUP_ITEMS
};



#if __JHEXEN__ || __JSTRIFE__
#  define NUM_PLAYERSETUP_ITEMS 6
#else
#  define NUM_PLAYERSETUP_ITEMS 6
#endif

menuitem_t PlayerSetupItems[] = {
    {ITT_EFUNC, 0, "", SCEditField, 0, NULL, &plrNameEd },
    {ITT_EMPTY, 0, NULL, NULL, 0},
#if __JHEXEN__
    {ITT_LRFUNC, 0, "Class:", SCPlayerClass, 0},
#else
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "Color:", SCPlayerColor, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EFUNC, 0, "Accept Changes", SCAcceptPlayer, 0 }
};

menu_t  PlayerSetupMenu = {
    0,
    60, 52,
    DrawPlayerSetupMenu,
    NUM_PLAYERSETUP_ITEMS, PlayerSetupItems,
    0, MENU_MULTIPLAYER,
    hu_font_b, cfg.menuColor, NULL, LINEHEIGHT_B,
    0, NUM_PLAYERSETUP_ITEMS
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int plrColor;

#if __JHEXEN__
static int plrClass;
#endif

// CODE --------------------------------------------------------------------

int Executef(int silent, char *format, ...)
{
    va_list argptr;
    char    buffer[512];

    va_start(argptr, format);
    vsnprintf(buffer, sizeof(buffer), format, argptr);
    va_end(argptr);
    return DD_Execute(buffer, silent);
}

void DrANumber(int number, int x, int y)
{
    char    buff[40];

    sprintf(buff, "%i", number);

    M_WriteText2(x, y, buff, hu_font_a, 1, 1, 1, menu_alpha);
}

void MN_DrCenterTextA_CS(char *text, int center_x, int y)
{
    M_WriteText2(center_x - M_StringWidth(text, hu_font_a) / 2, y, text,
                 hu_font_a, 1, 0, 0, menu_alpha);
}

void MN_DrCenterTextB_CS(char *text, int center_x, int y)
{
    M_WriteText2(center_x - M_StringWidth(text, hu_font_b) / 2, y, text,
                 hu_font_b, 1, 0, 0, menu_alpha);
}

void DrawMultiplayerMenu(void)
{
    M_DrawTitle("MULTIPLAYER", MultiplayerMenu.y - 30);
}

void DrawGameSetupMenu(void)
{
    char   *boolText[2] = { "NO", "YES" }, buf[50];
    char   *skillText[5] = { "BABY", "EASY", "MEDIUM", "HARD", "NIGHTMARE" };
#if __JDOOM__
    //char   *freeLookText[3] = { "NO", "NOT BFG", "ALL" };
    char   *dmText[3] = { "COOPERATIVE", "DEATHMATCH 1", "DEATHMATCH 2" };
#else
    char   *dmText[3] = { "NO", "YES", "YES" };
#endif
    menu_t *menu = &GameSetupMenu;
    int     idx;

#if __JHEXEN__
    char   *mapName = P_GetMapName(P_TranslateMap(cfg.netMap));
#elif __JSTRIFE__
    char   *mapName = "unnamed";
#endif

    M_DrawTitle("GAME SETUP", menu->y - 20);

    idx = 0;

#if __JDOOM__ || __JHERETIC__

# if __JDOOM__
#  if !__DOOM64TC__
    if(gamemode != commercial)
#  endif
# endif
    {
        sprintf(buf, "%i", cfg.netEpisode);
        M_WriteMenuText(menu, idx++, buf);
    }
    sprintf(buf, "%i", cfg.netMap);
    M_WriteMenuText(menu, idx++, buf);
    M_WriteMenuText(menu, idx++, skillText[cfg.netSkill]);
    M_WriteMenuText(menu, idx++, dmText[cfg.netDeathmatch]);
    M_WriteMenuText(menu, idx++, boolText[!cfg.netNomonsters]);
    M_WriteMenuText(menu, idx++, boolText[cfg.netRespawn]);
    M_WriteMenuText(menu, idx++, boolText[cfg.netJumping]);

# if __JDOOM__
    M_WriteMenuText(menu, idx++, boolText[cfg.netBFGFreeLook]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noCoopDamage]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noCoopWeapons]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noCoopAnything]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noNetBFG]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noTeamDamage]);
# endif                        // __JDOOM__
#elif __JHEXEN__ || __JSTRIFE__

    sprintf(buf, "%i", cfg.netMap);
    M_WriteMenuText(menu, idx++, buf);
    M_WriteText2(160 - M_StringWidth(mapName, hu_font_a) / 2,
                 menu->y + menu->itemHeight, mapName,
                 hu_font_a, 1, 0.7f, 0.3f, menu_alpha);

    idx++;
    M_WriteMenuText(menu, idx++, skillText[cfg.netSkill]);
    M_WriteMenuText(menu, idx++, dmText[cfg.netDeathmatch]);
    M_WriteMenuText(menu, idx++, boolText[!cfg.netNomonsters]);
    M_WriteMenuText(menu, idx++, boolText[cfg.netRandomclass]);
#endif                          // __JHEXEN__

    M_WriteMenuText(menu, idx++, boolText[cfg.netNoMaxZRadiusAttack]);
    sprintf(buf, "%i", cfg.netMobDamageModifier);
    M_WriteMenuText(menu, idx++, buf);
    sprintf(buf, "%i", cfg.netMobHealthModifier);
    M_WriteMenuText(menu, idx++, buf);

    if(cfg.netGravity != -1)
        sprintf(buf, "%i", cfg.netGravity);
    else
        sprintf(buf, "MAP DEFAULT");
    M_WriteMenuText(menu, idx++, buf);
}

/*
 * Finds the power of 2 that is equal to or greater than
 * the specified number.
 */
static int CeilPow2(int num)
{
    int     cumul;

    for(cumul = 1; num > cumul; cumul <<= 1);
    return cumul;
}

void DrawPlayerSetupMenu(void)
{
    spriteinfo_t sprInfo;
    menu_t *menu = &PlayerSetupMenu;
    int     useColor = plrColor;

#if __JHEXEN__
    int     numColors = 8;
    int     sprites[3] = { SPR_PLAY, SPR_CLER, SPR_MAGE };
#else
    int     plrClass = 0;
    int     numColors = 4;
    int     sprites[3] = { SPR_PLAY, SPR_PLAY, SPR_PLAY };
#endif

    M_DrawTitle("PLAYER SETUP", menu->y - 28);

    DrawEditField(menu, 0, &plrNameEd);

    if(useColor == numColors)
        useColor = menuTime / 5 % numColors;

    // Draw the color selection as a random player frame.
    //#ifndef __JHEXEN__
    //  gi.GetSpriteInfo(SPR_PLAY, CurrentPlrFrame, &sprInfo);
    //#else

    R_GetSpriteInfo(sprites[plrClass], CurrentPlrFrame, &sprInfo);

#if __JHEXEN__
    if(plrClass == PCLASS_FIGHTER)
    {
        // Fighter's colors are a bit different.
        if(useColor == 0)
            useColor = 2;
        else if(useColor == 2)
            useColor = 0;
    }
#endif
    Set(DD_TRANSLATED_SPRITE_TEXTURE,
        DD_TSPR_PARM(sprInfo.lump, plrClass, useColor));

    GL_DrawRect(162 - sprInfo.offset,
#ifdef __JDOOM__
                menu->y + 70 - sprInfo.topOffset,
#elif defined __JHERETIC__
                menu->y + 80 - sprInfo.topOffset,
#else
                menu->y + 90 - sprInfo.topOffset,
#endif
                CeilPow2(sprInfo.width), CeilPow2(sprInfo.height), 1, 1, 1,
                menu_alpha);

    if(plrColor == numColors)
    {
        M_WriteText2(184,
#ifdef __JDOOM__
                      menu->y + 49,
#elif defined __JHERETIC__
                      menu->y + 65,
#else
                      menu->y + 64,
#endif
                      "AUTOMATIC", hu_font_a, 1, 1, 1, menu_alpha);
    }

}

void SCEnterMultiplayerMenu(int option, void *data)
{
    int     count;

    // Choose the correct items for the Game Setup menu.
#ifdef __JDOOM__
# if !__DOOM64TC__
    if(gamemode == commercial)
    {
        GameSetupMenu.items = GameSetupItems2;
        GameSetupMenu.itemCount = GameSetupMenu.numVisItems =
            NUM_GAMESETUP_ITEMS - 1;
    }
    else
# endif
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
        count = IS_SERVER ? 3 : 2;
    }
    else
    {
        MultiplayerMenu.items = MultiplayerItems;
        count = 3;
    }
    MultiplayerMenu.itemCount = MultiplayerMenu.numVisItems = count;

    MultiplayerMenu.lastOn = 0;

    M_SetupNextMenu(&MultiplayerMenu);
}

void SCEnterHostMenu(int option, void *data)
{
    SCEnterGameSetup(0, NULL);
}

void SCEnterJoinMenu(int option, void *data)
{
    if(IS_NETGAME)
    {
        DD_Execute("net disconnect", false);
        M_ClearMenus();
        return;
    }
    DD_Execute("net setup client", false);
}

void SCEnterGameSetup(int option, void *data)
{
    // See to it that the episode and mission numbers are correct.
#if __DOOM64TC__
    if(cfg.netEpisode > 2)
        cfg.netEpisode = 2;
    if(cfg.netEpisode == 1)
    {
        if(cfg.netMap > 39)
            cfg.netMap = 39;
    }
    else
    {
        if(cfg.netMap > 7)
            cfg.netMap = 7;
    }
#elif __JDOOM__
    if(gamemode == commercial)
    {
        cfg.netEpisode = 1;
    }
    else if(gamemode == retail)
    {
        if(cfg.netEpisode > 4)
            cfg.netEpisode = 4;
        if(cfg.netMap > 9)
            cfg.netMap = 9;
    }
    else if(gamemode == registered)
    {
        if(cfg.netEpisode > 3)
            cfg.netEpisode = 3;
        if(cfg.netMap > 9)
            cfg.netMap = 9;
    }
    else if(gamemode == shareware)
    {
        cfg.netEpisode = 1;
        if(cfg.netMap > 9)
            cfg.netMap = 9;
    }
#elif defined __JHERETIC__
    if(cfg.netMap > 9)
        cfg.netMap = 9;
    if(cfg.netEpisode > 6)
        cfg.netEpisode = 6;
    if(cfg.netEpisode == 6 && cfg.netMap > 3)
        cfg.netMap = 3;
#elif defined __JHEXEN__ || __JSTRIFE__
    if(cfg.netMap < 1)
        cfg.netMap = 1;
    if(cfg.netMap > 31)
        cfg.netMap = 31;
#endif
    M_SetupNextMenu(&GameSetupMenu);
}

void SCGameSetupFunc(int option, void *data)
{
    byte   *ptr = data;

    *ptr ^= 1;
}

void SCGameSetupDeathmatch(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
#ifdef __JDOOM__
        if(cfg.netDeathmatch < 2)
#else
        if(cfg.netDeathmatch < 1)
#endif
            cfg.netDeathmatch++;
    }
    else if(cfg.netDeathmatch > 0)
    {
        cfg.netDeathmatch--;
    }
}

void SCGameSetupEpisode(int option, void *data)
{
#if __DOOM64TC__
    if(option == RIGHT_DIR)
    {
        if(cfg.netEpisode < 2)
            cfg.netEpisode++;
    }
    else if(cfg.netEpisode > 1)
    {
        cfg.netEpisode--;
    }
#elif __JDOOM__
    if(gamemode == shareware)
    {
        cfg.netEpisode = 1;
        return;
    }
    if(option == RIGHT_DIR)
    {
        if(cfg.netEpisode < (gamemode == retail ? 4 : 3))
            cfg.netEpisode++;
    }
    else if(cfg.netEpisode > 1)
    {
        cfg.netEpisode--;
    }
#elif __JHERETIC__
    if(shareware)
    {
        cfg.netEpisode = 1;
        return;
    }
    if(option == RIGHT_DIR)
    {
        if(cfg.netEpisode < 6)
            cfg.netEpisode++;
    }
    else if(cfg.netEpisode > 1)
    {
        cfg.netEpisode--;
    }
#endif
    return;
}

void SCGameSetupMission(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
#if __DOOM64TC__
        if(cfg.netMap < (cfg.netEpisode == 1? 39 : 7))
            cfg.netMap++;
#elif __JDOOM__
        if(cfg.netMap < (gamemode == commercial ? 32 : 9))
            cfg.netMap++;
#elif __JHERETIC__
        if(cfg.netMap < 9)
            cfg.netMap++;
#elif __JHEXEN__ || __JSTRIFE__
        if(cfg.netMap < 31)
            cfg.netMap++;
#endif
    }
    else if(cfg.netMap > 1)
    {
        cfg.netMap--;
    }
    return;
}

void SCGameSetupSkill(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netSkill < 4)
            cfg.netSkill++;
    }
    else if(cfg.netSkill > 0)
    {
        cfg.netSkill--;
    }
    return;
}

void SCOpenServer(int option, void *data)
{
    if(IS_NETGAME)
    {
        // Game already running, just change level.
#if __JHEXEN__ || __JSTRIFE__
        Executef(false, "setmap %i", cfg.netMap);
#else
        Executef(false, "setmap %i %i", cfg.netEpisode, cfg.netMap);
#endif

        M_ClearMenus();
        return;
    }

    // Go to DMI to setup server.
    DD_Execute("net setup server", false);
}

void SCCloseServer(int option, void *data)
{
    DD_Execute("net server close", false);
    M_ClearMenus();
}

void SCEnterPlayerSetupMenu(int option, void *data)
{
    strncpy(plrNameEd.text, *(char **) Con_GetVariable("net-name")->ptr, 255);
    plrColor = cfg.netColor;
#if __JHEXEN__
    plrClass = cfg.netClass;
#endif
    M_SetupNextMenu(&PlayerSetupMenu);
}

#if __JHEXEN__
void SCPlayerClass(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(plrClass < 2)
            plrClass++;
    }
    else if(plrClass > 0)
        plrClass--;
}
#endif

void SCPlayerColor(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
#if __JHEXEN__
        if(plrColor < 8)
            plrColor++;
#else
        if(plrColor < 4)
            plrColor++;
#endif
    }
    else if(plrColor > 0)
        plrColor--;
}

void SCAcceptPlayer(int option, void *data)
{
    char    buf[300];

    cfg.netColor = plrColor;
#if __JHEXEN__
    cfg.netClass = plrClass;
#endif

    strcpy(buf, "net-name ");
    M_StrCatQuoted(buf, plrNameEd.text);
    DD_Execute(buf, false);

    if(IS_NETGAME)
    {
        sprintf(buf, "setname ");
        M_StrCatQuoted(buf, plrNameEd.text);
        DD_Execute(buf, false);
#if __JHEXEN__
        // Must do 'setclass' first; the real class and color do not change
        // until the server sends us a notification -- this means if we do
        // 'setcolor' first, the 'setclass' after it will override the color
        // change (or such would appear to be the case).
        Executef(false, "setclass %i", plrClass);
#endif
        Executef(false, "setcolor %i", plrColor);
    }

    M_SetupNextMenu(&MultiplayerMenu);
}

void SCGameSetupDamageMod(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netMobDamageModifier < 100)
            cfg.netMobDamageModifier++;
    }
    else if(cfg.netMobDamageModifier > 1)
        cfg.netMobDamageModifier--;
}

void SCGameSetupHealthMod(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netMobHealthModifier < 20)
            cfg.netMobHealthModifier++;
    }
    else if(cfg.netMobHealthModifier > 1)
        cfg.netMobHealthModifier--;
}

void SCGameSetupGravity(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netGravity < 100)
            cfg.netGravity++;
    }
    else if(cfg.netGravity > -1) // -1 = map default, 0 = zero gravity.
        cfg.netGravity--;
}

void MN_TickerEx(void)          // The extended ticker.
{
    static int FrameTimer = 0;

    if(currentMenu == &PlayerSetupMenu)
    {
        if(FrameTimer++ >= 14)
        {
            FrameTimer = 0;
            CurrentPlrFrame = M_Random() % 8;
        }
    }
}

int Ed_VisibleSlotChars(char *text, int (*widthFunc) (char *text, dpatch_t *font))
{
    char    cbuf[2] = { 0, 0 };
    int     i, w;

    for(i = 0, w = 0; text[i]; i++)
    {
        cbuf[0] = text[i];
        w += widthFunc(cbuf, hu_font_a);
        if(w > SLOT_WIDTH)
            break;
    }
    return i;
}

void Ed_MakeCursorVisible()
{
    char    buf[MAX_EDIT_LEN + 1];
    int     i, len, vis;

    strcpy(buf, ActiveEdit->text);
    strupr(buf);
    strcat(buf, "_");           // The cursor.
    len = strlen(buf);
    for(i = 0; i < len; i++)
    {
        vis = Ed_VisibleSlotChars(buf + i, M_StringWidth);

        if(i + vis >= len)
        {
            ActiveEdit->firstVisible = i;
            break;
        }
    }
}

void DrawEditField(menu_t * menu, int index, editfield_t * ef)
{
    int     x = menu->x, y = menu->y + menu->itemHeight * index, vis;
    char    buf[MAX_EDIT_LEN + 1], *text;


    M_DrawSaveLoadBorder(x + 11, y + 5);
    strcpy(buf, ef->text);
    strupr(buf);
    if(ActiveEdit == ef && menuTime & 0x8)
        strcat(buf, "_");
    text = buf + ef->firstVisible;
    vis = Ed_VisibleSlotChars(text, M_StringWidth);
    text[vis] = 0;
    M_WriteText2(x + 8, y + 5, text, hu_font_a, 1, 1, 1, menu_alpha);

}

void SCEditField(int efptr, void *data)
{
    editfield_t *ef = data;

    // Activate this edit field.
    ActiveEdit = ef;
    strcpy(ef->oldtext, ef->text);
    Ed_MakeCursorVisible();
}
