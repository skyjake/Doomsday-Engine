/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <assert.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    event_t event;
    int     flags;
    char   *command[NUMBINDCLASSES];
} binding_t;

typedef struct {
    int     key;                // DDKEY
    char   *name;
} keyname_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern action_t *ddactions;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

binding_t *binds = NULL;
int     numBinds = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static keyname_t keyNames[] = {
    {DDKEY_PAUSE, "pause"},
    {DDKEY_ESCAPE, "escape"},
    {DDKEY_ESCAPE, "esc"},
    {DDKEY_RIGHTARROW, "right"},
    {DDKEY_LEFTARROW, "left"},
    {DDKEY_UPARROW, "up"},
    {DDKEY_DOWNARROW, "down"},
    {DDKEY_ENTER, "enter"},
    {DDKEY_TAB, "tab"},
    {DDKEY_RSHIFT, "shift"},
    {DDKEY_RCTRL, "ctrl"},
    {DDKEY_RALT, "alt"},
    {DDKEY_INS, "ins"},
    {DDKEY_DEL, "del"},
    {DDKEY_PGUP, "pgup"},
    {DDKEY_PGDN, "pgdown"},
    {DDKEY_PGDN, "pgdn"},
    {DDKEY_HOME, "home"},
    {DDKEY_END, "end"},
    {DDKEY_BACKSPACE, "bkspc"},
    {' ', "space"},
    {';', "smcln"},
    {'\"', "quote"},
    {DDKEY_F10, "f10"},
    {DDKEY_F11, "f11"},
    {DDKEY_F12, "f12"},
    {DDKEY_F1, "f1"},
    {DDKEY_F2, "f2"},
    {DDKEY_F3, "f3"},
    {DDKEY_F4, "f4"},
    {DDKEY_F5, "f5"},
    {DDKEY_F6, "f6"},
    {DDKEY_F7, "f7"},
    {DDKEY_F8, "f8"},
    {DDKEY_F9, "f9"},
    {'`', "tilde"},
    {DDKEY_NUMLOCK, "numlock"},
    {DDKEY_SCROLL, "scrlock"},
    {DDKEY_NUMPAD0, "pad0"},
    {DDKEY_NUMPAD1, "pad1"},
    {DDKEY_NUMPAD2, "pad2"},
    {DDKEY_NUMPAD3, "pad3"},
    {DDKEY_NUMPAD4, "pad4"},
    {DDKEY_NUMPAD5, "pad5"},
    {DDKEY_NUMPAD6, "pad6"},
    {DDKEY_NUMPAD7, "pad7"},
    {DDKEY_NUMPAD8, "pad8"},
    {DDKEY_NUMPAD9, "pad9"},
    {DDKEY_DECIMAL, "pad,"},
    {DDKEY_SUBTRACT, "pad-"},    // not really used 
    {DDKEY_ADD, "pad+"},        // not really used

    {0, NULL}                    // The terminator
};

static char *povDirNames[] = {
    "F", "FR", "R", "BR", "B", "BL", "L", "FL", NULL
};

// DJS 13/05/05
// Binding Classes
//       (very handy for gamepads with a limited number of buttons ;-))

// Idealy it should be possible to create/destroy binding classes dynamically
// at runtime using a console command. The game would then register any
// classes it NEEDS on init (and mark them as indestructable). Users would
// also be allowed to create any additional binding classes they required.
// However that would mean a fair amount of extra book keeping, so for
// now we have a static array and the names of the classes are set here.

// Bindings are saved with the classnames eg:
//   bind game +w +forward

// However, omission of the class name defaults the bind to class 0 (id 1)
// and the bindings in the cfg will be updated with the missing class names
// automatically on exit (for reading old cfg files).

// When a binding class is enabled/disabled we loop through the bindings
// looking for any that are bound to any keys/buttons being pressed at that time.
// If any are found we que extra up events that request a command in a specific
// binding class. Due to binding classes being ordered numericaly with the
// rule that only the command in the highest active binding class being executed
// we only need to check commands for bindings with a lower binding class id.

