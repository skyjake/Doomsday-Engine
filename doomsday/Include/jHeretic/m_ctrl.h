/* $Id$
 * Controls menu definition
 */
#ifndef __JHERETIC_CONTROLS__
#define __JHERETIC_CONTROLS__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "jHeretic/H_Action.h"

void    G_DefaultBindings(void);
void    M_DrawControlsMenu(void);

#define CTLCFG_TYPE void

CTLCFG_TYPE SCControlConfig(int option, void *data);

// Control flags.
#define CLF_ACTION		0x1		// The control is an action (+/- in front).
#define CLF_REPEAT		0x2		// Bind down + repeat.

typedef struct {
	char   *command;			// The command to execute.
	int     flags;
	int     bindClass;			// Class it should be bound into
	int     defKey;				// 
	int     defMouse;			// Zero means there is no default.
	int     defJoy;				//
} Control_t;

static const Control_t controls[] = {
	// Actions (must be first so the H2A_* constants can be used).
	{"left", CLF_ACTION, BDC_NORMAL, DDKEY_LEFTARROW, 0, 0},
	{"right", CLF_ACTION, BDC_NORMAL, DDKEY_RIGHTARROW, 0, 0},
	{"forward", CLF_ACTION, BDC_NORMAL, DDKEY_UPARROW, 0, 0},
	{"backward", CLF_ACTION, BDC_NORMAL, DDKEY_DOWNARROW, 0, 0},
	{"strafel", CLF_ACTION, BDC_NORMAL, ',', 0, 0},
	{"strafer", CLF_ACTION, BDC_NORMAL, '.', 0, 0},
	{"fire", CLF_ACTION, BDC_NORMAL, DDKEY_RCTRL, 1, 1},
	{"use", CLF_ACTION, BDC_NORMAL, ' ', 0, 4},
	{"strafe", CLF_ACTION, BDC_NORMAL, DDKEY_RALT, 3, 2},
	{"speed", CLF_ACTION, BDC_NORMAL, DDKEY_RSHIFT, 0, 3},

	{"flyup", CLF_ACTION, BDC_NORMAL, DDKEY_PGUP, 0, 8},
	{"flydown", CLF_ACTION, BDC_NORMAL, DDKEY_INS, 0, 9},
	{"falldown", CLF_ACTION, BDC_NORMAL, DDKEY_HOME, 0, 0},
	{"lookup", CLF_ACTION, BDC_NORMAL, DDKEY_PGDN, 0, 6},
	{"lookdown", CLF_ACTION, BDC_NORMAL, DDKEY_DEL, 0, 7},
	{"lookcntr", CLF_ACTION, BDC_NORMAL, DDKEY_END, 0, 0},
	{"usearti", CLF_ACTION, BDC_NORMAL, DDKEY_ENTER, 0, 0},
	{"mlook", CLF_ACTION, BDC_NORMAL, 'm', 0, 0},
	{"jlook", CLF_ACTION, BDC_NORMAL, 'j', 0, 0},
	{"nextwpn", CLF_ACTION, BDC_NORMAL, 0, 0, 0},

	{"prevwpn", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"weapon1", CLF_ACTION, BDC_NORMAL, '1', 0, 0},
	{"weapon2", CLF_ACTION, BDC_NORMAL, '2', 0, 0},
	{"weapon3", CLF_ACTION, BDC_NORMAL, '3', 0, 0},
	{"weapon4", CLF_ACTION, BDC_NORMAL, '4', 0, 0},
	{"weapon5", CLF_ACTION, BDC_NORMAL, '5', 0, 0},
	{"weapon6", CLF_ACTION, BDC_NORMAL, '6', 0, 0},
	{"weapon7", CLF_ACTION, BDC_NORMAL, '7', 0, 0},
	{"weapon8", CLF_ACTION, BDC_NORMAL, '8', 0, 0},
	{"weapon9", CLF_ACTION, BDC_NORMAL, '9', 0, 0},

	{"cantdie", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"invisib", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"health", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"sphealth", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"tomepwr", CLF_ACTION, BDC_NORMAL, DDKEY_BACKSPACE, 0, 0},
	{"torch", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"firebomb", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"egg", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"flyarti", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"teleport", CLF_ACTION, BDC_NORMAL, 0, 0, 0},

	{"panic", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"demostop", CLF_ACTION, BDC_NORMAL, 'o', 0, 0},

	// Menu hotkeys (default: F1 - F12).
	/*42 */ {"infoscreen", 0, BDC_NORMAL, DDKEY_F1, 0, 0},
	{"loadgame", 0, BDC_NORMAL, DDKEY_F3, 0, 0},
	{"savegame", 0, BDC_NORMAL, DDKEY_F2, 0, 0},
	{"soundmenu", 0, BDC_NORMAL, DDKEY_F4, 0, 0},
	{"quicksave", 0, BDC_NORMAL, DDKEY_F6, 0, 0},
	{"endgame", 0, BDC_NORMAL, DDKEY_F7, 0, 0},
	{"togglemsgs", 0, BDC_NORMAL, DDKEY_F8, 0, 0},
	{"quickload", 0, BDC_NORMAL, DDKEY_F9, 0, 0},
	{"quit", 0, BDC_NORMAL, DDKEY_F10, 0, 0},
	{"togglegamma", 0, BDC_NORMAL, DDKEY_F11, 0, 0},
	{"spy", 0, BDC_NORMAL, DDKEY_F12, 0, 0},

	// Inventory.
	{"invleft", CLF_REPEAT, BDC_NORMAL, '[', 0, 0},
	{"invright", CLF_REPEAT, BDC_NORMAL, ']', 0, 0},

	// Screen controls.
	{"viewsize +", CLF_REPEAT, BDC_NORMAL, '=', 0, 0},
	{"viewsize -", CLF_REPEAT, BDC_NORMAL, '-', 0, 0},
	{"sbsize +", CLF_REPEAT, BDC_NORMAL, 0, 0, 0},
	{"sbsize -", CLF_REPEAT, BDC_NORMAL, 0, 0, 0},

	// Misc.
	{"pause", 0, BDC_NORMAL, DDKEY_PAUSE, 0, 0},
	{"jump", CLF_ACTION, BDC_NORMAL, 0, 0, 0},
	{"beginchat", 0, BDC_NORMAL, 't', 0, 0},
	{"beginchat 0", 0, BDC_NORMAL, 'g', 0, 0},
	{"beginchat 1", 0, BDC_NORMAL, 'y', 0, 0},
	{"beginchat 2", 0, BDC_NORMAL, 'r', 0, 0},
	{"beginchat 3", 0, BDC_NORMAL, 'b', 0, 0},
	{"screenshot", 0, BDC_NORMAL, 0, 0, 0},

	{"automap", 0, BDC_NORMAL, DDKEY_TAB, 0, 0},
	{"follow", 0, BDC_CLASS1, 'f', 0, 0},
	{"rotate", 0, BDC_CLASS1, 'r', 0, 0},
	{"grid", 0, BDC_CLASS1, 'g', 0, 0},
	{"mzoomin", CLF_ACTION, BDC_CLASS1, '=', 0, 0},
	{"mzoomout", CLF_ACTION, BDC_CLASS1, '-', 0, 0},
	{"zoommax", 0, BDC_CLASS1, '0', 0, 0},
	{"addmark", 0, BDC_CLASS1, 'm', 0, 0},
	{"clearmarks", 0, BDC_CLASS1, 'c', 0, 0},
	{"mpanup", CLF_ACTION, BDC_CLASS2, DDKEY_UPARROW, 0, 0},
	{"mpandown", CLF_ACTION, BDC_CLASS2, DDKEY_DOWNARROW, 0, 0},
	{"mpanleft", CLF_ACTION, BDC_CLASS2, DDKEY_LEFTARROW, 0, 0},
	{"mpanright", CLF_ACTION, BDC_CLASS2, DDKEY_RIGHTARROW, 0, 0},

	{"", 0, 0, 0, 0, 0}				// terminator
};

