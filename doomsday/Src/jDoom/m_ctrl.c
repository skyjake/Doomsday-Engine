// JDoom Controls Menu.

#include "doomdef.h"
#include "doomstat.h"
#include "m_menu.h"
#include "Mn_def.h"
#include "D_Action.h"
#include "hu_stuff.h"
#include "s_sound.h"
#include "g_game.h"

#ifdef __JDOOM__
#	define CTLCFG_TYPE void
#else
#	define CTLCFG_TYPE static boolean
#endif

CTLCFG_TYPE SCControlConfig(int option);

void M_DrawControlsMenu(void);

// Control flags.
#define CLF_ACTION		0x1		// The control is an action (+/- in front).
#define CLF_REPEAT		0x2		// Bind down + repeat.

typedef struct
{
	char		*command;		// The command to execute.
	int			flags;
	int			defKey;			// 
	int			defMouse;		// Zero means there is no default.
	int			defJoy;			//
} Control_t;

//
// !!! Add new controls to the end, the existing indices must remain unchanged !!!
//
static Control_t controls[] =
{
	// Actions (must be first so the A_* constants can be used).
	"left",			CLF_ACTION,		DDKEY_LEFTARROW, 0, 0,
	"right",		CLF_ACTION,		DDKEY_RIGHTARROW, 0, 0,
	"forward",		CLF_ACTION,		DDKEY_UPARROW, 0, 0,
	"backward",		CLF_ACTION,		DDKEY_DOWNARROW, 0, 0,
	"strafel",		CLF_ACTION,		',', 0, 0,
	"strafer",		CLF_ACTION,		'.', 0, 0,
	"fire",			CLF_ACTION,		DDKEY_RCTRL, 1, 1,
	"use",			CLF_ACTION,		' ', 0, 4,
	"strafe",		CLF_ACTION,		DDKEY_RALT, 3, 2,
	"speed",		CLF_ACTION,		DDKEY_RSHIFT, 0, 3,

	"weap1",		CLF_ACTION,		0, 0, 0,
	"weapon2",		CLF_ACTION,		'2', 0, 0,
	"weap3",		CLF_ACTION,		0, 0, 0,
	"weapon4",		CLF_ACTION,		'4', 0, 0,
	"weapon5",		CLF_ACTION,		'5', 0, 0,
	"weapon6",		CLF_ACTION,		'6', 0, 0,
	"weapon7",		CLF_ACTION,		'7', 0, 0,
	"weapon8",		CLF_ACTION,		'8', 0, 0,
	"weapon9",		CLF_ACTION,		'9', 0, 0,
	"nextwpn",		CLF_ACTION,		0, 0, 0,

	"prevwpn",		CLF_ACTION,		0, 0, 0,
	"mlook",		CLF_ACTION,		'm', 0, 0,
	"jlook",		CLF_ACTION,		'j', 0, 0,
	"lookup",		CLF_ACTION,		DDKEY_PGDN, 0, 6,
	"lookdown",		CLF_ACTION,		DDKEY_DEL, 0, 7,
	"lookcntr",		CLF_ACTION,		DDKEY_END, 0, 0,
	"jump",			CLF_ACTION,		0, 0, 0,
	"demostop",		CLF_ACTION,		'o', 0, 0,

	// Menu hotkeys (default: F1 - F12).
/*28*/"HelpScreen",	0,				DDKEY_F1, 0, 0,
	"SaveGame",		0,				DDKEY_F2, 0, 0,

	"LoadGame",		0,				DDKEY_F3, 0, 0,
	"SoundMenu",	0,				DDKEY_F4, 0, 0,
	"QuickSave",	0,				DDKEY_F6, 0, 0,
	"EndGame",		0,				DDKEY_F7, 0, 0,
	"ToggleMsgs",	0,				DDKEY_F8, 0, 0,
	"QuickLoad",	0,				DDKEY_F9, 0, 0,
	"quit",			0,				DDKEY_F10, 0, 0,
	"ToggleGamma",	0,				DDKEY_F11, 0, 0,
	"spy",			0,				DDKEY_F12, 0, 0,

	// Screen controls.
	"viewsize -",	CLF_REPEAT,		'-', 0, 0,
	"viewsize +",	CLF_REPEAT,		'=', 0, 0,
	"sbsize -",		CLF_REPEAT,		0, 0, 0,
	"sbsize +",		CLF_REPEAT,		0, 0, 0,

	// Misc.
	"pause",		0,				DDKEY_PAUSE, 0, 0,
	"screenshot",	0,				0, 0, 0,
	"beginchat",	0,				't', 0, 0,
	"beginchat 0",	0,				'g', 0, 0,
	"beginchat 1",	0,				'i', 0, 0,
	"beginchat 2",	0,				'b', 0, 0,
	"beginchat 3",	0,				'r', 0, 0,
	"msgrefresh",	0,				DDKEY_ENTER, 0, 0,

	// More weapons.
	"weapon1",		CLF_ACTION,		'1', 0, 0,	
	"weapon3",		CLF_ACTION,		'3', 0, 0,

	"automap",		0,				DDKEY_TAB, 0, 0,

	"",				0,				0, 0, 0		// terminator
};
static Control_t *grabbing = NULL;