bindclass_t bindClasses[] = {
    {BDC_NORMAL, 1, "game"},
    {BDC_BIASEDITOR, 0, "biaseditor"},
    {BDC_CLASS1, 0, "map"},
    {BDC_CLASS2, 0, "mapfollowoff"}, // additonal classes that can be purposed by users
    {BDC_CLASS3, 0, "class1"},
    {BDC_CLASS4, 0, "class2"},
    {BDC_CLASS5, 0, "class3"},
    {0, 0, NULL},
};

// CODE --------------------------------------------------------------------

/*
 * B_EventMatch
 */
static boolean B_EventMatch(event_t *ev, event_t *bev)
{
    // Check the type.
    if(ev->type != bev->type)
        return false;

    switch (ev->type)
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
    binding_t *bnd;
    int     i, k;

    // We won't even bother with axis data.
    if(ev->type == ev_mouse || ev->type == ev_joystick)
        return false;

    // Check all the bindings and execute the necessary commands.
    for(i = 0, bnd = binds; i < numBinds; i++, bnd++)
    {
        // Do we need to execute a command?
        if(B_EventMatch(ev, &bnd->event))
        {
            if(ev->useclass != -1) // use a specific class? (regardless if it is active or not)
            {
                // FYI: These kind of events aren't sent via direct user input
                // Only by "us" when we need to switch binding classes and a 
                // current input is active eg; a key is held down during the 
                // switch that has commands in multiple binding classes.
                if(bnd->command[ev->useclass])
                {
                     //Con_Message("forced %s\n",bnd->command[ev->useclass]);
                    Con_Execute(bnd->command[ev->useclass], true);
                }
            } 
            else 
            {
                // loop backwards through the active binding classes,
                // the command in the highest binding class slot that
                // is currently active is executed
                for(k = NUMBINDCLASSES; k >= BDC_NORMAL; k--)
                    if(bindClasses[k].active && bnd->command[k])
                    {
                         //Con_Message("%s\n",bnd->command[k]);
                        Con_Execute(bnd->command[k], true);
                        break;
                    }
            }
        }
    }
    return false;
}

/*
 * B_GetBinding
 *  Returns a binding_t for the given event.
 */
binding_t *B_GetBinding(event_t *event, boolean create_new)
{
    int     i;
    binding_t *newb;

    // We'll first have to search through the existing bindings
    // to see if there already is one for this event.
    for(i = 0; i < numBinds; i++)
        if(B_EventMatch(event, &binds[i].event))
                return binds + i;
    if(!create_new)
        return NULL;

    // Hmm, no luck there. Let's create a new binding_t.
    binds = realloc(binds, sizeof(binding_t) * (numBinds + 1));
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
    int i;

    if(index < 0 || index > numBinds - 1)
        return;                    // What?

    for(i = BDC_NORMAL; i < NUMBINDCLASSES; i++)
        if(binds[index].command[i])
            free(binds[index].command[i]);

    if(index < numBinds - 1)    // If not the last one, do some rollback.
    {
        memmove(binds + index, binds + index + 1,
                sizeof(binding_t) * (numBinds - index - 1));
    }
    binds = realloc(binds, sizeof(binding_t) * --numBinds);
}

/*
 * B_Bind
 *  Binds the given event to the command.
 *  Rebinds old bindings.
 *  Binding to NULL without specifying a class deletes the binding.
 *  Bindind to NULL and specifying a class clears the command and
 *  if no more commands exist for this binding - will delete it
 */
void B_Bind(event_t *event, char *command, int bindClass)
{
    int k, count = 0;
    binding_t *bnd = B_GetBinding(event, true);

    if(!command)                // No string given?
    {
        // What about a bind class?
        if(!bindClass)
            B_DeleteBindingIdx(bnd - binds); // so del the binding_t
        else {
            // clear the command in bindClass only
            for(k = BDC_NORMAL; k < NUMBINDCLASSES; k++)
            {
                if(bnd->command[k])
                {
                    count++;
                    if(k == bindClass)
                        bnd->command[k] = NULL;
                }
            }
            if(count == 1)
                // there are no more commands for this binding so del the binding_t
                B_DeleteBindingIdx(bnd - binds);
        }
        return;
    }
    // Set the command.
    bnd->command[bindClass] = realloc(bnd->command[bindClass], 
        strlen(command) + 1);
    strcpy(bnd->command[bindClass], command);

    //  Con_Printf( "B_Bind: evtype:%d data:%d cmd:%s\n", bnd->event.type,
    //  bnd->event.data1, bnd->command[bindclass]);
}

