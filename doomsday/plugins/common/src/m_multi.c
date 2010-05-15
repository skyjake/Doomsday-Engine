/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 *
 * Contains an extension for edit fields.
 * \todo Remove unnecessary SC* declarations and other unused code.
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

#define SLOT_WIDTH          (200)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void            DrawMultiplayerMenu(void);
void            DrawGameSetupMenu(void);
void            DrawPlayerSetupMenu(void);

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
void            SCEnterPlayerSetupMenu(int option, void* data);
void            SCPlayerClass(int option, void* data);
void            SCPlayerColor(int option, void* data);
void            SCAcceptPlayer(int option, void* data);

void            ResetJoinMenuItems();

// Edit fields.
void            DrawEditField(menu_t* menu, int index, editfield_t* ef);
void            SCEditField(int efptr, void* data);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

editfield_t* ActiveEdit = NULL; // No active edit field by default.
editfield_t plrNameEd;
int CurrentPlrFrame = 0;

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

menu_t MultiplayerMenu = {
    0,
    116, 70,
    DrawMultiplayerMenu,
    3, MultiplayerItems,
    0, MENU_NEWGAME,
    GF_FONTA,cfg.menuColor2,
    LINEHEIGHT_A,
    0, 3
};

#if __JHEXEN__ || __JSTRIFE__

#  define NUM_GAMESETUP_ITEMS       11

