/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * con_bind.c: Event/Command Binding
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
	event_t	event;
	int		flags;
	char	*command;
} binding_t;

typedef struct {
	int		key;	// DDKEY
	char	*name;
} keyname_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

binding_t	*binds = NULL;
int			numBinds = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static keyname_t keyNames[] =
{
	{ DDKEY_PAUSE,		"pause" },
	{ DDKEY_ESCAPE,		"escape" },
	{ DDKEY_ESCAPE,		"esc" },
	{ DDKEY_RIGHTARROW,	"right" },
	{ DDKEY_LEFTARROW,	"left" },
	{ DDKEY_UPARROW,	"up" },
	{ DDKEY_DOWNARROW,	"down" },
	{ DDKEY_ENTER,		"enter" },
	{ DDKEY_TAB,		"tab" },
	{ DDKEY_RSHIFT,		"shift" },
	{ DDKEY_RCTRL,		"ctrl" },
	{ DDKEY_RALT,		"alt" },
	{ DDKEY_INS,		"ins" },
	{ DDKEY_DEL,		"del" },
	{ DDKEY_PGUP,		"pgup" },
	{ DDKEY_PGDN,		"pgdown" },
	{ DDKEY_PGDN,		"pgdn" },
	{ DDKEY_HOME,		"home" },
	{ DDKEY_END,		"end" },
	{ DDKEY_BACKSPACE,	"bkspc" },
	{ ' ',				"space" },
	{ ';',				"smcln" },
	{ '\"',				"quote" },
	{ DDKEY_F10,		"f10" },
	{ DDKEY_F11,		"f11" },
	{ DDKEY_F12,		"f12" },
	{ DDKEY_F1,			"f1" },
	{ DDKEY_F2,			"f2" },
	{ DDKEY_F3,			"f3" },
	{ DDKEY_F4,			"f4" },
	{ DDKEY_F5,			"f5" },
	{ DDKEY_F6,			"f6" },
	{ DDKEY_F7,			"f7" },
	{ DDKEY_F8,			"f8" },
	{ DDKEY_F9,			"f9" },
	{ '`',				"tilde" },
	{ DDKEY_NUMLOCK,	"numlock" },
	{ DDKEY_SCROLL,		"scrlock" },
	{ DDKEY_NUMPAD0,	"pad0" },
	{ DDKEY_NUMPAD1,	"pad1" },
	{ DDKEY_NUMPAD2,	"pad2" },
	{ DDKEY_NUMPAD3,	"pad3" },
	{ DDKEY_NUMPAD4,	"pad4" },
	{ DDKEY_NUMPAD5,	"pad5" },
	{ DDKEY_NUMPAD6,	"pad6" },
	{ DDKEY_NUMPAD7,	"pad7" },
	{ DDKEY_NUMPAD8,	"pad8" },
	{ DDKEY_NUMPAD9,	"pad9" },
	{ DDKEY_DECIMAL,	"pad," },
	{ DDKEY_SUBTRACT,	"pad-" }, // not really used 
	{ DDKEY_ADD,		"pad+" }, // not really used
	
	{ 0, NULL }// The terminator
};

static char *povDirNames[] = 
{
	"F", "FR", "R", "BR", "B", "BL", "L", "FL", NULL
};

// CODE --------------------------------------------------------------------

/*
 * B_EventMatch
 */
static boolean B_EventMatch(event_t *ev, event_t *bev)
{
	// Check the type.
	if(ev->type != bev->type) return false;
		
	switch(ev->type)
	{
	case ev_keydown:
	case ev_keyrepeat:
	case ev_keyup:
	case ev_povdown:
	case ev_povup:
		return (ev->data1 == bev->data1);

	case ev_mousebdown:
	case ev_mousebup:
	case ev_joybdown:
	case ev_joybup:
		return ((ev->data1 & bev->data1) != 0);

	default:
		// We don't know what to compare... :-]
		break;
	}
	return false;
}

/*
 * B_Responder
 */