/*
 * B_ClearBinding
 */
void B_ClearBinding(char *command, int bindClass)
{
    int     i, k, count;
    boolean match;

    for(i = 0; i < numBinds; i++)
    {
        match = false;
        count = 0;
        if(bindClass == -1)
        {
            // clear all commands in all binding classes
            for(k = BDC_NORMAL; k < NUMBINDCLASSES; k++)
                if(binds[i].command[k])
                    if(!stricmp(binds[i].command[k], command))
                        B_DeleteBindingIdx(i--);
        } else {
            // clear the command in bindClass only
            for(k = BDC_NORMAL; k < NUMBINDCLASSES; k++)
            {
                if(binds[i].command[k])
                {
                    count++;
                    if((!stricmp(binds[i].command[k], command)) && k == bindClass)
                    {
                        match = true;
                        binds[i].command[k] = NULL;
                    }
                }
            }
            if(match && count == 1)
                // there are no more commands for this binding so del the binding_t
                B_DeleteBindingIdx(i--);
        }
    }
}

/*
 * B_Shutdown
 *  Deallocates the memory for the commands and bindings.
 */
void B_Shutdown()
{
    int     i, k;

    for(i = 0; i < numBinds; i++)
    {
        for(k = BDC_NORMAL; k < NUMBINDCLASSES; k++)
        {
            if(binds[i].command[k])
                free(binds[i].command[k]);
        }
    }
    free(binds);
    binds = NULL;
    numBinds = 0;
}

/*
 * shortNameForKey
 *  If buff is "" upon returning, the key is not valid for controls.
 */
static char *shortNameForKey(int ddkey)
{
    int     i;

    for(i = 0; keyNames[i].key; i++)
        if(ddkey == keyNames[i].key)
            return keyNames[i].name;
    return NULL;
}

/*
 * getByShortName
 */
static int getByShortName(const char *key)
{
    int     i;

    for(i = 0; keyNames[i].key; i++)
        if(!strnicmp(key, keyNames[i].name, strlen(keyNames[i].name)))
            return keyNames[i].key;
    return 0;
}

/*
 * buttonNumber
 */
static int buttonNumber(int flags)
{
    int     i;

    for(i = 0; i < 32; i++)
        if(flags & (1 << i))
            return i;
    return -1;
}

/*
 * B_EventBuilder
 *  Converts between events and their textual representations.
 *  Buff and event must be valid sources and destinations.
 */