menuitem_t GameSetupItems1[] = {
    {ITT_LRFUNC, 0, "MAP:", SCGameSetupMap, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_LRFUNC, 0, "SKILL:", SCGameSetupSkill, 0},
    {ITT_EFUNC, 0, "DEATHMATCH:", SCGameSetupFunc, 0, NULL, &cfg.netDeathmatch },
    {ITT_EFUNC, 0, "MONSTERS:", SCGameSetupFunc, 0, NULL, &cfg.netNoMonsters },
    {ITT_EFUNC, 0, "RANDOM CLASSES:", SCGameSetupFunc, 0, NULL, &cfg.netRandomClass },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &cfg.netNoMaxZRadiusAttack },
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
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netNoMonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netRespawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &cfg.netJumping },
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noCoopDamage },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noTeamDamage },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &cfg.netNoMaxZRadiusAttack },
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
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netNoMonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netRespawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &cfg.netJumping },
    {ITT_EFUNC, 0, "ALLOW BFG AIMING :", SCGameSetupFunc, 0, NULL, &cfg.netBFGFreeLook},
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noCoopDamage },
    {ITT_EFUNC, 0, "NO COOP WEAPONS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopWeapons },
    {ITT_EFUNC, 0, "NO COOP OBJECTS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopAnything },
    {ITT_EFUNC, 0, "COOP ITEMS RESPAWN :", SCGameSetupFunc, 0, NULL, &cfg.coopRespawnItems },
    {ITT_EFUNC, 0, "NO BFG 9000 :", SCGameSetupFunc, 0, NULL, &cfg.noNetBFG },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noTeamDamage },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &cfg.netNoMaxZRadiusAttack },
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
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL,  &cfg.netNoMonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netRespawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &cfg.netJumping },
    {ITT_EFUNC, 0, "ALLOW BFG AIMING :", SCGameSetupFunc, 0, NULL, &cfg.netBFGFreeLook},
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noCoopDamage },
    {ITT_EFUNC, 0, "NO COOP WEAPONS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopWeapons },
    {ITT_EFUNC, 0, "NO COOP OBJECTS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopAnything },
    {ITT_EFUNC, 0, "COOP ITEMS RESPAWN :", SCGameSetupFunc, 0, NULL, &cfg.coopRespawnItems },
    {ITT_EFUNC, 0, "NO BFG 9000 :", SCGameSetupFunc, 0, NULL, &cfg.noNetBFG },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noTeamDamage },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &cfg.netNoMaxZRadiusAttack },
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
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL,  &cfg.netNoMonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netRespawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &cfg.netJumping },
    {ITT_EFUNC, 0, "ALLOW BFG AIMING :", SCGameSetupFunc, 0, NULL, &cfg.netBFGFreeLook},
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noCoopDamage },
    {ITT_EFUNC, 0, "NO COOP WEAPONS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopWeapons },
    {ITT_EFUNC, 0, "NO COOP OBJECTS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopAnything },
    {ITT_EFUNC, 0, "COOP ITEMS RESPAWN :", SCGameSetupFunc, 0, NULL, &cfg.coopRespawnItems },
    {ITT_EFUNC, 0, "NO BFG 9000 :", SCGameSetupFunc, 0, NULL, &cfg.noNetBFG },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noTeamDamage },
    {ITT_EFUNC, 0, "NO MAX Z RADIUS ATTACKS", SCGameSetupFunc, 0, NULL, &cfg.netNoMaxZRadiusAttack },
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
    GF_FONTA,                  //1, 0, 0,
    cfg.menuColor2,
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

menu_t PlayerSetupMenu = {
    0,
    60, 52,
    DrawPlayerSetupMenu,
    NUM_PLAYERSETUP_ITEMS, PlayerSetupItems,
    0, MENU_MULTIPLAYER,
    GF_FONTB, cfg.menuColor, LINEHEIGHT_B,
    0, NUM_PLAYERSETUP_ITEMS
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int plrColor;

#if __JHEXEN__
static int plrClass;
#endif

// CODE --------------------------------------------------------------------

int Executef(int silent, char* format, ...)
{
    va_list argptr;
    char buffer[512];

    va_start(argptr, format);
    dd_vsnprintf(buffer, sizeof(buffer), format, argptr);
    va_end(argptr);
    return DD_Execute(silent, buffer);
}

void DrANumber(int number, int x, int y)
{
    char buf[40];
    sprintf(buf, "%i", number);
    M_DrawText2(buf, x, y, GF_FONTA, 1, 1, 1, Hu_MenuAlpha());
}

void MN_DrCenterTextA_CS(char* string, int centerX, int y)
{
    M_DrawText2(string, centerX - M_TextWidth(string, GF_FONTA) / 2, y, GF_FONTA, 1, 0, 0, Hu_MenuAlpha());
}

void MN_DrCenterTextB_CS(char* string, int centerX, int y)
{
    M_DrawText2(string, centerX - M_TextWidth(string, GF_FONTB) / 2, y, GF_FONTB, 1, 0, 0, Hu_MenuAlpha());
}

void DrawMultiplayerMenu(void)
{
    MN_DrawTitle(GET_TXT(TXT_MULTIPLAYER), MultiplayerMenu.y - 30);
}

void DrawGameSetupMenu(void)
{
    char*               boolText[] = {"NO", "YES"};
    char*               skillText[] = {"BABY", "EASY", "MEDIUM", "HARD", "NIGHTMARE"};
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
    char*               mapName = P_GetMapName(P_TranslateMap(cfg.netMap));
#elif __JSTRIFE__
    char*               mapName = "unnamed";
#endif

    MN_DrawTitle(GET_TXT(TXT_GAMESETUP), menu->y - 20);

    idx = 0;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__

# if __JDOOM__ || __JHERETIC__
#  if __JDOOM__
    if(gameMode != commercial)
#  endif
    {
        sprintf(buf, "%u", cfg.netEpisode+1);
        M_WriteMenuText(menu, idx++, buf);
    }
# endif
    sprintf(buf, "%u", cfg.netMap+1);
    M_WriteMenuText(menu, idx++, buf);
    M_WriteMenuText(menu, idx++, skillText[cfg.netSkill]);
    M_WriteMenuText(menu, idx++, dmText[cfg.netDeathmatch]);
    M_WriteMenuText(menu, idx++, boolText[!cfg.netNoMonsters]);
    M_WriteMenuText(menu, idx++, boolText[cfg.netRespawn]);
    M_WriteMenuText(menu, idx++, boolText[cfg.netJumping]);

# if __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, boolText[cfg.netBFGFreeLook]);
# endif // __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, boolText[cfg.noCoopDamage]);
# if __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, boolText[cfg.noCoopWeapons]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noCoopAnything]);
    M_WriteMenuText(menu, idx++, boolText[cfg.coopRespawnItems]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noNetBFG]);
# endif // __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, boolText[cfg.noTeamDamage]);
#elif __JHEXEN__ || __JSTRIFE__

    sprintf(buf, "%u", cfg.netMap+1);
    M_WriteMenuText(menu, idx++, buf);
    M_DrawText2(mapName, 160 - M_TextWidth(mapName, GF_FONTA) / 2, menu->y + menu->itemHeight, GF_FONTA, 1, 0.7f, 0.3f, Hu_MenuAlpha());

    idx++;
    M_WriteMenuText(menu, idx++, skillText[cfg.netSkill]);
    M_WriteMenuText(menu, idx++, dmText[cfg.netDeathmatch]);
    M_WriteMenuText(menu, idx++, boolText[!cfg.netNoMonsters]);
    M_WriteMenuText(menu, idx++, boolText[cfg.netRandomClass]);
#endif // __JHEXEN__

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

void DrawPlayerSetupMenu(void)
{
#define AVAILABLE_WIDTH     38
#define AVAILABLE_HEIGHT    52

    spriteinfo_t        sprInfo;
    menu_t*             menu = &PlayerSetupMenu;
    int                 useColor = plrColor;
    float               menuAlpha = Hu_MenuAlpha();
    int                 tclass = 0;
#if __JHEXEN__
    int                 numColors = 8;
    static const int sprites[3] = {SPR_PLAY, SPR_CLER, SPR_MAGE};
#else
    int                 plrClass = 0;
    int                 numColors = 4;
    static const int    sprites[3] = {SPR_PLAY, SPR_PLAY, SPR_PLAY};
#endif
    float               x, y, w, h;
    float               s, t, scale;

    MN_DrawTitle(GET_TXT(TXT_PLAYERSETUP), menu->y - 28);

    DrawEditField(menu, 0, &plrNameEd);

    if(useColor == numColors)
        useColor = menuTime / 5 % numColors;
#if __JHEXEN__
    R_GetTranslation(plrClass, useColor, &tclass, &useColor);
#endif

    // Draw the color selection as a random player frame.
    R_GetSpriteInfo(sprites[plrClass], CurrentPlrFrame, &sprInfo);

    x = 162;
#if __JDOOM__ || __JDOOM64__
    y = menu->y + 70;
#elif __JHERETIC__
    y = menu->y + 80;
#else
    y = menu->y + 90;
#endif

    w = sprInfo.width;
    h = sprInfo.height;

    s = sprInfo.texCoord[0];
    t = sprInfo.texCoord[1];

    if(h > w)
        scale = AVAILABLE_HEIGHT / h;
    else
        scale = AVAILABLE_WIDTH / w;

    w *= scale;
    h *= scale;

    x -= sprInfo.width / 2 * scale;
    y -= sprInfo.height * scale;

    DGL_SetTranslatedSprite(sprInfo.material, tclass, useColor);

    DGL_Color4f(1, 1, 1, menuAlpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0 * s, 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, 1 * s, 0);
        DGL_Vertex2f(x + w, y);

        DGL_TexCoord2f(0, 1 * s, t);
        DGL_Vertex2f(x + w, y + h);

        DGL_TexCoord2f(0, 0 * s, t);
        DGL_Vertex2f(x, y + h);
    DGL_End();

    if(plrColor == numColors)
    {
        M_DrawText2("AUTOMATIC", 184,
#if __JDOOM__ || __JDOOM64__
                      menu->y + 49,
#elif __JHERETIC__
                      menu->y + 65,
#else
                      menu->y + 64,
#endif
                      GF_FONTA, 1, 1, 1, menuAlpha);
    }

#undef AVAILABLE_WIDTH
#undef AVAILABLE_HEIGHT
}

void SCEnterMultiplayerMenu(int option, void* data)
{
    int                 count;

    // Choose the correct items for the Game Setup menu.
#if __JDOOM__
    if(gameMode == commercial)
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
    if(cfg.netMap > 31)
        cfg.netMap = 31;
#elif __JDOOM__
    if(gameMode == commercial)
    {
        cfg.netEpisode = 0;
    }
    else if(gameMode == retail)
    {
        if(cfg.netEpisode > 3)
            cfg.netEpisode = 3;
        if(cfg.netMap > 8)
            cfg.netMap = 8;
    }
    else if(gameMode == registered)
    {
        if(cfg.netEpisode > 2)
            cfg.netEpisode = 2;
        if(cfg.netMap > 8)
            cfg.netMap = 8;
    }
    else if(gameMode == shareware)
    {
        cfg.netEpisode = 0;
        if(cfg.netMap > 8)
            cfg.netMap = 8;
    }
#elif __JHERETIC__
    if(cfg.netMap > 8)
        cfg.netMap = 8;
    if(cfg.netEpisode > 5)
        cfg.netEpisode = 5;
    if(cfg.netEpisode == 5 && cfg.netMap > 2)
        cfg.netMap = 2;
#elif __JHEXEN__ || __JSTRIFE__
    if(cfg.netMap > 30)
        cfg.netMap = 30;
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

#if __JDOOM__ || __JHERETIC__
void SCGameSetupEpisode(int option, void* data)
{
# if __JDOOM__
    if(gameMode == shareware)
    {
        cfg.netEpisode = 0;
        return;
    }

    if(option == RIGHT_DIR)
    {
        if(cfg.netEpisode < (gameMode == retail ? 3 : 2))
            cfg.netEpisode++;
    }
    else if(cfg.netEpisode != 0)
    {
        cfg.netEpisode--;
    }
# elif __JHERETIC__
    if(shareware)
    {
        cfg.netEpisode = 0;
        return;
    }

    if(option == RIGHT_DIR)
    {
        if(cfg.netEpisode < (gameMode == extended? 5 : 2))
            cfg.netEpisode++;
    }
    else if(cfg.netEpisode != 0)
    {
        cfg.netEpisode--;
    }
    if(cfg.netMap > (cfg.netEpisode == 5? 2 : 8))
        cfg.netMap = (cfg.netEpisode == 5? 2 : 8);
# endif
}
#endif

void SCGameSetupMap(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
#if __JDOOM64__
        if(cfg.netMap < 31)
            cfg.netMap++;
#elif __JDOOM__
        if(cfg.netMap < (gameMode == commercial ? 31 : 8))
            cfg.netMap++;
#elif __JHERETIC__
        if(cfg.netMap < (cfg.netEpisode == 5? 2 : 8))
            cfg.netMap++;
#elif __JHEXEN__ || __JSTRIFE__
        if(cfg.netMap < 30)
            cfg.netMap++;
#endif
    }
    else if(cfg.netMap != 0)
    {
        cfg.netMap--;
    }
}

void SCGameSetupSkill(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netSkill < NUM_SKILL_MODES - 1)
            cfg.netSkill++;
    }
    else if(cfg.netSkill > 0)
    {
        cfg.netSkill--;
    }
}