boolean B_Responder(event_t *ev)
{
	binding_t	*bnd;
	int			i;

	// We won't even bother with axis data.
	if(ev->type == ev_mouse || ev->type == ev_joystick) return false;
	
	// Check all the bindings and execute the necessary commands.
	for(i = 0, bnd = binds; i < numBinds; i++, bnd++)
	{
		// Do we need to execute the command?
		if(B_EventMatch(ev, &bnd->event))
		{
			Con_Execute(bnd->command, true);
			// Only one binding per event, unless mouse or joystick buttons
			// are involved. Several bindings may match their button masks,
			// you see.
			/* if(ev->type != ev_mousebdown && ev->type != ev_mousebup 
				&& ev->type != ev_joybdown && ev->type != ev_joybup)
				return true;
			*/
		}
	}
	return false;
}

/*
 * B_GetBinding
 *	Returns a binding_t for the given event.
 */
binding_t *B_GetBinding(event_t *event, boolean create_new)
{
	int			i;
	binding_t	*newb;
	
	// We'll first have to search through the existing bindings
	// to see if there already is one for this event.
	for(i=0; i<numBinds; i++)
		if(B_EventMatch(event, &binds[i].event))
			return binds + i;
	if(!create_new) return NULL;

	// Hmm, no luck there. Let's create a new binding_t.
	binds = realloc(binds, sizeof(binding_t) * (numBinds+1));
	newb = binds + numBinds++;	
	memset(newb, 0, sizeof(*newb));
	// Copy the event data.
	memcpy(&newb->event, event, sizeof(*event));
	return newb;
}

/*
 * B_DeleteBindingIdx
 */
void B_DeleteBindingIdx(int index)
{
	if(index < 0 || index > numBinds-1) return;	// What?

	free(binds[index].command);
	if(index < numBinds-1) // If not the last one, do some rollback.
	{
		memmove(binds+index, binds+index+1,
				sizeof(binding_t) * (numBinds-index-1));
	}
	binds = realloc(binds, sizeof(binding_t)* --numBinds);
}

/*
 * B_Bind
 *	Binds the given event to the command.
 *	Rebinds old bindings.
 *	Binding to NULL will delete the binding.
 */
void B_Bind(event_t *event, char *command)
{
	binding_t *bnd = B_GetBinding(event, true);

	if(!command)		// No string given?
	{
		B_DeleteBindingIdx(bnd - binds);
		return;
	}
	// Set the command.
	bnd->command = realloc(bnd->command, strlen(command)+1);
	strcpy(bnd->command, command);

//	Con_Printf( "B_Bind: evtype:%d data:%d cmd:%s\n", bnd->event.type,
//	bnd->event.data1, bnd->command);
}

/*
 * B_ClearBinding
 */
void B_ClearBinding(char *command)
{
	int i;

	for(i = 0; i < numBinds; i++)
		if(!stricmp(binds[i].command, command))
			B_DeleteBindingIdx(i--);
}

/*
 * B_Shutdown
 *	Deallocates the memory for the commands and bindings.
 */
void B_Shutdown()
{
	int		i;

	for(i=0; i<numBinds; i++) free(binds[i].command);
	free(binds);
	binds = NULL;
	numBinds = 0;
}

/*
 * shortNameForKey
 *	If buff is "" upon returning, the key is not valid for controls.
 */
static char *shortNameForKey(int ddkey)
{
	int		i;

	for(i=0; keyNames[i].key; i++)
		if(ddkey == keyNames[i].key) return keyNames[i].name;
	return NULL;
}

/*
 * getByShortName
 */
static const int getByShortName(const char *key)
{
	int		i;

	for(i=0; keyNames[i].key; i++)
		if(!strnicmp(key, keyNames[i].name, strlen(keyNames[i].name)))
			return keyNames[i].key;
	return 0;
}

/*
 * buttonNumber
 */
static int buttonNumber(int flags)
{
	int		i;

	for(i = 0; i < 32; i++) if(flags & (1<<i)) return i;
	return -1;
}

/*
 * B_EventBuilder
 *	Converts between events and their textual representations.
 *	Buff and event must be valid sources and destinations.
 */