void B_EventBuilder(char *buff, event_t *ev, boolean to_event)
{
    char    prefix;
    char   *begin;
    int     key;

    if(to_event)
    {
        // Convert the text to an event.
        prefix = buff[0];
        begin = buff;
        if(strlen(buff) > 1)
        {
            if(prefix != '+' && prefix != '-' && prefix != '*')
                prefix = '+';    // 'Down' by default.
            begin = buff + 1;
        }
        else
            prefix = '+';

        // First check the obvious cases.
        if(!strnicmp(begin, "mb", 2))    // Mouse button?
        {
            ev->type = prefix == '+' ? ev_mousebdown : ev_mousebup;
            ev->data1 = 1 << (atoi(begin + 2) - 1);
        }
        else if(!strnicmp(begin, "mw", 2))    // Mouse wheel?
        {
            ev->type = prefix == '+' ? ev_mousebdown : ev_mousebup;
            ev->data1 =
                !stricmp(begin + 2, "up") ? DDMB_MWHEELUP : DDMB_MWHEELDOWN;
        }
        else if(!strnicmp(begin, "jb", 2))    // Joystick button?
        {
            ev->type = prefix == '+' ? ev_joybdown : ev_joybup;
            ev->data1 = 1 << (atoi(begin + 2) - 1);
        }
        else if(!strnicmp(begin, "pov", 3))    // A POV angle?
        {
            int     i;

            ev->type = prefix == '+' ? ev_povdown : ev_povup;
            ev->data1 = -1;
            for(i = 0; povDirNames[i]; i++)
                if(!stricmp(begin + 3, povDirNames[i]))
                {
                    ev->data1 = i;
                    break;
                }
        }
        else
        {
            ev->type =
                prefix == '+' ? ev_keydown : prefix ==
                '*' ? ev_keyrepeat : ev_keyup;
            if((key = getByShortName(begin)))
                ev->data1 = key;
            else
                ev->data1 = begin[0];
        }
    }
    else
    {
        // Convert the event to text.
        switch (ev->type)
        {
        case ev_keydown:
        case ev_keyrepeat:
        case ev_keyup:
            // Choose the right prefix.
            prefix =
                ev->type == ev_keydown ? '+' : ev->type ==
                ev_keyup ? '-' : '*';
            if((begin = shortNameForKey(ev->data1)))
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
            prefix = ev->type == ev_mousebdown ? '+' : '-';
            if(ev->data1 & (DDMB_MWHEELUP | DDMB_MWHEELDOWN))
                sprintf(buff, "%cMW%s", prefix,
                        ev->data1 & DDMB_MWHEELUP ? "up" : "down");
            else
                sprintf(buff, "%cMB%d", prefix, buttonNumber(ev->data1) + 1);
            break;

        case ev_joybdown:
        case ev_joybup:
            prefix = ev->type == ev_joybdown ? '+' : '-';
            sprintf(buff, "%cJB%d", prefix, buttonNumber(ev->data1) + 1);
            break;

        case ev_povdown:
        case ev_povup:
            prefix = ev->type == ev_povdown ? '+' : '-';
            sprintf(buff, "%cPOV%s", prefix, povDirNames[ev->data1]);
            break;

        default:
            Con_Error("B_EventBuilder (->text): bad event type (%d).\n",
                      ev->type);
        }
    }
}

/*
 * CCmdBind
 */
D_CMD(Bind)
{
    boolean prefixGiven = true;
    boolean bindClassGiven = false;
    char    validEventName[16], buff[80];
    char    prefix = '+', *begin;
    char    *evntptr = argv[2];
    char    *cmdptr = argv[3];
    event_t event;
    int     repeat = !stricmp(argv[0], "bindr") ||
        !stricmp(argv[0], "safebindr");
    int     safe = !strnicmp(argv[0], "safe", 4);
    int        i, bc = -1;
    binding_t *existing;

    if(argc < 2 || argc > 4)
    {
        Con_Printf("Usage: %s (class) (event) (cmd)\n", argv[0]);
        Con_Printf("Binding Classes:\n");
            for(i = BDC_NORMAL; i < NUMBINDCLASSES; i++)
                Con_Printf("  %s\n", bindClasses[i].name);
        return true;
    }

    // Check for a specified binding class
    for(i = BDC_NORMAL; i < NUMBINDCLASSES; i++)
    {
        if(!(stricmp(argv[1],bindClasses[i].name)) ||
            ((!strnicmp(argv[1], "bdc", 3) &&
			(atoi(argv[1]+3) == bindClasses[i].id) )))
        {
            bc = bindClasses[i].id;
            bindClassGiven = true;
            break;
        }
    }

    // Has a binding class been specified?
    if(!bindClassGiven)
    {
        // No it hasn't! default to normal
        bc = BDC_NORMAL;
        evntptr = argv[1];
        cmdptr = argv[2];
    }

    begin = evntptr;
    if(strlen(evntptr) > 1)        // Can the event have a prefix?
    {
        prefix = evntptr[0];
        begin = evntptr + 1;
        if(prefix != '+' && prefix != '-' && prefix != '*')
        {
            begin = evntptr;
            prefix = '+';
            prefixGiven = false;
        }
    }
    else
        prefixGiven = false;

    if((argc == 3 && !prefixGiven && bindClassGiven) ||
        ((argc == 2) && !prefixGiven && !bindClassGiven))
    {
        char    prefixes[3] = { '+', '-', '*' };
        int     i;

        // We're clearing a binding. If no prefix has been given,
        // +, - and * are all cleared.
        for(i = 0; i < 3; i++)
        {
            sprintf(buff, "%c%s", prefixes[i], evntptr);
            B_EventBuilder(buff, &event, true);    // Convert it to an event_t
            B_Bind(&event, NULL, bc);
        }
        return true;
    }
    if(argc == 4 || (argc == 3 && !bindClassGiven))
    {
        char    cprefix = cmdptr[0];

        if(cprefix != '+' && cprefix != '-' && begin == evntptr)
        {
            // Bind both the + and -.
            sprintf(validEventName, "-%s", evntptr);
            sprintf(buff, "-%s", cmdptr);
            if(Con_ActionCommand(buff, true))
            {
                B_EventBuilder(validEventName, &event, true);
                if(safe && (existing = B_GetBinding(&event, false)))
                    if(existing->command[bc])
                        return false;

                B_Bind(&event, buff,bc);
                sprintf(validEventName, "+%s", evntptr);
                sprintf(buff, "+%s", cmdptr);
                B_EventBuilder(validEventName, &event, true);
                B_Bind(&event, buff, bc);
                return true;
            }
        }
    }
    sprintf(validEventName, "%c%s", prefix, begin);

    //Con_Printf( "Binding %s : %s.\n", validEventName, argc==2? "(nothing)" : cmdptr);

    // Convert the name to an event.
    B_EventBuilder(validEventName, &event, true);
    if(safe && (existing = B_GetBinding(&event, false)))
        if(existing->command[bc])
            return false;

    // Now we can create a binding for it.
    if(argc == 2 && !bindClassGiven && !prefixGiven ||
            argc == 3 && bindClassGiven)
        B_Bind(&event, NULL, bc);
    else
        B_Bind(&event, cmdptr, bc);

    // A repeater?
    if(repeat && event.type == ev_keydown)
    {
        event.type = ev_keyrepeat;

        if(argc == 2 && !bindClassGiven && !prefixGiven ||
                argc == 3 && bindClassGiven)
            B_Bind(&event, NULL, bc);
        else
            B_Bind(&event, cmdptr, bc);
    }
    return true;
}

