// Menu defines and types.

#ifndef __MENU_DEFS_H_
#define __MENU_DEFS_H_

#include "r_defs.h"

// Macros

#define LEFT_DIR	0
#define RIGHT_DIR	1
#define DIR_MASK	0x1
#define ITEM_HEIGHT 20
#define SELECTOR_XOFFSET (-28)
#define SELECTOR_YOFFSET (-1)
#define SLOTTEXTLEN     16
#define ASCII_CURSOR '['

#define LINEHEIGHT		16
#define LINEHEIGHT_B	15
#define LINEHEIGHT_A	8

// Types

typedef enum
{
	ITT_EMPTY,
	ITT_EFUNC,
	ITT_LRFUNC,
	ITT_SETMENU,
	ITT_INERT
} ItemType_t;

typedef enum
{
	MENU_MAIN,
	MENU_EPISODE,
	MENU_SKILL,
	MENU_OPTIONS,
	MENU_OPTIONS2,
	MENU_GAMEPLAY,
	MENU_HUD,
	MENU_CONTROLS,
	MENU_MOUSE,
	MENU_JOYSTICK,
	MENU_LOAD,
	MENU_SAVE,
	MENU_MULTIPLAYER,
	MENU_GAMESETUP,
	MENU_PLAYERSETUP,
	MENU_NONE
} MenuType_t;

typedef struct
{
	ItemType_t type;
	char *text;
	void (*func)(int option);
	int option;
	char *lumpname;
} MenuItem_t;

typedef struct
{
	int x;
	int y;
	void (*drawFunc)(void);
	int itemCount;
	MenuItem_t *items;
	int lastOn;
	MenuType_t prevMenu;
	dpatch_t *font;				// Font for menu items.
	//float red, green, blue;		// And their color.
	int	itemHeight;
	// For multipage menus.
	int firstItem, numVisItems;
} Menu_t;


extern int MenuTime;
extern boolean shiftdown;
extern Menu_t *currentMenu;
extern short itemOn;

void SetMenu(MenuType_t menu);
void M_DrawSaveLoadBorder(int x,int y);
void M_DrawTitle(char *text, int y);
void M_WriteText(int x, int y, char *string);
void M_WriteText2(int x, int y, char *string, dpatch_t *font, 
				  float	red, float green, float	blue);
void M_WriteMenuText(Menu_t *menu, int index, char *text);

extern Menu_t ControlsDef;

// Multiplayer menus.
extern Menu_t MultiplayerMenu;
//extern Menu_t ProtocolMenu;
//extern Menu_t HostMenu;
//extern Menu_t JoinMenu;
extern Menu_t GameSetupMenu;
extern Menu_t PlayerSetupMenu;
//extern Menu_t NetGameMenu;
//extern Menu_t TCPIPMenu;
//extern Menu_t SerialMenu;
//extern Menu_t ModemMenu;

boolean SCEnterMultiplayerMenu(int option);
void MN_TickerEx(void);	// The extended ticker.

// Edit field routines.
boolean Ed_Responder(event_t *event);

#endif // __MENU_DEFS_H_