void SCOpenServer(int option, void* data)
{
    if(IS_NETGAME)
    {
        // Game already running, just change map.
#if __JHEXEN__ || __JSTRIFE__
        Executef(false, "setmap %u", cfg.netMap+1);
#elif __JDOOM64__
        Executef(false, "setmap 1 %u", cfg.netMap+1);
#else
        Executef(false, "setmap %u %u", cfg.netEpisode+1, cfg.netMap+1);
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

void SCEnterPlayerSetupMenu(int option, void* data)
{
    strncpy(plrNameEd.text, *(char **) Con_GetVariable("net-name")->ptr, 255);
    plrColor = cfg.netColor;
#if __JHEXEN__
    plrClass = cfg.netClass;
#endif
    M_SetupNextMenu(&PlayerSetupMenu);
}

#if __JHEXEN__
void SCPlayerClass(int option, void* data)
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

void SCPlayerColor(int option, void* data)
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

void SCAcceptPlayer(int option, void* data)
{
    char                buf[300];

    cfg.netColor = plrColor;
#if __JHEXEN__
    cfg.netClass = plrClass;
#endif

    strcpy(buf, "net-name ");
    M_StrCatQuoted(buf, plrNameEd.text, 300);
    DD_Execute(false, buf);

    if(IS_NETGAME)
    {
        sprintf(buf, "setname ");
        M_StrCatQuoted(buf, plrNameEd.text, 300);
        DD_Execute(false, buf);
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

void SCGameSetupDamageMod(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netMobDamageModifier < 100)
            cfg.netMobDamageModifier++;
    }
    else if(cfg.netMobDamageModifier > 1)
        cfg.netMobDamageModifier--;
}

void SCGameSetupHealthMod(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netMobHealthModifier < 20)
            cfg.netMobHealthModifier++;
    }
    else if(cfg.netMobHealthModifier > 1)
        cfg.netMobHealthModifier--;
}