D_CMD(ClearBindings)
{
    B_Shutdown();
    Con_Printf("All bindings cleared.\n");
    return true;
}

D_CMD(DeleteBind)
{
    int     i, bc = -1;
    int     start = 1;
    char   *cmdptr = argv[1];

    if(argc < 2)
    {
        Con_Printf("Usage: %s (binding class) (cmd) ...\n", argv[0]);
        Con_Printf(": Omit Binding class to clear cmds in all binding classes\n");
        return true;
    }

    // Check for a specified binding class
    if(argc > 2)
        for(i = BDC_NORMAL; i < NUMBINDCLASSES; i++)
        {
            if((!stricmp(argv[1],bindClasses[i].name)) ||
                (atoi(argv[1]) == bindClasses[i].id))
            {
                if(argc < 3)
                {
                    Con_Printf("Usage: %s (binding class) (cmd) ...\n", argv[0]);
                    Con_Printf(": Omit Binding class to clear cmds in all binding classes\n");
                    return true;
                } 
                else 
                {
                    bc = i;
                    cmdptr = argv[2];
                    start = 2;
                    break;
                }
            }
        }

    if(bc == -1 || bc < NUMBINDCLASSES)
    {
        for(i = start; i < argc && cmdptr; cmdptr = argv[i++])
            B_ClearBinding(cmdptr, bc);
    } 
    else 
    {
        Con_Printf("Not a valid binding class. Enter listbindclasses.\n");
        return false;
    }

    return true;
}

D_CMD(ListBindClasses)
{
    int k;

    // Show the available binding classes
    Con_Printf("Binding Classes:\n");

    for(k = BDC_NORMAL; k < NUMBINDCLASSES; k++)
        Con_Printf("  %s\n", bindClasses[k].name);

    return true;
}