extern const Control_t *grabbing;

static const MenuItem_t ControlsItems[] = {
	{ITT_EMPTY, "PLAYER ACTIONS", NULL, 0},
	{ITT_EFUNC, "LEFT :", SCControlConfig, A_TURNLEFT},
	{ITT_EFUNC, "RIGHT :", SCControlConfig, A_TURNRIGHT},
	{ITT_EFUNC, "FORWARD :", SCControlConfig, A_FORWARD},
	{ITT_EFUNC, "BACKWARD :", SCControlConfig, A_BACKWARD},
	{ITT_EFUNC, "STRAFE LEFT :", SCControlConfig, A_STRAFELEFT},
	{ITT_EFUNC, "STRAFE RIGHT :", SCControlConfig, A_STRAFERIGHT},
	{ITT_EFUNC, "FIRE :", SCControlConfig, A_FIRE},
	{ITT_EFUNC, "USE :", SCControlConfig, A_USE},
	{ITT_EFUNC, "JUMP : ", SCControlConfig, 60 /*A_JUMP */ },
	{ITT_EFUNC, "STRAFE :", SCControlConfig, A_STRAFE},
	{ITT_EFUNC, "SPEED :", SCControlConfig, A_SPEED},
	{ITT_EFUNC, "FLY UP :", SCControlConfig, A_FLYUP},
	{ITT_EFUNC, "FLY DOWN :", SCControlConfig, A_FLYDOWN},
	{ITT_EFUNC, "FALL DOWN :", SCControlConfig, A_FLYCENTER},
	{ITT_EFUNC, "LOOK UP :", SCControlConfig, A_LOOKUP},
	{ITT_EFUNC, "LOOK DOWN :", SCControlConfig, A_LOOKDOWN},
	{ITT_EFUNC, "LOOK CENTER :", SCControlConfig, A_LOOKCENTER},
	{ITT_EFUNC, "MOUSE LOOK :", SCControlConfig, A_MLOOK},
	{ITT_EFUNC, "JOYSTICK LOOK :", SCControlConfig, A_JLOOK},
	{ITT_EFUNC, "NEXT WEAPON :", SCControlConfig, A_NEXTWEAPON},
	{ITT_EFUNC, "PREV WEAPON :", SCControlConfig, A_PREVIOUSWEAPON},
	{ITT_EFUNC, "STAFF/GAUNTLETS :", SCControlConfig, A_WEAPON1},
	{ITT_EFUNC, "ELVENWAND :", SCControlConfig, A_WEAPON2},
	{ITT_EFUNC, "CROSSBOW :", SCControlConfig, A_WEAPON3},
	{ITT_EFUNC, "DRAGON CLAW :", SCControlConfig, A_WEAPON4},
	{ITT_EFUNC, "HELLSTAFF :", SCControlConfig, A_WEAPON5},
	{ITT_EFUNC, "PHOENIX ROD :", SCControlConfig, A_WEAPON6},
	{ITT_EFUNC, "FIREMACE :", SCControlConfig, A_WEAPON7},
	{ITT_EFUNC, "PANIC :", SCControlConfig, A_PANIC},
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, "ARTIFACTS", NULL, 0},
	{ITT_EFUNC, "INVINCIBILITY :", SCControlConfig, A_INVULNERABILITY},
	{ITT_EFUNC, "SHADOWSPHERE :", SCControlConfig, A_INVISIBILITY},
	{ITT_EFUNC, "QUARTZ FLASK :", SCControlConfig, A_HEALTH},
	{ITT_EFUNC, "MYSTIC URN :", SCControlConfig, A_SUPERHEALTH},
	{ITT_EFUNC, "TOME OF POWER:", SCControlConfig, A_TOMEOFPOWER},
	{ITT_EFUNC, "TORCH :", SCControlConfig, A_TORCH},
	{ITT_EFUNC, "TIME BOMB :", SCControlConfig, A_FIREBOMB},
	{ITT_EFUNC, "MORPH OVUM :", SCControlConfig, A_EGG},
	{ITT_EFUNC, "WINGS OF WRATH :", SCControlConfig, A_FLY},
	{ITT_EFUNC, "CHAOS DEVICE :", SCControlConfig, A_TELEPORT},
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, "INVENTORY", NULL, 0},
	{ITT_EFUNC, "INVENTORY LEFT :", SCControlConfig, 53},
	{ITT_EFUNC, "INVENTORY RIGHT :", SCControlConfig, 54},
	{ITT_EFUNC, "USE ARTIFACT :", SCControlConfig, A_USEARTIFACT},
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, "MENU HOTKEYS", NULL, 0},
	{ITT_EFUNC, "INFO :", SCControlConfig, 42},
	{ITT_EFUNC, "SOUND MENU :", SCControlConfig, 45},
	{ITT_EFUNC, "LOAD GAME :", SCControlConfig, 43},
	{ITT_EFUNC, "SAVE GAME :", SCControlConfig, 44},
	{ITT_EFUNC, "QUICK LOAD :", SCControlConfig, 49},
	{ITT_EFUNC, "QUICK SAVE :", SCControlConfig, 46},
	//  { ITT_EFUNC, "SUICIDE :", SCControlConfig, 44 },
	{ITT_EFUNC, "END GAME :", SCControlConfig, 47},
	{ITT_EFUNC, "QUIT :", SCControlConfig, 50},
	{ITT_EFUNC, "MESSAGES ON/OFF:", SCControlConfig, 48},
	{ITT_EFUNC, "GAMMA CORRECTION :", SCControlConfig, 51},
	{ITT_EFUNC, "SPY MODE :", SCControlConfig, 52},
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, "SCREEN", NULL, 0},
	{ITT_EFUNC, "SMALLER VIEW :", SCControlConfig, 56},
	{ITT_EFUNC, "LARGER VIEW :", SCControlConfig, 55},
	{ITT_EFUNC, "SMALLER STATBAR :", SCControlConfig, 58},
	{ITT_EFUNC, "LARGER STATBAR :", SCControlConfig, 57},
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},

	{ITT_EMPTY, "AUTOMAP", NULL, 0},
	{ITT_EFUNC, "OPEN/CLOSE MAP :", SCControlConfig, 67},
	{ITT_EFUNC, "PAN UP :", SCControlConfig, 76},
	{ITT_EFUNC, "PAN DOWN :", SCControlConfig, 77},
	{ITT_EFUNC, "PAN LEFT :", SCControlConfig, 78},
	{ITT_EFUNC, "PAN RIGHT :", SCControlConfig, 79},
	{ITT_EFUNC, "FOLLOW MODE :", SCControlConfig, 68},
	{ITT_EFUNC, "ROTATE MODE :", SCControlConfig, 69},
	{ITT_EFUNC, "TOGGLE GRID :", SCControlConfig, 70},
	{ITT_EFUNC, "ZOOM IN :", SCControlConfig, 71},
	{ITT_EFUNC, "ZOOM OUT :", SCControlConfig, 72},
	{ITT_EFUNC, "ZOOM EXTENTS :", SCControlConfig, 73},
	{ITT_EFUNC, "ADD MARK :", SCControlConfig, 74},
	{ITT_EFUNC, "CLEAR MARKS :", SCControlConfig, 75},

	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, "MISCELLANEOUS", NULL, 0},
	{ITT_EFUNC, "SCREENSHOT :", SCControlConfig, 66},
	{ITT_EFUNC, "PAUSE :", SCControlConfig, 59},
	{ITT_EFUNC, "CHAT :", SCControlConfig, 61},
	{ITT_EFUNC, "GREEN CHAT :", SCControlConfig, 62},
	{ITT_EFUNC, "YELLOW CHAT :", SCControlConfig, 63},
	{ITT_EFUNC, "RED CHAT :", SCControlConfig, 64},
	{ITT_EFUNC, "BLUE CHAT :", SCControlConfig, 65},
	{ITT_EFUNC, "STOP DEMO :", SCControlConfig, A_STOPDEMO}
};

#endif
