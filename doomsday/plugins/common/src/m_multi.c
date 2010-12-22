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
#endif

#include "hu_stuff.h"
#include "hu_menu.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void            DrawMultiplayerMenu(const mn_page_t* page, int x, int y);
void            DrawGameSetupMenu(const mn_page_t* page, int x, int y);
void            DrawPlayerSetupMenu(const mn_page_t* page, int x, int y);

void            SCGameSetupMap(mn_object_t* obj, int option);
void            SCGameSetupDamageMod(mn_object_t* obj, int option);
void            SCGameSetupHealthMod(mn_object_t* obj, int option);
void            SCGameSetupGravity(mn_object_t* obj, int option);
#if __JHEXEN__
void            SCPlayerClass(mn_object_t* obj, int option);
#endif
void            SCPlayerColor(mn_object_t* obj, int option);

void            SCOpenServer(mn_object_t* obj, int option);
void            SCCloseServer(mn_object_t* obj, int option);
void            SCEnterPlayerSetupMenu(mn_object_t* obj, int option);
void            SCAcceptPlayer(mn_object_t* obj, int option);
void            SCEnterHostMenu(mn_object_t* obj, int option);
void            SCEnterJoinMenu(mn_object_t* obj, int option);
void            SCEnterGameSetup(mn_object_t* obj, int option);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mndata_edit_t plrNameEd;
int CurrentPlrFrame = 0;

mn_object_t MultiplayerItems[] = {
    { MN_BUTTON,    0,  0,  "Player Setup", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, SCEnterPlayerSetupMenu },
    { MN_BUTTON,    0,  0,  "Join Game",    GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, SCEnterJoinMenu },
    { MN_BUTTON,    0,  0,  "Host Game",    GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, SCEnterHostMenu },
    { MN_NONE }
};

mn_object_t MultiplayerServerItems[] = {
    { MN_BUTTON,    0,  0,  "Player Setup", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, SCEnterPlayerSetupMenu },
    { MN_BUTTON,    0,  0,  "Game Setup",   GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, SCEnterHostMenu },
    { MN_BUTTON,    0,  0,  "Close Server", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, SCCloseServer },
    { MN_NONE }
};

mn_object_t MultiplayerClientItems[] = {
    { MN_BUTTON,    0,  0,  "Player Setup", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, SCEnterPlayerSetupMenu },
    { MN_BUTTON,    0,  0,  "Disconnect",   GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, SCEnterJoinMenu },
    { MN_NONE}
};

mn_page_t MultiplayerMenu = {
    MultiplayerItems, 3,
    0,
    { 116, 70 },
    DrawMultiplayerMenu,
    0, &GameTypeMenu,
    .2f,
    0, 3
};

#if __JDOOM__ || __JHERETIC__
mndata_list_t lst_net_episode = {
    0, 0, "server-game-episode"
};
#endif

mndata_listitem_t lstit_net_skill[NUM_SKILL_MODES];
mndata_list_t lst_net_skill = {
    lstit_net_skill, NUMLISTITEMS(lstit_net_skill), "server-game-skill"
};

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_listitem_t lstit_net_gamemode[] = {
    { "CoOperative",  0 },
    { "Deathmatch 1", 1 },
    { "Deathmatch 2", 2 }
};
mndata_list_t lst_net_gamemode = {
    lstit_net_gamemode, NUMLISTITEMS(lstit_net_gamemode), "server-game-deathmatch"
};
#endif

#if __JDOOM__
# define NUM_GAMESETUP_ITEMS        34
#elif __JDOOM64__
# define NUM_GAMESETUP_ITEMS        32
#elif __JHERETIC__
# define NUM_GAMESETUP_ITEMS        24
#elif __JHEXEN__
# define NUM_GAMESETUP_ITEMS        17
#endif