D_CMD(ListBindings)
{
    int     i, k, onlythis = -1;
    int        comcount = 0;
    char    buffer[20];

    // Are we showing bindings in a particular class only?
    if(argc >= 2)
    {
        for(k = BDC_NORMAL; k < NUMBINDCLASSES; k++)
            if(!stricmp(argv[1],bindClasses[k].name))
            {
                // only show bindings in this class
                onlythis = bindClasses[k].id;
            }
    }

    // loop through the bindings
    for(i = 0; i < numBinds; i++)
    {
        // build the event
        B_EventBuilder(buffer, &binds[i].event, false);

        // loop through the bindclasses
        for(k = BDC_NORMAL; k < NUMBINDCLASSES; k++)
        {
            if(binds[i].command[k])
            {
                if(argc >= 2)
                {
                    if(onlythis == -1 && strnicmp(buffer + 1, argv[1], strlen(argv[1])))
                        continue;        // Doesn't match the search pattern.
                    else
                    {
                        if(argc >= 3)
                        {
                            if(strnicmp(buffer + 1, argv[2], strlen(argv[2])) || (k != -1 && k != onlythis))
                                continue; // Doesn't match the search pattern
                        } else if(onlythis != -1 && k != onlythis)
                            continue;    // Doesn't match the search pattern
                    }
                }
                comcount++;
                if(onlythis != -1)
                    Con_Printf("%-8s : %s\n", buffer, binds[i].command[k]);
                else
                    Con_Printf("%-8s : %s : %s\n", buffer, bindClasses[k].name, binds[i].command[k]);
            }
        }
    }
    if(onlythis != -1)
    {
        Con_Printf("Showing %d (%s class) commands from %d bindings.\n", 
            comcount, bindClasses[onlythis].name, numBinds);
    }
    else
    {
        Con_Printf("Showing %d commands from %d bindings.\n", 
            comcount, numBinds);
    }
    return true;
}

/*
 *  CCmdEnableBindClass
 *        Enables/disables binding classes
 *        Ques extra input events as required
 */
D_CMD(EnableBindClass)
{
    int i = -1, j, k;
    int        count;

    if(argc < 2 || argc > 3)
    {
        for(i = BDC_NORMAL; i < NUMBINDCLASSES; i++)
            Con_Printf("%d: %s is %s\n",i,bindClasses[i].name,(bindClasses[i].active)? "On" : "Off");

        Con_Printf("Usage: %s (binding class) (1= On 0= Off (leave blank to toggle))\n", argv[0]);
        return true;
    }

    // look for a binding class with either a name or id
    // that matches the argument
    for(i = BDC_NORMAL; i < NUMBINDCLASSES; i++)
    {
        if(!(stricmp(argv[1],bindClasses[i].name)))
        {
            i = bindClasses[i].id;
            break;
        }
    }

	Con_Printf("Class is %d %s\n",i, bindClasses[i].name);

    if(i >= BDC_NORMAL && i < NUMBINDCLASSES)
    {
        // Set the bind class as requested
        if(argc == 3)
            bindClasses[i].active = (atoi(argv[2]))? 1 : 0; // implicitly set
        else
            bindClasses[i].active = bindClasses[i].active? 0: 1; // toggle
    } else {
        Con_Printf("Not a valid binding class. Enter listbindclasses.\n");
        return false;
    }
    
    // Now we need to do a check in case there are keys currently
    // being pressed that should be released if the event binding they are
    // bound too has commands in the bind class being enabled/disabled.

    // loop through the bindings
    for(j = 0; j < numBinds; j++)
    {
        // we're only interested in bindings for down events currently being pressed
        // that have a binding in the class being enabled/disabled (i)
        if( binds[j].command[i] &&
            ( (binds[j].event.type == ev_keydown && (DD_IsKeyDown(binds[j].event.data1))) ||
                (binds[j].event.type == ev_mousebdown && (DD_IsMouseBDown(binds[j].event.data1))) ||
                (binds[j].event.type == ev_joybdown && (DD_IsJoyBDown(binds[j].event.data1))) ))
        {
            count = 0;
            // loop through the commands for this binding,
            // count the number of commands for this binding that are for currently active
            // bind classes with a lower id than the class being enabled/disabled (i)
            for(k = BDC_NORMAL; k < NUMBINDCLASSES; k++)
                if(bindClasses[k].active && binds[j].command[k])
                {
                    // if there is a command for this event binding in a class that is currently active
                    // (current is k), that has a greater id than the class being enabled/disabled (i)
                    // then we don't need to que any extra events at all as that will have been done
                    // when the binding class with the higher id was enabled. The commands in the lower
                    // classes can't have been active (for this event), as the highest class command
                    // is ALWAYS executed unless a specific class is requested.
                    if(k > i)
                    {
                        // don't need to que any extra events
                        count = 0;
                        break;
                    } else
                      count++;
                }

            if(count > 0)
            {
                // We need to send up events with a forced binding
                // command request for all the binding classes with a
                // lower id than the class being enabled/disabled (i)
                // that are also active.
                for(k = BDC_NORMAL; k < i; k++)
                {
                    if(bindClasses[k].active && binds[j].command[k])
                    {
                        event_t ev;
                        // que an up event for this down event.
                        ev = binds[j].event;
                        switch(ev.type)
                        {
                            case ev_keydown:
                                ev.type = ev_keyup;
                                break;
                            case ev_mousebdown:
                                ev.type = ev_mousebup;
                                break;
                            case ev_joybdown:
                                ev.type = ev_joybup;
                                break;
                        }
                        // request a command in this class
                        ev.useclass = bindClasses[k].id;
                        // Finally, post the event
                        DD_PostEvent(&ev);
                    }
                }
            }

            // Also send an up event for this binding if the currently
            // currently active command is in the class being disabled
            // and it has the highest id of the active bindClass commands
            // for this binding
            for(k = NUMBINDCLASSES - 1; k > BDC_NORMAL; k--)
            {
                if((k > i && bindClasses[k].active && binds[j].command[k])
                   || k < i)
                    break;

                if(!bindClasses[k].active && binds[j].command[k])
                {
                    event_t ev;
                    // que an up event for this down event.
                    ev = binds[j].event;
                    switch(ev.type)
                    {
                        case ev_keydown:
                            ev.type = ev_keyup;
                            break;
                        case ev_mousebdown:
                            ev.type = ev_mousebup;
                            break;
                        case ev_joybdown:
                            ev.type = ev_joybup;
                            break;
                     }
                     // request a command in this class
                     ev.useclass = bindClasses[k].id;
                     // Finally, post the event
                     DD_PostEvent(&ev);
                }
			}
        }
    }

    return true;
}

