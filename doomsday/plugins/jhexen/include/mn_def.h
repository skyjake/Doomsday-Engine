/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * mn_def.h: Menu defines and types.
 */

#ifndef __MN_DEF_H__
#define __MN_DEF_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "jhexen.h"
#include "hu_stuff.h"
#include "m_defs.h"

// Macros

#define LEFT_DIR                0
#define RIGHT_DIR               1
#define ITEM_HEIGHT             20
#define SLOTTEXTLEN             16
#define ASCII_CURSOR            '_'

#define LINEHEIGHT              20
#define LINEHEIGHT_A            10
#define LINEHEIGHT_B            20

#define MENUCURSOR_OFFSET_X     -2
#define MENUCURSOR_OFFSET_Y     -1
#define MENUCURSOR_TICSPERFRAME 8

#define CURSORPREF              "M_SLCTR%d"
#define SKULLBASELMP            "FBULA0"
#define NUMCURSORS              2

#define NUMSAVESLOTS            6

#define MAX_EDIT_LEN            256

// Types
typedef struct {
    char        text[MAX_EDIT_LEN];
    char        oldtext[MAX_EDIT_LEN];  // If the current edit is canceled...
    int         firstVisible;
} editfield_t;

typedef enum {
    MENU_MAIN,
    MENU_NEWGAME,
    MENU_CLASS,
    MENU_SKILL,
    MENU_OPTIONS,
    MENU_OPTIONS2,
    MENU_HUD,
    MENU_MAP,
    MENU_FILES,
    MENU_LOAD,
    MENU_SAVE,
    MENU_MULTIPLAYER,
    MENU_GAMESETUP,
    MENU_PROFILES,
    MENU_PLAYERSETUP,
    MENU_CONTROLS,
    MENU_NONE
} menutype_t;

extern int menuTime;
extern boolean shiftdown;
extern menu_t* currentMenu;
extern short itemOn;

extern menu_t MapDef;
extern menu_t ControlsDef;
extern menu_t ProfilesDef;
extern menu_t EditProfileDef;

// Multiplayer menus.
extern menu_t   MultiplayerMenu;
extern menu_t   GameSetupMenu;

void        M_StartControlPanel(void);
void            M_DrawSaveLoadBorder(int x, int y, int width);
void        M_WriteMenuText(const menu_t *menu, int index, const char *text);

// Color widget.
void        DrawColorWidget();
void        SCColorWidget(int index, void *data);
void        M_WGCurrentColor(int option, void *data);

void        M_SetupNextMenu(menu_t *    menudef);
void        M_DrawTitle(char *text, int y);
void        MN_DrawSlider(const menu_t *menu, int item, int width, int slot);
void        MN_DrawColorBox(const menu_t *menu, int index, float r, float g,
                        float b, float a);
void        M_FloatMod10(float *variable, int option);

void        SCEnterMultiplayerMenu(int option, void *data);
void        MN_TickerEx(void); // The extended ticker.

// Widget routines.
boolean     Cl_Responder(event_t *event); // Handles control in a menu widget

// Edit field routines.
boolean     Ed_Responder(event_t *event);

boolean     MN_CurrentMenuHasBackground(void);

void        MN_ActivateMenu(void);
void        MN_DeactivateMenu(void);
void        MN_TextFilter(char *text);
void        MN_DrTextA(char *text, int x, int y);
void        MN_DrTextAYellow(char *text, int x, int y);
int         MN_TextAWidth(char *text);
void        MN_DrTextB(char *text, int x, int y);
int         MN_TextBWidth(char *text);
void        MN_DrawTitle(char *text, int y);

// Drawing text in the current state.
void        MN_DrTextA_CS(char *text, int x, int y);
void        MN_DrTextAYellow_CS(char *text, int x, int y);
void        MN_DrTextB_CS(char *text, int x, int y);

void        strcatQuoted(char *dest, char *src);

void            M_ToggleVar(int option, void* context);

#endif