mn_object_t GameSetupItems[] = {
#if __JDOOM__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Episode",      GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LIST,      0,  0,  "",             GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, Hu_MenuCvarList, &lst_net_episode },
#endif
    { MN_TEXT,      0,  0,  "Map",          GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LIST,      0,  0,  "",             GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, SCGameSetupMap },
    { MN_TEXT,      0,  0,  "Skill",        GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LIST,      0,  0,  "",             GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, Hu_MenuCvarList, &lst_net_skill },
#if __JHEXEN__
    { MN_TEXT,      0,  0,  "Deathmatch",   GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-deathmatch", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Random Classes", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-randclass", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#else
    { MN_TEXT,      0,  0,  "Mode",         GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LIST,      0,  0,  "",             GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, Hu_MenuCvarList, &lst_net_gamemode },
#endif
    { MN_TEXT,      0,  0,  "Monsters",     GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-nomonsters", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Respawn Monsters", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-respawn", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Allow Jumping", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-jump", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Allow BFG Aiming", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-bfg-freeaim", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "No CoOp Damage", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-coop-nodamage", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "No CoOp Weapons", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-coop-noweapons", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "No CoOp Objects", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-coop-nothing", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "CoOp Items Respawn", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-coop-respawn-items", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "No BFG 9000",  GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-nobfg",  GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "No Team Damage", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-noteamdamage", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
    { MN_TEXT,      0,  0,  "No Max Z Radius Attacks", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "server-game-radiusattack-nomaxz", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_LIST,      0,  0,  "Damage Multiplier", GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, SCGameSetupDamageMod },
    { MN_LIST,      0,  0,  "Health Multiplier", GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, SCGameSetupHealthMod },
    { MN_LIST,      0,  0,  "Gravity Multiplier", GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, SCGameSetupGravity },
    { MN_BUTTON,    0,  0,  "Proceed...",   GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, SCOpenServer },
    { MN_NONE }
};

mn_page_t GameSetupMenu = {
    GameSetupItems, NUM_GAMESETUP_ITEMS,
    0,
#if __JDOOM__ || __JDOOM64__
    { 90, 54 },
#elif __JHERETIC__
    { 74, 64 },
#elif __JHEXEN__
    { 90, 64 },
#endif
    DrawGameSetupMenu,
    0, &MultiplayerMenu,
#if __JHERETIC__ || __JHEXEN__
    0, 11, { 11, 64 }
#else
    0, 13, { 13, 54 }
#endif
};

mn_object_t PlayerSetupItems[] = {
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_ActivateEditField, &plrNameEd },
#if __JHEXEN__
    { MN_TEXT,      0,  0,              "Class",    GF_FONTB, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LIST,      0,  0,              "",         GF_FONTB, 0, MNList_InlineDrawer, MNList_InlineDimensions, SCPlayerClass },
#endif
    { MN_TEXT,      0,  0,              "Color",    GF_FONTB, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LIST,      0,  0,              "",         GF_FONTB, 0, MNList_InlineDrawer, MNList_InlineDimensions, SCPlayerColor },
    { MN_BUTTON,    0,  0,              "Accept Changes", GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, SCAcceptPlayer },
    { MN_NONE }
};

mn_page_t PlayerSetupMenu = {
#if __JHEXEN__
    PlayerSetupItems, 5,
#else
    PlayerSetupItems, 4,
#endif
    0,
    { 60, 52 },
    DrawPlayerSetupMenu,
    0, &MultiplayerMenu,
    .2f,
#if __JHEXEN__
    0, 4
#else
    0, 3
#endif
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int plrColor;

#if __JHEXEN__
static int plrClass;
#endif

// CODE --------------------------------------------------------------------

void DrawMultiplayerMenu(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(cfg.menuColors[0][0], cfg.menuColors[0][1], cfg.menuColors[0][2], Hu_MenuAlpha());

    M_DrawMenuText3(GET_TXT(TXT_MULTIPLAYER), SCREENWIDTH/2, y-30, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

void DrawGameSetupMenu(const mn_page_t* page, int x, int y)
{
    char buf[50];
    int idx;
#if __JHEXEN__
    char* mapName = P_GetMapName(P_TranslateMap(cfg.netMap));
#endif

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][0], cfg.menuColors[0][1], cfg.menuColors[0][2], Hu_MenuAlpha());
    M_DrawMenuText3(GET_TXT(TXT_GAMESETUP), SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);

    idx = 0;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
# if __JDOOM__ || __JHERETIC__
#  if __JDOOM__
    if(gameModeBits & (GM_DOOM|GM_DOOM_SHAREWARE|GM_DOOM_ULTIMATE))
#  endif
    {
        sprintf(buf, "%u", cfg.netEpisode+1);
        //M_WriteMenuText(menu, idx++, x, y, buf);
    }
# endif
    sprintf(buf, "%u", cfg.netMap+1);
    //M_WriteMenuText(menu, idx++, x, y, buf);
#elif __JHEXEN__

    sprintf(buf, "%u", cfg.netMap+1);
    //M_WriteMenuText(menu, idx++, x, y, buf);
    DGL_Color4f(1, 0.7f, 0.3f, Hu_MenuAlpha());
    //M_DrawMenuText3(mapName, 160, y + page->itemHeight, GF_FONTA, DTF_ALIGN_TOP);

    idx++;
#endif // __JHEXEN__

    sprintf(buf, "%i", cfg.netMobDamageModifier);
    //M_WriteMenuText(menu, idx++, x, y, buf);
    sprintf(buf, "%i", cfg.netMobHealthModifier);
    //M_WriteMenuText(menu, idx++, x, y, buf);

    if(cfg.netGravity != -1)
        sprintf(buf, "%i", cfg.netGravity);
    else
        sprintf(buf, "MAP DEFAULT");
    //M_WriteMenuText(menu, idx++, x, y, buf);

    DGL_Disable(DGL_TEXTURE_2D);
}

void DrawPlayerSetupMenu(const mn_page_t* page, int inX, int inY)
{
#define AVAILABLE_WIDTH     38
#define AVAILABLE_HEIGHT    52

    spriteinfo_t sprInfo;
    int useColor = plrColor;
    float mnAlpha = Hu_MenuAlpha();
    int tclass = 0;
#if __JHEXEN__
    int numColors = 8;
    static const int sprites[3] = {SPR_PLAY, SPR_CLER, SPR_MAGE};
#else
    int plrClass = 0;
    int numColors = 4;
    static const int sprites[3] = {SPR_PLAY, SPR_PLAY, SPR_PLAY};
#endif
    float x = inX, y = inY, w, h;
    float s, t, scale;

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][0], cfg.menuColors[0][1], cfg.menuColors[0][2], mnAlpha);
    M_DrawMenuText3(GET_TXT(TXT_PLAYERSETUP), SCREENWIDTH/2, y-28, GF_FONTB, DTF_ALIGN_TOP);

    if(useColor == numColors)
        useColor = mnTime / 5 % numColors;
    if(plrColor == numColors)
    {
        short textFlags = DTF_ALIGN_TOPLEFT;
        if(cfg.menuEffects == 0)
            textFlags |= DTF_NO_TYPEIN;
        DGL_Color4f(1, 1, 1, mnAlpha);
        GL_DrawTextFragment3("AUTOMATIC", 184,
#if __JDOOM__ || __JDOOM64__
                      y + 49,
#elif __JHERETIC__
                      y + 65,
#else
                      y + 64,
#endif
                      GF_FONTA, textFlags);
    }

#if __JHEXEN__
    R_GetTranslation(plrClass, useColor, &tclass, &useColor);
#endif

    // Draw the color selection as a random player frame.
    R_GetSpriteInfo(sprites[plrClass], CurrentPlrFrame, &sprInfo);

    x += 102;
#if __JDOOM__ || __JDOOM64__
    y += 70;
#elif __JHERETIC__
    y += 80;
#else
    y += 90;
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

    DGL_Color4f(1, 1, 1, mnAlpha);
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

    DGL_Disable(DGL_TEXTURE_2D);

#undef AVAILABLE_WIDTH
#undef AVAILABLE_HEIGHT
}