void SCGameSetupGravity(int option, void* data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netGravity < 100)
            cfg.netGravity++;
    }
    else if(cfg.netGravity > -1) // -1 = map default, 0 = zero gravity.
        cfg.netGravity--;
}

/**
 * The extended ticker.
 */
void MN_TickerEx(void)
{
    static int          frameTimer = 0;

    if(currentMenu == &PlayerSetupMenu)
    {
        if(frameTimer++ >= 14)
        {
            frameTimer = 0;
            CurrentPlrFrame = M_Random() % 8;
        }
    }
}

int Ed_VisibleSlotChars(char* text, int (*widthFunc) (const char* text, gamefontid_t font))
{
    char                cbuf[2] = { 0, 0 };
    int                 i, w;

    for(i = 0, w = 0; text[i]; ++i)
    {
        cbuf[0] = text[i];
        w += widthFunc(cbuf, GF_FONTA);
        if(w > SLOT_WIDTH)
            break;
    }
    return i;
}

void Ed_MakeCursorVisible(void)
{
    char                buf[MAX_EDIT_LEN + 1];
    int                 i, len, vis;

    strcpy(buf, ActiveEdit->text);
    strupr(buf);
    strcat(buf, "_");           // The cursor.
    len = strlen(buf);
    for(i = 0; i < len; i++)
    {
        vis = Ed_VisibleSlotChars(buf + i, M_TextWidth);

        if(i + vis >= len)
        {
            ActiveEdit->firstVisible = i;
            break;
        }
    }
}