void B_EventBuilder(char *buff, event_t *ev, boolean to_event)
{
	char	prefix;
	char	*begin;
	int		key;
	
	if(to_event)
	{
		// Convert the text to an event.
		prefix = buff[0];
		begin = buff;
		if(strlen(buff) > 1)
		{
			if(prefix != '+' && prefix != '-' && prefix != '*')
				prefix = '+';	// 'Down' by default.
			begin = buff+1;
		}
		else prefix = '+';
		
		// First check the obvious cases.
		if(!strnicmp(begin, "mb", 2))	// Mouse button?
		{
			ev->type = prefix=='+'? ev_mousebdown : ev_mousebup;
			ev->data1 = 1 << (atoi(begin+2) - 1);
		}
		else if(!strnicmp(begin, "mw", 2)) // Mouse wheel?
		{
			ev->type = prefix=='+'? ev_mousebdown : ev_mousebup;
			ev->data1 = !stricmp(begin+2, "up")? DDMB_MWHEELUP : DDMB_MWHEELDOWN;
		}
		else if(!strnicmp(begin, "jb", 2)) // Joystick button?
		{
			ev->type = prefix=='+'? ev_joybdown : ev_joybup;
			ev->data1 = 1 << (atoi(begin+2) - 1);
		}
		else if(!strnicmp(begin, "pov", 3)) // A POV angle?
		{
			int	i;
			ev->type = prefix=='+'? ev_povdown : ev_povup;
			ev->data1 = -1;
			for(i=0; povDirNames[i]; i++)
				if(!stricmp(begin+3, povDirNames[i]))
				{
					ev->data1 = i;
					break;
				}
		}
		else 
		{
			ev->type = prefix=='+'? ev_keydown : prefix=='*'? ev_keyrepeat : ev_keyup;
			if( (key=getByShortName(begin)) )
				ev->data1 = key;									
			else
				ev->data1 = begin[0];
		}
	}
	else
	{
		// Convert the event to text.
		switch(ev->type)
		{
		case ev_keydown:
		case ev_keyrepeat:
		case ev_keyup:
			// Choose the right prefix.
			prefix = ev->type==ev_keydown? '+' : ev->type==ev_keyup? '-' : '*';
			if( (begin = shortNameForKey(ev->data1)) )
			{
				sprintf(buff, "%c%s", prefix, begin);
			}
			else if(ev->data1 > 32 && ev->data1 < 128)
			{
				sprintf(buff, "%c%c", prefix, ev->data1);
			}
			break;

		case ev_mousebdown:
		case ev_mousebup:
			prefix = ev->type==ev_mousebdown? '+' : '-';
			if(ev->data1 & (DDMB_MWHEELUP | DDMB_MWHEELDOWN))
				sprintf(buff, "%cMW%s", prefix, ev->data1 & DDMB_MWHEELUP? "up" : "down");
			else
				sprintf(buff, "%cMB%d", prefix, buttonNumber(ev->data1)+1);
			break;

		case ev_joybdown:
		case ev_joybup:
			prefix = ev->type==ev_joybdown? '+' : '-';
			sprintf(buff, "%cJB%d", prefix, buttonNumber(ev->data1)+1);
			break;

		case ev_povdown:
		case ev_povup:
			prefix = ev->type==ev_povdown? '+' : '-';
			sprintf(buff, "%cPOV%s", prefix, povDirNames[ev->data1]);
			break;

		default:
			Con_Error("B_EventBuilder (->text): bad event type (%d).\n", ev->type);
		}
	}
}

/*
 * CCmdBind
 */