void SCEnterMultiplayerMenu(mn_object_t* obj, int option)
{
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    int i;
#endif
    int count;

    // Choose the correct items for the Game Setup menu.
#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM)
    {
        // Neutralize the Episode select objects.
        GameSetupItems[0].flags |= MNF_HIDDEN;
        GameSetupItems[1].flags |= MNF_HIDDEN;
    }
#endif

#if __JDOOM__ || __JHERETIC__
    // Allocate and populate the episode list.
# if __JHERETIC__
    lst_net_episode.count = (gameMode == heretic_shareware? 1 : gameMode == heretic_extended? 6 : 3);
# else // __JDOOM__
    lst_net_episode.count = ((gameMode == doom_shareware || gameMode == doom_chex)? 1 : gameMode == doom_ultimate ? 4 : 3);
# endif
    lst_net_episode.items = Z_Malloc(sizeof(mndata_listitem_t)*lst_net_episode.count, PU_GAMESTATIC, 0);
    for(i = 0; i < lst_net_episode.count; ++i)
    {
        mndata_listitem_t* item = &((mndata_listitem_t*)lst_net_episode.items)[i];
        dd_snprintf(item->text, 256, "%s", GET_TXT(TXT_EPISODE1 + i));
        item->data = i;
    }
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    // Populate the skillmode list.
    for(i = 0; i < NUM_SKILL_MODES; ++i)
    {
        mndata_listitem_t* item = &((mndata_listitem_t*)lstit_net_skill)[i];
        dd_snprintf(item->text, 256, "%s", GET_TXT(TXT_SKILL1 + i));
        item->data = i;
    }
#endif

    // Show the appropriate menu.
    if(IS_NETGAME)
    {
        MultiplayerMenu._objects = IS_SERVER ? MultiplayerServerItems : MultiplayerClientItems;
        count = IS_SERVER ? 3 : 2;
    }
    else
    {
        MultiplayerMenu._objects = MultiplayerItems;
        count = 3;
    }
    MultiplayerMenu._size = MultiplayerMenu.numVisObjects = count;
    MultiplayerMenu.focus = 0;

    MN_GotoPage(&MultiplayerMenu);
}

void SCEnterHostMenu(mn_object_t* obj, int option)
{
    SCEnterGameSetup(NULL, 0);
}

void SCEnterJoinMenu(mn_object_t* obj, int option)
{
    if(IS_NETGAME)
    {
        DD_Execute(false, "net disconnect");
        Hu_MenuCommand(MCMD_CLOSE);
        return;
    }

    DD_Execute(false, "net setup client");
}