void B_WriteToFile(FILE * file)
{
    int     i, k;
    char    buffer[20];

    for(i = 0; i < numBinds; i++)
    {
        for(k = BDC_NORMAL; k < NUMBINDCLASSES; k++)
        {
            B_EventBuilder(buffer, &binds[i].event, false);
            if(binds[i].command[k])
            {
                fprintf(file, "bind ");
                fprintf(file, "%s ", bindClasses[k].name);
                fprintf(file, "%s", buffer);
                fprintf(file, " \"");
                M_WriteTextEsc(file, binds[i].command[k]);
                fprintf(file, "\"\n");
            }
        }
    }
}

/*
 * B_BindingsForCommand
 *  Returns the number of bindings. The buffer will be filled with the
 *  names of the events.
 */
int B_BindingsForCommand(char *command, char *buffer, int bindClass)
{
    int     i, k, count = 0;
    char    bindname[20];

    strcpy(buffer, "");
    for(i = 0; i < numBinds; i++)
    {
        B_EventBuilder(bindname, &binds[i].event, false);
        // If bindClass is -1 check all bindclasses
        if(bindClass == -1)
        {
            for(k = BDC_NORMAL; k < NUMBINDCLASSES; k++)
                if(binds[i].command[k])
                {
                    if(!stricmp(command, binds[i].command[k]))
                    {
                        if(buffer[0])        // If the buffer is not empty...
                            strcat(buffer, " ");
                        strcat(buffer, bindname);
                        count++;
                    }
                }
        } 
        else 
        {
            // only check bindClass
            assert(bindClass >= 0 && bindClass < sizeof(binds[i].command)/
                sizeof(binds[i].command[0]));
                
            if(binds[i].command[bindClass])
            {
                if(!stricmp(command, binds[i].command[bindClass]))
                {
                    if(buffer[0])        // If the buffer is not empty...
                        strcat(buffer, " ");
                    strcat(buffer, bindname);
                    count++;
                }
            }
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
    int     code = getByShortName(key);

    return code ? code : key[0];
}