static MenuItem_t ControlsItems[] =
{
	{ ITT_EMPTY, "PLAYER ACTIONS", NULL, 0 },
	{ ITT_EFUNC, "LEFT :", SCControlConfig, A_TURNLEFT },
	{ ITT_EFUNC, "RIGHT :", SCControlConfig, A_TURNRIGHT },
	{ ITT_EFUNC, "FORWARD :", SCControlConfig, A_FORWARD },
	{ ITT_EFUNC, "BACKWARD :", SCControlConfig, A_BACKWARD },
	{ ITT_EFUNC, "STRAFE LEFT :", SCControlConfig, A_STRAFELEFT },
	{ ITT_EFUNC, "STRAFE RIGHT :", SCControlConfig, A_STRAFERIGHT },
	{ ITT_EFUNC, "FIRE :", SCControlConfig, A_FIRE },
	{ ITT_EFUNC, "USE :", SCControlConfig, A_USE },	
	{ ITT_EFUNC, "JUMP : ", SCControlConfig, A_JUMP },
	{ ITT_EFUNC, "STRAFE :", SCControlConfig, A_STRAFE },
	{ ITT_EFUNC, "SPEED :", SCControlConfig, A_SPEED },
	{ ITT_EFUNC, "LOOK UP :", SCControlConfig, A_LOOKUP },
	{ ITT_EFUNC, "LOOK DOWN :", SCControlConfig, A_LOOKDOWN },
	{ ITT_EFUNC, "LOOK CENTER :", SCControlConfig, A_LOOKCENTER },
	{ ITT_EFUNC, "MOUSE LOOK :", SCControlConfig, A_MLOOK },
	{ ITT_EFUNC, "JOYSTICK LOOK :", SCControlConfig, A_JLOOK },
	{ ITT_EFUNC, "NEXT WEAPON :", SCControlConfig, A_NEXTWEAPON },
	{ ITT_EFUNC, "PREV WEAPON :", SCControlConfig, A_PREVIOUSWEAPON },
	{ ITT_EFUNC, "FIST/CHAINSAW :", SCControlConfig, 51 },
	{ ITT_EFUNC, "FIST :", SCControlConfig, A_WEAPON1 },
	{ ITT_EFUNC, "CHAINSAW :", SCControlConfig, A_WEAPON8 },
	{ ITT_EFUNC, "PISTOL :", SCControlConfig, A_WEAPON2 },
	{ ITT_EFUNC, "SUPER SG/SHOTGUN :", SCControlConfig, 52 },
	{ ITT_EFUNC, "SHOTGUN :", SCControlConfig, A_WEAPON3 },
	{ ITT_EFUNC, "SUPER SHOTGUN :", SCControlConfig, A_WEAPON9 },
	{ ITT_EFUNC, "CHAINGUN :", SCControlConfig, A_WEAPON4 },
	{ ITT_EFUNC, "ROCKET LAUNCHER :", SCControlConfig, A_WEAPON5 },
	{ ITT_EFUNC, "PLASMA RIFLE :", SCControlConfig, A_WEAPON6 },
	{ ITT_EFUNC, "BFG 9000 :", SCControlConfig, A_WEAPON7 },
	{ ITT_EMPTY, NULL, NULL, 0 },
	{ ITT_EMPTY, NULL, NULL, 0 },
	{ ITT_EMPTY, "MENU HOTKEYS", NULL, 0 },
	{ ITT_EFUNC, "HELP :", SCControlConfig, 28 },
	{ ITT_EFUNC, "SOUND MENU :", SCControlConfig, 31 },
	{ ITT_EFUNC, "LOAD GAME :", SCControlConfig, 30 },
	{ ITT_EFUNC, "SAVE GAME :", SCControlConfig, 29 },
	{ ITT_EFUNC, "QUICK LOAD :", SCControlConfig, 35 },
	{ ITT_EFUNC, "QUICK SAVE :", SCControlConfig, 32 },
	{ ITT_EFUNC, "END GAME :", SCControlConfig, 33 },
	{ ITT_EFUNC, "QUIT :", SCControlConfig, 36 },
	{ ITT_EFUNC, "MESSAGES ON/OFF:", SCControlConfig, 34 },
	{ ITT_EFUNC, "GAMMA CORRECTION :", SCControlConfig, 37 },
	{ ITT_EFUNC, "SPY MODE :", SCControlConfig, 38 },
	{ ITT_EMPTY, NULL, NULL, 0 },
	{ ITT_EMPTY, "SCREEN", NULL, 0 },
	{ ITT_EFUNC, "SMALLER VIEW :", SCControlConfig, 39 },
	{ ITT_EFUNC, "LARGER VIEW :", SCControlConfig, 40 },
	{ ITT_EFUNC, "SMALLER STATBAR :", SCControlConfig, 41 },
	{ ITT_EFUNC, "LARGER STATBAR :", SCControlConfig, 42 },
	{ ITT_EMPTY, NULL, NULL, 0 },
	{ ITT_EMPTY, "MISCELLANEOUS", NULL, 0 },
	{ ITT_EFUNC, "AUTOMAP :", SCControlConfig, 53 },
	{ ITT_EFUNC, "PAUSE :", SCControlConfig, 43 },
	{ ITT_EFUNC, "SCREENSHOT :", SCControlConfig, 44 },
	{ ITT_EFUNC, "CHAT :", SCControlConfig, 45 },
	{ ITT_EFUNC, "GREEN CHAT :", SCControlConfig, 46 },
	{ ITT_EFUNC, "INDIGO CHAT :", SCControlConfig, 47 },
	{ ITT_EFUNC, "BROWN CHAT :", SCControlConfig, 48 },
	{ ITT_EFUNC, "RED CHAT :", SCControlConfig, 49 },
	{ ITT_EFUNC, "MSG REFRESH :", SCControlConfig, 50 }
};