void SCEnterGameSetup(mn_object_t* obj, int option)
{
    // See to it that the episode and map numbers are correct.
#if __JDOOM64__
    if(cfg.netMap > 31)
        cfg.netMap = 31;
#elif __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
    {
        cfg.netEpisode = 0;
        if(gameMode == doom_chex && cfg.netMap > 4)
            cfg.netMap = 4;
    }
    else // Any DOOM.
    {
        if(gameMode == doom_ultimate)
        {
            if(cfg.netEpisode > 3)
                cfg.netEpisode = 3;
            if(cfg.netMap > 8)
                cfg.netMap = 8;
        }
        else if(gameMode == doom)
        {
            if(cfg.netEpisode > 2)
                cfg.netEpisode = 2;
            if(cfg.netMap > 8)
                cfg.netMap = 8;
        }
        else if(gameMode == doom_shareware)
        {
            cfg.netEpisode = 0;
            if(cfg.netMap > 8)
                cfg.netMap = 8;
        }
    }
#elif __JHERETIC__
    if(cfg.netMap > 8)
        cfg.netMap = 8;
    if(cfg.netEpisode > 5)
        cfg.netEpisode = 5;
    if(cfg.netEpisode == 5 && cfg.netMap > 2)
        cfg.netMap = 2;
#elif __JHEXEN__
    if(cfg.netMap > 30)
        cfg.netMap = 30;
#endif
    MN_GotoPage(&GameSetupMenu);
}

void SCGameSetupMap(mn_object_t* obj, int option)
{
    if(option == RIGHT_DIR)
    {
#if __JDOOM64__
        if(cfg.netMap < 31)
            cfg.netMap++;
#elif __JDOOM__
        if(cfg.netMap < ((gameModeBits & GM_ANY_DOOM2) ? 31 : (gameMode == doom_chex? 4 : 8)))
            cfg.netMap++;
#elif __JHERETIC__
        if(cfg.netMap < (cfg.netEpisode == 5? 2 : 8))
            cfg.netMap++;
#elif __JHEXEN__
        if(cfg.netMap < 30)
            cfg.netMap++;
#endif
    }
    else if(cfg.netMap != 0)
    {
        cfg.netMap--;
    }
}

void SCOpenServer(mn_object_t* obj, int option)
{
    if(IS_NETGAME)
    {
        // Game already running, just change map.
#if __JHEXEN__
        DD_Executef(false, "setmap %u", cfg.netMap+1);
#elif __JDOOM64__
        DD_Executef(false, "setmap 1 %u", cfg.netMap+1);
#else
        DD_Executef(false, "setmap %u %u", cfg.netEpisode+1, cfg.netMap+1);
#endif

        Hu_MenuCommand(MCMD_CLOSE);
        return;
    }

    // Go to DMI to setup server.
    DD_Execute(false, "net setup server");
}

void SCCloseServer(mn_object_t* obj, int option)
{
    DD_Execute(false, "net server close");
    Hu_MenuCommand(MCMD_CLOSE);
}

void SCEnterPlayerSetupMenu(mn_object_t* obj, int option)
{
    M_SetEditFieldText(&plrNameEd, *(char**) Con_GetVariable("net-name")->ptr);
    plrColor = cfg.netColor;
#if __JHEXEN__
    plrClass = cfg.netClass;
#endif
    MN_GotoPage(&PlayerSetupMenu);
}

#if __JHEXEN__
void SCPlayerClass(mn_object_t* obj, int option)
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

void SCPlayerColor(mn_object_t* obj, int option)
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

void SCAcceptPlayer(mn_object_t* obj, int option)
{
    char buf[300];

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
        DD_Executef(false, "setclass %i", plrClass);
#endif
        DD_Executef(false, "setcolor %i", plrColor);
    }

    MN_GotoPage(&MultiplayerMenu);
}

void SCGameSetupDamageMod(mn_object_t* obj, int option)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netMobDamageModifier < 100)
            cfg.netMobDamageModifier++;
    }
    else if(cfg.netMobDamageModifier > 1)
        cfg.netMobDamageModifier--;
}

void SCGameSetupHealthMod(mn_object_t* obj, int option)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netMobHealthModifier < 20)
            cfg.netMobHealthModifier++;
    }
    else if(cfg.netMobHealthModifier > 1)
        cfg.netMobHealthModifier--;
}

void SCGameSetupGravity(mn_object_t* obj, int option)
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
    static int frameTimer = 0;

    if(MN_CurrentPage() == &PlayerSetupMenu)
    {
        if(frameTimer++ >= 14)
        {
            frameTimer = 0;
            CurrentPlrFrame = M_Random() % 8;
        }
    }
}