int CCmdBind(int argc, char **argv)
{
	boolean prefixGiven = true;
	char validEventName[16], buff[80];
	char prefix = '+', *begin;
	event_t event;
	int repeat = !stricmp(argv[0], "bindr") || !stricmp(argv[0], "safebindr");
	int safe = !strnicmp(argv[0], "safe", 4);

	if(argc < 2 || argc > 3)
	{
		Con_Printf( "Usage: %s (event) (cmd)\n", argv[0]);
		return true;
	}
	begin = argv[1];
	if(strlen(argv[1]) > 1) // Can the event have a prefix?
	{
		prefix = argv[1][0];
		begin = argv[1] + 1;
		if(prefix != '+' && prefix != '-' && prefix != '*')
		{
			begin = argv[1];
			prefix = '+';
			prefixGiven = false;
		}
	}
	else prefixGiven = false;
	if(argc == 2 && !prefixGiven)
	{
		char prefixes[3] = { '+', '-', '*' };
		int i;
		// We're clearing a binding. If no prefix has been given,
		// +, - and * are all cleared.
		for(i=0; i<3; i++)
		{
			sprintf(buff, "%c%s", prefixes[i], argv[1]);
			B_EventBuilder(buff, &event, true);	// Convert it to an event_t
			B_Bind(&event, NULL);		
		}
		return true;
	}
	if(argc == 3)
	{
		char cprefix = argv[2][0];
		if(cprefix != '+' && cprefix != '-' && begin == argv[1])
		{
			// Bind both the + and -.
			sprintf(validEventName, "-%s", argv[1]);
			sprintf(buff, "-%s", argv[2]);
			if(Con_ActionCommand(buff, true))
			{
				B_EventBuilder(validEventName, &event, true);
				if(safe && B_GetBinding(&event, false)) return false;
				B_Bind(&event, buff);
				sprintf(validEventName, "+%s", argv[1]);
				sprintf(buff, "+%s", argv[2]);
				B_EventBuilder(validEventName, &event, true);
				B_Bind(&event, buff);
				return true;
			}
		}
	}
	sprintf(validEventName, "%c%s", prefix, begin);	
	
	//Con_Printf( "Binding %s : %s.\n", validEventName, argc==2? "(nothing)" : argv[2]);
	
	// Convert the name to an event.
	B_EventBuilder(validEventName, &event, true);
	if(safe && B_GetBinding(&event, false)) return false;
	// Now we can create a binding for it.
	B_Bind(&event, argc==2? NULL : argv[2]);

	// A repeater?
	if(repeat && event.type == ev_keydown)
	{
		event.type = ev_keyrepeat;
		B_Bind(&event, argc==2? NULL : argv[2]);
	}
	return true;
}

int CCmdClearBindings(int argc, char **argv)
{
	B_Shutdown();
	Con_Printf( "All bindings cleared.\n");
	return true;
}

int CCmdListBindings(int argc, char **argv)
{
	int		i;
	char	buffer[20];

	for(i=0; i<numBinds; i++)
	{
		B_EventBuilder(buffer, &binds[i].event, false);
		if(argc >= 2)
		{
			if(strnicmp(buffer+1, argv[1], strlen(argv[1])))
				continue; // Doesn't match the search pattern.
		}
		Con_Printf( "%-8s : %s\n", buffer, binds[i].command);
	}
	Con_Printf( "There are %d bindings.\n", numBinds);
	return true;
}

void B_WriteToFile(FILE *file)
{
	int i;
	char buffer[20];

	for(i = 0; i < numBinds; i++)
	{
		B_EventBuilder(buffer, &binds[i].event, false);
		fprintf(file, "bind ");
		fprintf(file, "%s", buffer);
		fprintf(file, " \"");
		M_WriteTextEsc(file, binds[i].command);
		fprintf(file, "\"\n");
	}
}

/*
 * B_BindingsForCommand
 *	Returns the number of bindings. The buffer will be filled with the
 *	names of the events.
 */
int B_BindingsForCommand(char *command, char *buffer)
{
	int		i, count = 0;
	char	bindname[20];
	
	strcpy(buffer, "");
	for(i=0; i<numBinds; i++)
	{
		if(!stricmp(command, binds[i].command))
		{
			B_EventBuilder(bindname, &binds[i].event, false);
			if(buffer[0])	// If the buffer is not empty...
				strcat(buffer, " ");
			strcat(buffer, bindname);
			count++;
		}
	}
	return count;
}

/*
 * Return the key code that corresponds the given key identifier name.
 * This is a public function.
 */
int DD_GetKeyCode(const char *key)
{
	int code = getByShortName(key);
	return code? code : key[0];
}