#if __JDOOM__ || __JDOOM64__
#define EDITFIELD_BOX_YOFFSET 3
#else
#define EDITFIELD_BOX_YOFFSET 5
#endif

void DrawEditField(menu_t* menu, int index, editfield_t* ef)
{
    char buf[MAX_EDIT_LEN + 1], *text;
    int vis, width = M_TextWidth("a", GF_FONTA) * 27;

    strcpy(buf, ef->text);
    strupr(buf);
    if(ActiveEdit == ef && menuTime & 0x8)
        strcat(buf, "_");
    text = buf + ef->firstVisible;
    vis = Ed_VisibleSlotChars(text, M_TextWidth);
    text[vis] = 0;

    M_DrawSaveLoadBorder(menu->x - 8, menu->y + EDITFIELD_BOX_YOFFSET + (menu->itemHeight * index), width + 16);
    M_DrawText2(text, menu->x, menu->y + EDITFIELD_BOX_YOFFSET + 1 + (menu->itemHeight * index), GF_FONTA, 1, 1, 1, Hu_MenuAlpha());
}

void SCEditField(int efptr, void* data)
{
    editfield_t* ef = data;

    // Activate this edit field.
    ActiveEdit = ef;
    strcpy(ef->oldtext, ef->text);
    Ed_MakeCursorVisible();
}
