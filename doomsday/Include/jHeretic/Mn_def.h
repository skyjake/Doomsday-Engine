// Menu defines and types.

#ifndef __MENU_DEFS_H_
#define __MENU_DEFS_H_

// Macros

#define LEFT_DIR 0
#define RIGHT_DIR 1
#define DIR_MASK 0x1
#define ITEM_HEIGHT 20
#define SELECTOR_XOFFSET (-28)
#define SELECTOR_YOFFSET (-1)
#define SLOTTEXTLEN     16
#define ASCII_CURSOR '_'

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
	MENU_MOUSEOPTS,
	MENU_JOYCONFIG,
	MENU_FILES,
	MENU_LOAD,
	MENU_SAVE,
	MENU_MULTIPLAYER,
//	MENU_PROTOCOL,
//	MENU_HOSTGAME,
//	MENU_JOINGAME,
	MENU_GAMESETUP,
	MENU_PLAYERSETUP,
//	MENU_NETGAME,
//	MENU_TCPIP,
//	MENU_SERIAL,
//	MENU_MODEM,
	MENU_NONE
} MenuType_t;

typedef struct
{
	ItemType_t type;
	char *text;
	boolean (*func)(int option);
	int option;
	MenuType_t menu;
} MenuItem_t;

typedef struct
{
	int x;
	int y;
	void (*drawFunc)(void);
	int itemCount;
	MenuItem_t *items;
	int oldItPos;
	MenuType_t prevMenu;
	// Enhancements. -jk
	void (*textDrawer)(char*,int,int);
	int	itemHeight;
	// For multipage menus.
	int firstItem, numVisItems;
} Menu_t;


extern int MenuTime;
extern boolean shiftdown;
extern Menu_t *CurrentMenu;
extern int CurrentItPos;

void SetMenu(MenuType_t menu);

// Multiplayer menus.
extern Menu_t MultiplayerMenu;
extern Menu_t ProtocolMenu;
extern Menu_t HostMenu;
extern Menu_t JoinMenu;
extern Menu_t GameSetupMenu;
extern Menu_t PlayerSetupMenu;
extern Menu_t NetGameMenu;
extern Menu_t TCPIPMenu;
extern Menu_t SerialMenu;
extern Menu_t ModemMenu;

boolean SCEnterMultiplayerMenu(int option);
void MN_TickerEx(void);	// The extended ticker.

// Edit field routines.
boolean Ed_Responder(event_t *event);

void MN_DrawTitle(char *text, int y);
void MN_DrawMenuText(Menu_t *menu, int index, char *text);

#endif // __MENU_DEFS_H_
