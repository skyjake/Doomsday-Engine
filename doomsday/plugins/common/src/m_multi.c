/**\filem_multi.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>
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

#include "hu_menu.h"

void M_DrawMultiplayerMenu(const mn_page_t* page, int x, int y);
void M_DrawPlayerSetupMenu(const mn_page_t* page, int x, int y);

#if __JHEXEN__
void M_ChangePlayerClass(mn_object_t* obj, int option);
#endif
void M_ChangePlayerColor(mn_object_t* obj, int option);

void M_EnterPlayerSetupMenu(mn_object_t* obj, int option);
void M_AcceptPlayerSetup(mn_object_t* obj, int option);
void M_EnterClientMenu(mn_object_t* obj, int option);

mn_object_t MultiplayerItems[] = {
    { MN_BUTTON,    0,  0,  "Player Setup", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_EnterPlayerSetupMenu },
    { MN_BUTTON,    0,  0,  "Join Game",    GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_EnterClientMenu },
    { MN_NONE }
};

mn_object_t MultiplayerClientItems[] = {
    { MN_BUTTON,    0,  0,  "Player Setup", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_EnterPlayerSetupMenu },
    { MN_BUTTON,    0,  0,  "Disconnect",   GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_EnterClientMenu },
    { MN_NONE}
};

mn_page_t MultiplayerMenu = {
    MultiplayerItems, 2,
    0,
    { 97, 65 },
    M_DrawMultiplayerMenu,
    0, &GameTypeMenu,
    //0, 2
};

mndata_edit_t edit_player_name = { "", "", 0, NULL, "net-name" };

/// \todo Read these names from Text definitions.
mndata_listitem_t listit_player_color[NUMPLAYERCOLORS+1] = {
#if __JHEXEN__
    { "Blue",       0 },
    { "Red",        1 },
    { "Yellow",     2 },
    { "Green",      3 },
    { "Jade",       4 },
    { "White",      5 },
    { "Hazel",      6 },
    { "Purple",     7 },
    { "Automatic",  8 }
#elif __JHERETIC__
    { "Green",      0 },
    { "Orange",     1 },
    { "Red",        2 },
    { "Blue",       3 },
    { "Automatic",  4 }
#else
    { "Green",      0 },
    { "Indigo",     1 },
    { "Brown",      2 },
    { "Red",        3 },
    { "Automatic",  4}
#endif
};
mndata_list_t list_player_color = {
    listit_player_color, NUMLISTITEMS(listit_player_color)
};

mndata_mobjpreview_t mop_player_preview;

mn_object_t PlayerSetupItems[] = {
    { MN_MOBJPREVIEW, 0,0,              "",         0, 0, 0, MNMobjPreview_Drawer, MNMobjPreview_Dimensions, NULL, &mop_player_preview },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, Hu_MenuCvarEdit, &edit_player_name },
#if __JHEXEN__
    { MN_TEXT,      0,  0,              "Class",    GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LISTINLINE, 0, 0,              "",         GF_FONTA, MENU_COLOR3, 0, MNListInline_Drawer, MNListInline_Dimensions, M_ChangePlayerClass },
#endif
    { MN_TEXT,      0,  0,              "Color",    GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LISTINLINE, 0, 0,              "",         GF_FONTA, MENU_COLOR3, 0, MNListInline_Drawer, MNListInline_Dimensions, M_ChangePlayerColor, &list_player_color },
    { MN_BUTTON,    0,  0,              "Accept Changes", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_AcceptPlayerSetup },
    { MN_NONE }
};

mn_page_t PlayerSetupMenu = {
#if __JHEXEN__
    PlayerSetupItems, 7,
#else
    PlayerSetupItems, 5,
#endif
    0,
    { 70, 54 },
    M_DrawPlayerSetupMenu,
    0, &MultiplayerMenu,
#if __JHEXEN__
    //0, 7
#else
    //0, 5
#endif
};

#if __JHEXEN__
static int plrClass;
#endif

void M_DrawMultiplayerMenu(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(cfg.menuColors[0][0], cfg.menuColors[0][1], cfg.menuColors[0][2], Hu_MenuAlpha());

    M_DrawMenuText3(GET_TXT(TXT_MULTIPLAYER), x + 60, y - 25, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawPlayerSetupMenu(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(cfg.menuColors[0][0], cfg.menuColors[0][1], cfg.menuColors[0][2], Hu_MenuAlpha());

    M_DrawMenuText3(GET_TXT(TXT_PLAYERSETUP), x + 90, y - 25, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_EnterMultiplayerMenu(mn_object_t* obj, int option)
{
    // Show the appropriate menu.
    if(IS_NETGAME)
    {
        MultiplayerMenu._objects = MultiplayerClientItems;
    }
    else
    {
        MultiplayerMenu._objects = MultiplayerItems;
    }

    MN_GotoPage(&MultiplayerMenu);
}

void M_EnterClientMenu(mn_object_t* obj, int option)
{
    if(IS_NETGAME)
    {
        DD_Execute(false, "net disconnect");
        Hu_MenuCommand(MCMD_CLOSE);
        return;
    }

    DD_Execute(false, "net setup client");
}

void M_EnterPlayerSetupMenu(mn_object_t* obj, int option)
{
    mndata_mobjpreview_t* mop = &mop_player_preview;

#if __JHEXEN__
    mop->mobjType = PCLASS_INFO(cfg.netClass)->mobjType;
    mop->plrClass = cfg.netClass;
#else
    mop->mobjType = MT_PLAYER;
#endif
    mop->tClass = 0;
    mop->tMap = cfg.netColor;

    list_player_color.selection = cfg.netColor;
#if __JHEXEN__
    plrClass = cfg.netClass;
#endif

    MN_GotoPage(&PlayerSetupMenu);
}

#if __JHEXEN__
void M_ChangePlayerClass(mn_object_t* obj, int option)
{
    if(option == RIGHT_DIR)
    {
        if(plrClass < 2)
            plrClass++;
    }
    else if(plrClass > 0)
        plrClass--;

    /// \fixme Find the object by id.
    { mndata_mobjpreview_t* mop = &mop_player_preview;
    mop->mobjType = PCLASS_INFO(plrClass)->mobjType;
    mop->plrClass = plrClass;
    }
}
#endif

void M_ChangePlayerColor(mn_object_t* obj, int option)
{
    int *val = &list_player_color.selection;
    if(option == RIGHT_DIR)
    {
        if(*val < NUMPLAYERCOLORS)
            ++(*val);
    }
    else if(*val > 0)
    {
        --(*val);
    }

    /// \fixme Find the object by id.
    { mndata_mobjpreview_t* mop = &mop_player_preview;
    mop->tMap = *val;}
}

void M_AcceptPlayerSetup(mn_object_t* obj, int option)
{
    char buf[300];

    cfg.netColor = list_player_color.selection;
#if __JHEXEN__
    cfg.netClass = plrClass;
#endif

    strcpy(buf, "net-name ");
    M_StrCatQuoted(buf, edit_player_name.text, 300);
    DD_Execute(false, buf);

    if(IS_NETGAME)
    {
        strcpy(buf, "setname ");
        M_StrCatQuoted(buf, edit_player_name.text, 300);
        DD_Execute(false, buf);
#if __JHEXEN__
        // Must do 'setclass' first; the real class and color do not change
        // until the server sends us a notification -- this means if we do
        // 'setcolor' first, the 'setclass' after it will override the color
        // change (or such would appear to be the case).
        DD_Executef(false, "setclass %i", plrClass);
#endif
        DD_Executef(false, "setcolor %i", list_player_color.selection);
    }

    MN_GotoPage(&MultiplayerMenu);
}