Menu_t ControlsDef =
{
	32, 40,
	M_DrawControlsMenu,
	61, ControlsItems,
	1, MENU_OPTIONS,
	hu_font_a, //1, 0, 0, 
	LINEHEIGHT_A,
	0, 16
};

CTLCFG_TYPE SCControlConfig(int option)
{
	grabbing = controls + option;
#ifndef __JDOOM__
	return true;
#endif
}

void spacecat(char *str, const char *catstr)
{
	if(str[0]) strcat(str, " ");

	if(!stricmp(catstr, "smcln")) catstr = ";";

	strcat(str, catstr);
}

void M_DrawControlsMenu(void)
{
	int			i;
	char		controlCmd[80];
	char		buff[80], prbuff[80], *token;
	Menu_t		*menu = &ControlsDef;
	MenuItem_t	*item = menu->items + menu->firstItem;
	Control_t	*ctrl;

	M_DrawTitle("CONTROLS", menu->y-28);
	sprintf(buff, "PAGE %i/%i", menu->firstItem/menu->numVisItems+1,
		menu->itemCount/menu->numVisItems+1);
	M_WriteText2(160-M_StringWidth(buff, hu_font_a)/2, 
		menu->y-12, buff, hu_font_a, 1, .7f, .3f);

	for(i = 0; i < menu->numVisItems && menu->firstItem + i < menu->itemCount; 
		i++, item++)
	{
		if(item->type == ITT_EMPTY) continue;
		
		ctrl = controls + item->option;
		strcpy(buff, "");
		if(ctrl->flags & CLF_ACTION)
			sprintf(controlCmd, "+%s", ctrl->command);
		else
			strcpy(controlCmd, ctrl->command);
		// Let's gather all the bindings for this command.
		if(!B_BindingsForCommand(controlCmd, buff))
			strcpy(buff, "NONE");
	
		// Now we must interpret what the bindings string says.
		// It may contain characters we can't print.
		strcpy(prbuff, "");
		token = strtok(buff, " ");
		while(token)
		{
			if(token[0] == '+')
			{
				spacecat(prbuff, token+1);
			}
			if((token[0] == '*' && !(ctrl->flags & CLF_REPEAT)) ||
			   token[0] == '-')
			{
				spacecat(prbuff, token);
			}
			token = strtok(NULL, " ");
		}
		strupr(prbuff);

		if(grabbing == ctrl)
		{
			// We're grabbing for this control.
			spacecat(prbuff, "...");
		}

		M_WriteText2(menu->x+134, menu->y + i*menu->itemHeight, 
			prbuff, hu_font_a, 1, 1, 1);
	}
}

// Set default bindings for unbound Controls.
void D_DefaultBindings()
{
	int			i;
	Control_t	*ctr;
	char		evname[80], cmd[256], buff[256];
	event_t		event;
	
	// Check all Controls.
	for(i = 0; controls[i].command[0]; i++)
	{
		ctr = controls + i;
		// If this command is bound to something, skip it.
		sprintf(cmd, "%s%s", ctr->flags & CLF_ACTION? "+" : "",
			ctr->command);
		if(B_BindingsForCommand(cmd, buff)) continue;

		// This Control has no bindings, set it to the default.
		sprintf(buff, "\"%s\"", ctr->command);
		if(ctr->defKey)
		{
			event.type = ev_keydown;
			event.data1 = ctr->defKey;
			B_EventBuilder(evname, &event, false);
			sprintf(cmd, "%s %s %s", ctr->flags & CLF_REPEAT? "safebindr" : "safebind",
				evname+1, buff);
			Con_Execute(cmd, true);
		}
		if(ctr->defMouse)
		{
			event.type = ev_mousebdown;
			event.data1 = 1 << (ctr->defMouse-1);
			B_EventBuilder(evname, &event, false);
			sprintf(cmd, "%s %s %s", ctr->flags & CLF_REPEAT? "safebindr" : "safebind",
				evname+1, buff);
			Con_Execute(cmd, true);
		}
		if(ctr->defJoy)
		{
			event.type = ev_joybdown;
			event.data1 = 1 << (ctr->defJoy-1);
			B_EventBuilder(evname, &event, false);
			sprintf(cmd, "%s %s %s", ctr->flags & CLF_REPEAT? "safebindr" : "safebind",
				evname+1, buff);
			Con_Execute(cmd, true);
		}
	}
}

//---------------------------------------------------------------------------
//
// FUNC H_PrivilegedResponder
//
//---------------------------------------------------------------------------

int findtoken(char *string, char *token, char *delim)
{
	char *ptr = strtok(string, delim);
	while(ptr)
	{
		if(!stricmp(ptr, token)) return true;
		ptr = strtok(NULL, delim);
	}
	return false;
}

int D_PrivilegedResponder(event_t *event)
{
	// We're interested in key or button down events.
	if(grabbing && (event->type == ev_keydown || event->type == ev_mousebdown 
		|| event->type == ev_joybdown || event->type == ev_povdown))
	{
		// We'll grab this event.
		char cmd[256], buff[256], evname[80];
		boolean del = false;

		// Check for a cancel.
		if(event->type == ev_keydown)
		{
			if(event->data1 == DDKEY_ESCAPE)
			{
				grabbing = NULL;
				return true;
			}
		}

		// We shall issue a silent console command, but first we need
		// a textual representation of the event.
		B_EventBuilder(evname, event, false); // "Deconstruct" into a name.

		// If this binding already exists, remove it.
		sprintf(cmd, "%s%s", grabbing->flags & CLF_ACTION? "+" : "",
			grabbing->command);
		if(B_BindingsForCommand(cmd, buff))
			if(findtoken(buff, evname, " "))		// Get rid of it?
			{
				del = true;
				strcpy(buff, "");
			}
		if(!del) sprintf(buff, "\"%s\"", grabbing->command);
		sprintf(cmd, "%s %s %s", 
			grabbing->flags & CLF_REPEAT? "bindr" : "bind",
			evname + 1, buff);
		Con_Execute(cmd, false);
		// We've finished the grab.
		grabbing = NULL;
		S_LocalSound(sfx_pistol, NULL);
		return true;
	}

	// Process the screen shot key right away.
	if(devparm && event->data1 == DDKEY_F1)
	{
		if(event->type == ev_keydown) G_ScreenShot();
		// All F1 events are eaten.
		return true;
	}
	return false;
}

