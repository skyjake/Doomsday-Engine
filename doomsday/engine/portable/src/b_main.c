/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/*
 * b_main.c: Event/Command Binding
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"
#include "de_play.h"

#include "b_command.h"
#include "p_control.h"

#include <ctype.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    int     key;                // DDKEY
    char   *name;
} keyname_t;

/*
enum {
    BL_KEYS,
//    BL_KEYSD,
    BL_AXES,
//    BL_AXESD,
    NUM_BIND_LISTS
};

typedef struct {
    uint    numBinds[NUM_BIND_LISTS];
    binding_t *binds[NUM_BIND_LISTS];
} devcontrolbinds_t;
*/

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(BindEventToCommand);
D_CMD(BindControlToDevice);
D_CMD(ListBindings);
D_CMD(ListBindingClasses);
D_CMD(ClearBindingClasses);
D_CMD(ClearBindings);
D_CMD(DeleteBindingById);
D_CMD(ActivateBindingClass);
D_CMD(DefaultBindings);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

/*
static boolean eventBuilder(char *buff, ddevent_t *ev);
static void freeBindList(binding_t *list, uint num);
 */

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

/*
bindclass_t *bindClasses = NULL;
uint numBindClasses = 0;
static uint maxBindClasses = 0;
 */

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int bindingIdCounter;

/*
static devcontrolbinds_t devCtrlBinds[NUM_INPUT_DEVICES];
 */

static const keyname_t keyNames[] = {
    {DDKEY_PAUSE,       "pause"},
    {DDKEY_ESCAPE,      "escape"},
    {DDKEY_ESCAPE,      "esc"},
    {DDKEY_RIGHTARROW,  "right"},
    {DDKEY_LEFTARROW,   "left"},
    {DDKEY_UPARROW,     "up"},
    {DDKEY_DOWNARROW,   "down"},
    {DDKEY_ENTER,       "enter"},
    {DDKEY_TAB,         "tab"},
    {DDKEY_RSHIFT,      "shift"},
    {DDKEY_RCTRL,       "ctrl"},
    {DDKEY_RCTRL,       "control"},
    {DDKEY_RALT,        "alt"},
    {DDKEY_INS,         "insert"},
    {DDKEY_INS,         "ins"},
    {DDKEY_DEL,         "delete"},
    {DDKEY_DEL,         "del"},
    {DDKEY_PGUP,        "pageup"},
    {DDKEY_PGUP,        "pgup"},
    {DDKEY_PGDN,        "pagedown"},
    {DDKEY_PGDN,        "pgdown"},
    {DDKEY_PGDN,        "pgdn"},
    {DDKEY_HOME,        "home"},
    {DDKEY_END,         "end"},
    {DDKEY_BACKSPACE,   "backspace"},
    {DDKEY_BACKSPACE,   "bkspc"},
    {'[',               "sqbracketleft"},
    {']',               "sqbracketright"},
    {'+',               "plus"},
    {'-',               "minus"},
    {'+',               "plus"},
    {'=',               "equals"},
    {' ',               "space"},
    {';',               "semicolon"},
    {',',               "comma"},
    {'.',               "period"},
    {'\"',              "quote"},
    {'\'',              "apostrophe"},
    {DDKEY_F10,         "f10"},
    {DDKEY_F11,         "f11"},
    {DDKEY_F12,         "f12"},
    {DDKEY_F1,          "f1"},
    {DDKEY_F2,          "f2"},
    {DDKEY_F3,          "f3"},
    {DDKEY_F4,          "f4"},
    {DDKEY_F5,          "f5"},
    {DDKEY_F6,          "f6"},
    {DDKEY_F7,          "f7"},
    {DDKEY_F8,          "f8"},
    {DDKEY_F9,          "f9"},
    {'`',               "tilde"},
    {DDKEY_NUMLOCK,     "numlock"},
    {DDKEY_SCROLL,      "scrlock"},
    {DDKEY_NUMPAD0,     "pad0"},
    {DDKEY_NUMPAD1,     "pad1"},
    {DDKEY_NUMPAD2,     "pad2"},
    {DDKEY_NUMPAD3,     "pad3"},
    {DDKEY_NUMPAD4,     "pad4"},
    {DDKEY_NUMPAD5,     "pad5"},
    {DDKEY_NUMPAD6,     "pad6"},
    {DDKEY_NUMPAD7,     "pad7"},
    {DDKEY_NUMPAD8,     "pad8"},
    {DDKEY_NUMPAD9,     "pad9"},
    {DDKEY_DECIMAL,     "padcomma"},
    {DDKEY_SUBTRACT,    "padminus"},    // not really used
    {DDKEY_ADD,         "padplus"},    // not really used

    {0, NULL}                    // The terminator
};

/*
static const char *povDirNames[] = {
    "F", "FR", "R", "BR", "B", "BL", "L", "FL", NULL
};
*/

//static const char evStatePrefixes[NUM_EVENT_STATES] = {'+', '-', '*'};

// DJS 13/05/05
// Binding Classes
//       (very handy for gamepads with a limited number of buttons ;-))

// Binding classes are created dynamically at runtime. During (pre)init the
// game register the classes it NEEDS. The order of the bind Classes in the
// array (the class stack) determines the order in which bindings are
// checked in B_Responder(). Thus it is important that bind classes are
// created in the correct order (game specific).

// It would also be possible for users to create any additional binding
// classes they required at runtime via console commands.
// However that would mean a fair amount of extra book keeping, so for now
// there are three generic classes which can be used for this purpose.

// Bindings are saved with the classnames eg:
//   bind game +w +forward

// However, omission of the class name defaults the bind to class 0 (id 0)
// and the bindings in the cfg will be updated with the missing class names
// automatically on exit (for reading old cfg files).

// When a binding class is enabled/disabled we loop through the bindings
// looking for any that are bound to any keys/buttons being pressed at that
// time. If any are found we que extra up events that request a command in
// a specific binding class. Due to binding classes being ordered numericaly
// with the rule that only the command in the highest active binding class
// being executed we only need to check commands for bindings with a lower
// binding class id.

// A binding class may be "absolute". This means that when the class is
// active, if there is no binding in the current class (while decending the
// bind class stack in B_Responder()) and that class is "absolute" - all
// classes BELOW the current will be ignored and no binding will be found
// for the event. The event is NOT eaten and may continue on down the event
// responder chain.

/*
bindclass_t ddBindClasses[] = {
    {"game", DDBC_NORMAL, 1, 0},
        // additonal classes that can be purposed by users
        {"class1", DDBC_UCLASS1, 0, 0},
        {"class2", DDBC_UCLASS2, 0, 0},
        {"class3", DDBC_UCLASS3, 0, 0},
    {"biaseditor", DDBC_BIASEDITOR, 0, 0},
    {NULL}
};
*/

// CODE --------------------------------------------------------------------

void B_Register(void)
{
    C_CMD("bindevent",      "ss",   BindEventToCommand);
    C_CMD("bindcontrol",    "ss",   BindControlToDevice);
    C_CMD("listbclasses",   NULL,   ListBindingClasses);
    C_CMD("listbindings",   NULL,   ListBindings);
    C_CMD("clearbindings",  "",     ClearBindings);
    C_CMD("clearbclasses",  "",     ClearBindingClasses);
    C_CMD("delbind",        "i",    DeleteBindingById);
    C_CMD("defaultbindings", "",    DefaultBindings);
    C_CMD("activatebclass", "s",    ActivateBindingClass);
    C_CMD("deactivatebclass", "s",  ActivateBindingClass);
}

/**
 * Called once on init.
 */
void B_Init(void)
{
    bclass_t* bc = 0;
    
    if(isDedicated)
    {
        // Why sir, we are but poor folk! Them bindings are too good for us.
        return;
    }
    
    B_NewClass(DEFAULT_BINDING_CLASS_NAME);

    // Game classes.
    // FIXME: Obviously belong to the game, so shouldn't be here.
    B_NewClass("menu");
    B_AcquireKeyboard(B_NewClass("message"), true);

    // Binding class for the console.
    bc = B_NewClass(CONSOLE_BINDING_CLASS_NAME);
    B_AcquireKeyboard(bc, true); // Console takes over all keyboard events.
/*
    B_BindCommand("joy-hat-angle3", "print {angle 3}");
    B_BindCommand("joy-hat-center", "print center");
    
    B_BindCommand("game:key-m-press", "print hello");
    B_BindCommand("key-codex20-up", "print {space released}");
    B_BindCommand("key-up-down + key-shift + key-ctrl", "print \"shifted and controlled up\"");
    B_BindCommand("key-up-down + key-shift", "print \"shifted up\"");
    B_BindCommand("mouse-left", "print mbpress");
    B_BindCommand("mouse-right-up", "print \"right mb released\"");
    B_BindCommand("joy-x-neg1.0 + key-ctrl-up", "print \"joy x negative without ctrl\"");
    B_BindCommand("joy-x- within 0.1 + joy-y-pos1", "print \"joy x centered\"");
    B_BindCommand("joy-x-pos1.0", "print \"joy x positive\"");
    B_BindCommand("joy-x-neg1.0", "print \"joy x negative\"");
    B_BindCommand("joy-z-pos1.0", "print \"joy z positive\"");
    B_BindCommand("joy-z-neg1.0", "print \"joy z negative\"");
    B_BindCommand("joy-w-pos1.0", "print \"joy w positive\"");
    B_BindCommand("joy-w-neg1.0", "print \"joy w negative\"");
    */

    /*B_BindControl("turn", "key-left-staged-inverse");
    B_BindControl("turn", "key-right-staged");
    B_BindControl("turn", "mouse-x");
    B_BindControl("turn", "joy-x + key-shift-up + joy-hat-center + key-code123-down");
    */

    // Bind all the defaults (of engine & game, everything).
    Con_Executef(CMDS_DDAY, false, "defaultbindings");

    // Enable the classes for the initial state.
    B_ActivateClass(B_ClassByName(DEFAULT_BINDING_CLASS_NAME), true);
}

void B_BindDefaults(void)
{
    // Engine's highest priority class: opening control panel, opening the console.
    
    // Console bindings (when open).
    
    // Bias editor.
    
}

/**
 * Deallocates the memory for the commands and bindings.
 */
void B_Shutdown(void)
{
    B_DestroyAllClasses();
}

/**
 * @return  Never returns zero, as that is reserved for list roots.
 */
int B_NewIdentifier(void)
{
    int id = 0;
    while(!id) 
    {
        id = ++bindingIdCounter;
    }
    return id;
}

const char* B_ParseClass(const char* desc, bclass_t** bc)
{
    ddstring_t* str = Str_New();
    
    *bc = 0;
    if(!strchr(desc, ':'))
    {
        // No class defined.
        return desc;
    }
    desc = Str_CopyDelim(str, desc, ':');
    *bc = B_ClassByName(Str_Text(str));
    Str_Free(str);
    return desc;
}

evbinding_t* B_BindCommand(const char* eventDesc, const char* command)
{
    bclass_t* bc;
    evbinding_t *b;
    
    if(isDedicated)
        return NULL;
    
    // The class may be included in the descriptor.
    eventDesc = B_ParseClass(eventDesc, &bc);
    if(!bc)
    {
        bc = B_ClassByName(DEFAULT_BINDING_CLASS_NAME);
    }
    
    if((b = B_NewCommandBinding(&bc->commandBinds, eventDesc, command)) != NULL)
    {
        B_UpdateDeviceStateAssociations();
    }
    return b;
}

dbinding_t* B_BindControl(const char* controlDesc, const char* device)
{
    bclass_t* bc = 0;
    int localNum = 0;
    controlbinding_t* conBin = 0;
    dbinding_t* devBin = 0;
    ddstring_t* str = 0;
    const char* ptr = 0;
    playercontrol_t* control = 0;
    
    if(isDedicated)
        return NULL;
    
    // The control description may begin with the local player number.
    str = Str_New();
    ptr = Str_CopyDelim(str, controlDesc, '-');
    if(!strncasecmp(Str_Text(str), "local", 5) && Str_Length(str) > 5)
    {
        localNum = strtoul(Str_Text(str) + 5, NULL, 10) - 1;
        if(localNum < 0 || localNum >= DDMAXPLAYERS)
        {
            Con_Message("B_BindControl: Local player number %i is invalid.\n", localNum);
            goto finished;
        }
        // Skip past it.
        controlDesc = ptr;
    }
    
    // The next part must be the control name.
    controlDesc = Str_CopyDelim(str, controlDesc, '-');
    control = P_PlayerControlByName(Str_Text(str));
    if(!control)
    {
        Con_Message("B_BindControl: Player control \"%s\" not defined.\n", Str_Text(str));
        goto finished;
    }
    bc = B_ClassByName(control->bindClassName);
    if(!bc)
    {
        bc = B_ClassByName(DEFAULT_BINDING_CLASS_NAME);
    }
    VERBOSE( Con_Message("B_BindControl: Control '%s' in class '%s' of local player %i to be "
                         "bound to '%s'.\n", control->name, bc->name, localNum, device) );
    
    conBin = B_GetControlBinding(bc, control->id);
    if(!(devBin = B_NewDeviceBinding(&conBin->deviceBinds[localNum], device)))
    {
        // Failure in the parsing.
        B_DestroyControlBinding(conBin);
        conBin = 0;
        goto finished;
    }
    
    B_UpdateDeviceStateAssociations();
    
finished:
    Str_Free(str);
    return devBin;
}

dbinding_t* B_GetControlDeviceBindings(int localNum, int control, bclass_t** bClass)
{
    playercontrol_t* pc;
    bclass_t* bc;

    if(localNum < 0 || localNum >= DDMAXPLAYERS)
        return NULL;
    
    pc = P_PlayerControlById(control);
    bc = B_ClassByName(pc->bindClassName);
    if(bClass)
        *bClass = bc;

    return &B_GetControlBinding(bc, control)->deviceBinds[localNum];
}

boolean B_Delete(int bid)
{
    int         i;
    
    for(i = 0; i < B_ClassCount(); ++i)
    {
        if(B_DeleteBinding(B_ClassByPos(i), bid))
            return true;
    }
    return false;
}

D_CMD(BindEventToCommand)
{
    evbinding_t* b = B_BindCommand(argv[1], argv[2]);
    if(b)
    {
        Con_Printf("Binding %i created.\n", b->bid);
    }
    return (b != NULL);
}

D_CMD(BindControlToDevice)
{
    dbinding_t* b = B_BindControl(argv[1], argv[2]);
    if(b)
    {
        Con_Printf("Binding %i created.\n", b->bid);
    }
    return (b != NULL);
}

D_CMD(ListBindingClasses)
{
    B_PrintClasses();
    return true;
}

D_CMD(ListBindings)
{
    B_PrintAllBindings();
    return true;
}

D_CMD(ClearBindingClasses)
{
    B_DestroyAllClasses();
    return true;
}

D_CMD(ClearBindings)
{
    int         i;
    
    for(i = 0; i < B_ClassCount(); ++i)
    {
        Con_Printf("Clearing binding class \"%s\"...\n", B_ClassByPos(i)->name);
        B_ClearClass(B_ClassByPos(i));
    }
    // We can restart the id counter, all the old bindings were destroyed.
    bindingIdCounter = 0;
    return true;
}

D_CMD(DeleteBindingById)
{
    int bid = strtoul(argv[1], NULL, 10);
    
    if(B_Delete(bid))
    {
        Con_Printf("Binding %i deleted successfully.\n", bid);
    }
    else
    {
        Con_Printf("Cannot delete binding %i, it was not found.\n", bid);
    }
    return true;
}

D_CMD(DefaultBindings)
{
    if(isDedicated)
        return false;

    B_BindDefaults();
    
    // Set the game's default bindings.
    Con_Executef(CMDS_DDAY, false, "defaultgamebindings");
    return true;
}

D_CMD(ActivateBindingClass)
{
    boolean doActivate = !stricmp(argv[0], "activatebclass");
    bclass_t* bc = B_ClassByName(argv[1]);
    
    if(!bc)
    {
        Con_Printf("Binding class '%s' does not exist.\n", argv[1]);
        return false;
    }
    B_ActivateClass(bc, doActivate);
    return true;    
}

#if 0
#if _DEBUG
const char* EventType_Str(evtype_t type)
{
    static char typeStr[40];
    struct evnttype_s {
        int type;
        const char* str;
    } evnts[] =
    {
        { EV_KEY,           "key"},
        { EV_MOUSE_AXIS,    "mouse axis"},
        { EV_MOUSE_BUTTON,  "mouse button"},
        { EV_JOY_AXIS,      "joy axis"},
        { EV_JOY_SLIDER,    "joy slider"},
        { EV_JOY_BUTTON,    "joy button"},
        { EV_POV,           "pov"},
        { 0, NULL}
    };
    uint        i;
    boolean     found;

    i = 0;
    found = false;
    while(evnts[i].str && !found)
    {
        if(evnts[i].type == type)
            found = true;
        else
            i++;
    }

    if(found)
        return evnts[i].str;

    sprintf(typeStr, "(unnamed %i)", type);
    return typeStr;
}
#endif

static uint searchBindListForControlID(binding_t **list, uint num,
                                       int controlID)
{
    uint        i;
    boolean     found;

    i = 0;
    found = false;
    while(i < num && !found)
    {
        // \fixme Use a faster than O(n) linear search.
        if(controlID == (*list)[i].controlID)
            found = true;
        else
            i++;
    }
    if(found)
        return i+1; // Index + 1.

    return 0;
}

static binding_t __inline *bindingForEvent(ddevent_t *event)
{
    uint        num, idx;
    binding_t **list;
    devcontrolbinds_t *devBinds = &devCtrlBinds[event->device];

    list = &devBinds->binds[event->isAxis? BL_AXES : BL_KEYS];
    num  = devBinds->numBinds[event->isAxis? BL_AXES : BL_KEYS];

    idx = searchBindListForControlID(list, num, (int) event->obsolete.controlID);
    if(idx != 0)
        return &(*list)[idx-1]; // 1 based index.

    return NULL;
}

/**
 * Search event-type-specific binding array for a command which matches the
 * search critera.
 *
 * @param event         The event to find the command for.
 *
 * @return              Ptr to the found bindcontrol_t ELSE <code>NULL</code>.
 */
static bindcontrol_t *B_GetBindControlForEvent(ddevent_t *ev)
{
    uint        i;
	binding_t  *bnd;
    bindcontrol_t *ctrl = NULL;
    boolean     found;

    if(!I_GetDevice(ev->device, true))
        return NULL;

    bnd = bindingForEvent(ev);
    if(bnd == NULL)
        return NULL;

    found = false;
    if(!ev->noclass) // Use a specific class? (active or not)
    {
        // \note These kind of events aren't sent via direct user input.
        // Only by "us" when we need to switch binding classes and a
        // current input is active eg; a key is held down during the
        // switch that has commands in multiple binding classes.
        ctrl = &bnd->binds[ev->useclass];
        switch(ctrl->type)
        {
        case BND_AXIS:
            if(ctrl->data.axiscontrol.playercontrol != -1)
                found = true;
            break;

        case BND_COMMAND:
            if(ctrl->data.command.command[ev->data1])
                found = true;
            break;

        case BND_UNUSED:
        default:
            break;
        }
    }
    else
    {
        uint        idx;
        boolean     done;

        // Loop backwards through the active binding classes, the
        // command in the highest binding class slot that is currently
        // active is executed.
        i = 0;
        done = false;
        while(i < numBindClasses && !done)
        {
            idx = numBindClasses - 1 - i;

            if(bindClasses[idx].active == 1)
            {
                ctrl = &bnd->binds[idx];
                switch(ctrl->type)
                {
                case BND_AXIS:
                    if(ctrl->data.axiscontrol.playercontrol != -1)
                    {
                        found = true;
                        done = true;
                    }
                    break;

                case BND_COMMAND:
                    {
                    bindcommand_t *cmd = &ctrl->data.command;
                    if(cmd->command[ev->data1])
                    {
                        found = true;
                        done = true;
                    }
                    else
                    {
                        // RULE: If a repeat event does not have a
                        // binding in BINDCLASS (k) we should ignore
                        // commands in all lower classes IF there is NOT
                        // a down binding for this event in this class.
                        if(ev->data1 == EVS_REPEAT && cmd->command[EVS_DOWN])
                            done = true; // Do nothing.
                    }
                    break;
                    }

                case BND_UNUSED:
                default:
                    break;
                }

                if(!done)
                {
                    // Should we ignore commands in lower classes?
                    if(bindClasses[idx].flags & BCF_ABSOLUTE)
                        done = true;
                }
            }

            if(!done)
                i++;
        }
    }

    if(found)
        return ctrl;

    return NULL;
}
#endif

/**
 * Checks to see if we need to respond to the given input event in some way
 * and then if so executes the action associated to the event.
 *
 * @param ev            ddevent_t we may need to respond to.
 *
 * @return              <code>true</code> If an action was executed.
 */
boolean B_Responder(ddevent_t *ev)
{
    return B_TryEvent(ev);
    
    /*
    bindcontrol_t *ctrl;

    if((ctrl = B_GetBindControlForEvent(ev)) == NULL)
        return false; // Nope, nothing.

    // Found one!
    if(ctrl->type == BND_COMMAND)
    {
        bindcommand_t *bnd = &ctrl->data.command;

        Con_Execute(CMDS_BIND, bnd->command[ev->data1], true, false);
    }
    else // An axis control.
    {
        float       pos;
        inputdev_t *device = I_GetDevice(ev->device, true);
        inputdevaxis_t *axis = &device->axes[ev->obsolete.controlID];
        bindaxis_t *bnd = &ctrl->data.axiscontrol;

        // Invert the axis position, if requested.
        if(bnd->invert)
            pos = -axis->position;
        else
            pos = axis->position;

        // Update the control state.
        switch(device->axes[ev->obsolete.controlID].type)
        {
        case IDAT_STICK: // joysticks, gamepads
            P_ControlSetAxis(P_LocalToConsole(bnd->localPlayer),
		                     bnd->playercontrol, pos);
            break;

        case IDAT_POINTER: // mouse
            P_ControlAxisDelta(P_LocalToConsole(bnd->localPlayer),
		                       bnd->playercontrol, pos);
            break;

        default:
            break;
        }
    }

    return true;
     */
}

#if 0
/**
 * Retrieve a binding for the given device control.
 *
 * @param deviceID      Device ident for the binding.
 * @param controlID     Device control index for the binding, either a
 *                      key/button number or axis index number.
 * @param isAxis        If <code>true</code> @param controlID is an axis
 *                      index number.
 * @param createNew     If <code>true</code> a new binding_t will be 
 *                      allocated if an existing one cannot be found.
 *
 * @return              Binding for the given event OR <code>NULL</code>
 */
static binding_t *B_GetBinding(uint deviceID, uint controlID,
                               boolean isAxis, boolean createNew)
{
    uint        i, *num, idx;
    binding_t  *newb, **list;
    devcontrolbinds_t *devBinds = &devCtrlBinds[deviceID];
 
    list = &devBinds->binds[isAxis? BL_AXES : BL_KEYS];
    num  = &devBinds->numBinds[isAxis? BL_AXES : BL_KEYS];

    // We'll first have to search through the existing bindings
    // to see if there already is one for this event.
    idx = searchBindListForControlID(list, *num, (int) controlID);
    if(idx != 0)
        return &(*list)[idx-1]; // 1 based index.

    // If we arn't creating a new one, we've nothing more to do.
    if(!createNew)
        return NULL;

    // Hmm, no luck there. Let's create a new binding_t.
    *list = M_Realloc((*list), sizeof(binding_t) * ++(*num));
    newb = (*list) + (*num) - 1;

    // Initalize the binding.
    newb->controlID = controlID;
    newb->binds = M_Calloc(sizeof(bindcontrol_t) * maxBindClasses);

    for(i = 0; i < numBindClasses; ++i)
        newb->binds[i].type = BND_UNUSED;

    return newb;
}

static void B_DeleteBindingIdx(uint deviceID, uint index, boolean isAxis)
{
    uint        i, k, *num;
    devcontrolbinds_t *devBinds = &devCtrlBinds[deviceID];
    binding_t  *bnd, **list;

    list = &devBinds->binds[isAxis? BL_AXES : BL_KEYS];
    num  = &devBinds->numBinds[isAxis? BL_AXES : BL_KEYS];

    if(index > (*num) - 1)
        return; // What?

    bnd = &(*list)[index];
    for(i = 0; i < numBindClasses; ++i)
    {
        if(bnd->binds[i].type == BND_COMMAND)
            for(k = 0; k < NUM_EVENT_STATES; ++k)
            {
                bindcommand_t *com = &bnd->binds[i].data.command;
                if(com->command[k])
                {
                    M_Free(com->command[k]);
                    com->command[k] = NULL;
                }
            }
    }
    M_Free(bnd->binds);

    if(index < (*num) - 1) // If not the last one, do some rollback.
    {
        memmove((*list) + index, (*list) + index + 1,
                sizeof(binding_t) * ((*num) - index - 1));
    }
    (*list) = M_Realloc((*list), sizeof(binding_t) * --(*num));
}

static void freeBindList(binding_t *list, uint num)
{
    uint        i, j, k;
    binding_t  *bnd;
    bindcontrol_t *ctrl;

    if(!list)
        return;

    for(i = 0, bnd = list; i < num; ++i, bnd++)
    {
        for(j = 0; j < numBindClasses; ++j)
        {
            ctrl = &bnd->binds[j];

            if(ctrl->type == BND_COMMAND)
            {
                bindcommand_t *com = &ctrl->data.command;

                for(k = 0; k < NUM_EVENT_STATES; ++k)
                    if(com->command[k])
                        M_Free(com->command[k]);
            }
        }

        M_Free(bnd->binds);
        bnd->binds = NULL;
    }
}

/**
 * Clears all bindings for all devices, commands and axes.
 *
 * @param active    If <code>true</code> clear those from active lists.
 * @param default   If <code>true</code> clear those from defaults lists.
 */
void B_ClearBindings(boolean active, boolean defaults)
{
    uint        d, l;
    devcontrolbinds_t *devBinds;
    uint        *num;
    binding_t  **bnd;

    if(!active && !defaults)
        return; // Nothing to do...

    for(d = 0; d < NUM_INPUT_DEVICES; ++d)
    {
        devBinds = &devCtrlBinds[d];

        for(l = 0; l < NUM_BIND_LISTS; ++l)
        {
/*
            // Only clearing selected lists?
            if((!active && (l == BL_KEYS || l == BL_AXES)) ||
               (!defaults && (l == BL_KEYSD || l == BL_AXESD)))
               continue;
*/
            bnd = &devBinds->binds[l];
            num = &devBinds->numBinds[l];
            if(*bnd)
            {
                freeBindList(*bnd, *num);
                M_Free(*bnd);
                (*bnd) = NULL;
                (*num) = 0;
            }
        }
    }
}

/**
 * Binds the given event to the command. Also rebinds old bindings.
 *
 * 1) Binding to NULL without specifying a class: deletes the binding.
 *
 * 2) Bindind to NULL and specifying a class: clears the command and
 *    if no more commands exist for this binding - will delete it
 */
binding_t *B_Bind(ddevent_t *ev, char *command, int control, uint bindClass)
{
    uint        i;
    binding_t  *bnd;
    bindcommand_t *com;
    bindaxis_t *ctrl;
    inputdev_t *device;
    boolean     removing = (ev->isAxis? (control < 0):(!command));

    bnd = B_GetBinding(ev->device, ev->obsolete.controlID, ev->isAxis, !removing);

    if(removing)
    {
        uint        k, count = 0;
        boolean     unused;

        if(!bnd)
            return NULL; // Can't remove a binding that doesn't exist.

        // Clear the command in bindClass only.
        for(i = 0; i < numBindClasses; ++i)
        {
            if(bnd->binds[i].type == BND_UNUSED)
                continue;

            if(i == bindClass)
            {
                unused = true;
                // Implicit; axis bindings which match bindClass are unused.
                
                // Command bindings need to be checked as each has
                // multiple states.
                if(bnd->binds[i].type == BND_COMMAND)
                {
                    for(k = 0; k < NUM_EVENT_STATES; ++k)
                    {
                        com = &bnd->binds[i].data.command;

                        if(com->command[k])
                        {
                            if((int) k == ev->data1)
                            {
                                M_Free(com->command[k]);
                                com->command[k] = NULL;
                            }
                            else
                                unused = false;
                        }
                    }
                    if(!unused)
                        count++;
                }
                if(unused)
                    bnd->binds[i].type = BND_UNUSED;
            }
            else
                count++;
        }

        if(count == 0)
        {   // There are no more controls/commands for this binding so delete.
            devcontrolbinds_t *devBinds = &devCtrlBinds[ev->device];
            uint    idx = bnd - devBinds->binds[ev->isAxis? BL_AXES : BL_KEYS];

            B_DeleteBindingIdx(ev->device, idx, ev->isAxis);
        }

        return NULL;
    }

    if(bnd->binds[bindClass].type == BND_COMMAND)
    {
        // If, changing from a command to an axis bind, free all commands
        // in all states. Else, just free the command in the state being
        // updated (if one already present).
        com = &bnd->binds[bindClass].data.command;
        for(i = 0; i < NUM_EVENT_STATES; ++i)
        {
            if(!ev->isAxis && (int) i != ev->data1)
                continue;

            if(com->command[i])
            {
                M_Free(com->command[i]);
                com->command[i] = NULL;
            }
        }
    }

    // Set the control
    device = I_GetDevice(ev->device, false);
    bnd->binds[bindClass].type = (ev->isAxis? BND_AXIS : BND_COMMAND);
    if(bnd->binds[bindClass].type == BND_AXIS)
    {
        ctrl = &bnd->binds[bindClass].data.axiscontrol;
	    ctrl->playercontrol = control;
//#if _DEBUG
//Con_Printf("B_Bind: (%s) axis '%s' ctrl: '%s'\n",
//           device->name, I_GetAxisByID(device, bnd->obsolete.controlID)->name,
//           P_ControlGetAxisName(control));
//#endif
    }
    else
    {
        com = &bnd->binds[bindClass].data.command;
        com->command[ev->data1] = M_Malloc(strlen(command) + 1);
        strcpy(com->command[ev->data1], command);

//#if _DEBUG
//Con_Printf("B_Bind: (%s) state: %i ctrlID: %d cmd: \"%s\"\n",
//           device->name, ev->data1,
//           bnd->obsolete.controlID, com->command[ev->data1]);
//#endif
    }

    return bnd;
}
#endif

const char *B_ShortNameForKey(int ddkey)
{
    uint        idx;
    static char nameBuffer[40];

    for(idx = 0; keyNames[idx].key; ++idx)
    {
        if(ddkey == keyNames[idx].key)
            return keyNames[idx].name;
    }

    if(isalnum(ddkey))
    {
        // Printable character, fabricate a single-character name.
        nameBuffer[0] = tolower(ddkey);
        nameBuffer[1] = 0;
        return nameBuffer;
    }

    return NULL;
}

int B_KeyForShortName(const char *key)
{
    uint        idx;

    for(idx = 0; keyNames[idx].key; ++idx)
    { 
        if(!stricmp(key, keyNames[idx].name))
            return keyNames[idx].key;
    }
    
    if(strlen(key) == 1 && isalnum(key[0]))
    {
        // ASCII char.
        return tolower(key[0]);
    }
    
    return 0;
}

static int getButtonNumber(int flags)
{
    uint        idx;
    boolean     found;

    idx = 0;
    found = false;
    while(idx < 32 && !found)
    {
        if(flags & (1 << idx))
            found = true;
        else
            idx++;
    }

    if(found)
        return idx;

    return -1;
}

#if 0
/**
 * Converts a textual representation of an event to the real thing.
 * Buff must be valid source buffer ev a valid destination.
 *
 * @param buff      Src buffer containing the textual event string.
 * @param ev        The destination event to be updated.
 */
static boolean eventBuilder(char *buff, ddevent_t *ev)
{
    char    prefix;
    char   *begin;
    int     key;

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
        ev->device = IDEV_MOUSE;
        ev->isAxis = false;
        ev->data1 = (prefix == '+' ? EVS_DOWN : EVS_UP);
        ev->obsolete.controlID = 1 << (atoi(begin + 2) - 1);
        return true;
    }
    else if(!strnicmp(begin, "mw", 2))    // Mouse wheel?
    {
        ev->device = IDEV_MOUSE;
        ev->isAxis = false; // mouse wheel is translated to keys.
        ev->data1 = (prefix == '+' ? EVS_DOWN : EVS_UP);
        ev->obsolete.controlID =
            !stricmp(begin + 2, "up") ? DD_MWHEEL_UP : DD_MWHEEL_DOWN;
        return true;
    }
    else if(!strnicmp(begin, "jb", 2))    // Joystick button?
    {
        ev->device = IDEV_JOY1;
        ev->isAxis = false;
        ev->data1 = (prefix == '+' ? EVS_DOWN : EVS_UP);
        ev->obsolete.controlID = 1 << (atoi(begin + 2) - 1);
        return true;
    }
    else if(!strnicmp(begin, "pov", 3))    // A POV angle?
    {
        uint        idx;
        boolean     found;

        ev->device = IDEV_JOY1;
        ev->isAxis = false;
        ev->data1 = (prefix == '+' ? EVS_DOWN : EVS_UP);
        ev->obsolete.controlID = 0;

        idx = 0;
        found = false;
        while(povDirNames[idx] && !found)
        {
            if(!stricmp(begin + 3, povDirNames[idx]))
            {
                ev->obsolete.controlID = idx;
                found = true;
            }
            else
                idx++;
        }
        return true;
    }
    else
    {
        ev->device = IDEV_KEYBOARD;
        ev->isAxis = false;
        ev->data1 =
            (prefix == '+' ? EVS_DOWN : (prefix == '*' ? EVS_REPEAT :
             EVS_UP));

        if((key = getByShortName(begin)))
            ev->obsolete.controlID = key;
        else
            ev->obsolete.controlID = begin[0];
        return true;
    }
}

/**
 * Forms a textual representation for an input event.
 *
 * @param  buff        Destination buffer to hold the formed string.
 * @param  deviceID    Device ID the event is for (e.g. IDEV_KEYBOARD = keyboard).
 * @param  controlID   Usage depends on <code>type</code> eg keynumber.
 * @param  isAxis      If <code>true</code> @param controlID is treated
 *                     as a device axis id.
 * @param  state       The event state (EVS_DOWN/EVS_UP/EVS_REPEAT).
 *                     Only used with keys (not with axis controls).
 */
void formEventString(char *buff, uint deviceID, int controlID,
                     boolean isAxis, evstate_t state)
{
    inputdev_t *device = I_GetDevice(deviceID, false);
    char   *begin;

    if(!device)
        Con_Error("formEventString: bad device id (%i)\n", deviceID);

    if(isAxis)
    {
        sprintf(buff, "%s-%s", device->name,
				device->axes[controlID].name);
    }
    else
    {
        if(state > EVS_REPEAT)
            Con_Error("formEventString: bad event state (%d)\n", state);

        switch(deviceID)
        {
        case IDEV_KEYBOARD:
            if((begin = B_ShortNameForKey(controlID)))
            {
                sprintf(buff, "%c%s", evStatePrefixes[state], begin);
            }
            else if(controlID > 32 && controlID < 128)
            {
                sprintf(buff, "%c%c", evStatePrefixes[state], controlID);
            }
            break;

        case IDEV_MOUSE:
            if(controlID & (DD_MWHEEL_UP | DD_MWHEEL_DOWN))
                sprintf(buff, "%cMW%s", evStatePrefixes[state],
                        controlID & DD_MWHEEL_UP ? "up" : "down");
            else
                sprintf(buff, "%cMB%d", evStatePrefixes[state],
                        buttonNumber(controlID) + 1);
            break;

        case IDEV_JOY1:
            sprintf(buff, "%cJB%d", evStatePrefixes[state],
                    buttonNumber(controlID) + 1);
            break;

        default:
            break; // impossible.
        }
    }
}

/**
 * Forms a textual representation for an input event, translates from the
 * game's event_t format to our internal ddevent_t format.
 * Part of the Doomsday public API.
 */
void B_FormEventString(char *buff, evtype_t type, evstate_t state,
                       int data1)
{
    uint        deviceID = 0;
    boolean     isAxis = false;

    // These are the same translation rules as used in DD_ProcessEvents()
    // except inverted as we are translating from event_t to ddevent_t.
    switch(type)
    {
    case EV_KEY:
        deviceID = IDEV_KEYBOARD;
        isAxis = false;
        break;

    case EV_MOUSE_AXIS:
        deviceID = IDEV_MOUSE;
        isAxis = true;
        break;

    case EV_MOUSE_BUTTON:
        deviceID = IDEV_MOUSE;
        isAxis = false;
        break;

    case EV_JOY_AXIS:
        deviceID = IDEV_JOY1;
        isAxis = true;
        break;

    case EV_JOY_SLIDER:
        deviceID = IDEV_JOY1;
        isAxis = true;
        break;

    case EV_JOY_BUTTON:
        deviceID = IDEV_JOY1;
        isAxis = false;
        break;

    default:
        Con_Error("B_FormEventString: Unknown event type %i.", type);
    }

    formEventString(buff, deviceID, data1, isAxis, state);
}

/**
 * Retrieve the id of the named bind class.
 *
 * @param           The symbolic bind class name to search for OR identifier
 *                  in the form "bdc#" where '#' = bind class id.
 *
 * @return          <code>true</code> if one is found.
 */
static boolean B_GetBindClassIDbyName(const char *name, uint *id)
{
    uint        i;
    boolean     found = false;

    if(!name || !name[0])
        return false;

    // By bindClass id first.
    if(!strnicmp(name, "bdc", 3))
    {
        uint        idx = (uint) atoi(name+3);
        i = 0;
        while(i < numBindClasses && !found)
        {
            if(idx == bindClasses[i].id)
            {
                *id = idx;
                found = true;
            }
            else
                ++i;
        }
    }
    if(!found)
    {   // Not found, now check the names.
        i = 0;
        while(i < numBindClasses && !found)
        {
            if(!stricmp(name, bindClasses[i].name))
            {
                if(id)
                    *id = bindClasses[i].id;
                found = true;
            }
            else
                ++i;
        }
    }

    return found;
}
#endif

void DD_AddBindClass(bindclass_t *newbc)
{
    /*
    uint        i, j, k;
    bindclass_t *added;

    VERBOSE2(Con_Printf("DD_AddBindClass: %s.\n", newbc->name));

    if(B_GetBindClassIDbyName(newbc->name, NULL))
        Con_Error("DD_AddBindClass: Cannot register. A bind class by the "
                  "name '%s' already exists.", newbc->name);

    if(++numBindClasses > maxBindClasses)
    {
        // Allocate more memory.
        maxBindClasses *= 2;
        if(maxBindClasses < numBindClasses)
            maxBindClasses = numBindClasses;

        bindClasses = M_Realloc(bindClasses, sizeof(bindclass_t) * maxBindClasses);

        for(j = 0; j < NUM_INPUT_DEVICES; ++j)
        {
            devcontrolbinds_t *devBinds = &devCtrlBinds[j];
            uint        l;

            for(l = 0; l < NUM_BIND_LISTS; ++l)
            {
                binding_t  *bnd = devBinds->binds[l];
                uint        num = devBinds->numBinds[l];

                for(i = 0; i < num; ++i)
                {
                    bnd[i].binds =
                        M_Realloc(bnd[i].binds, sizeof(bindcontrol_t) * maxBindClasses);
                    for(k = numBindClasses - 1; k < maxBindClasses; ++k)
                        bnd[i].binds[k].type = BND_UNUSED;
                }
            }
        }
    }

    added = &bindClasses[numBindClasses - 1];
    memcpy(added, newbc, sizeof(bindclass_t));
    added->id = numBindClasses - 1;

    // Allocate a copy of the class name.
    added->name = strdup(newbc->name);
     */
}

/**
 * Enables/disables binding classes (wrapper for the game dll).
 *
 * Allows users to create their own binding classes that can be placed
 * anywhere in the bindClass stack without the dll having to keep track of
 * the classIDs.
 */
boolean DD_SetBindClass(uint classID, uint type)
{
    // Creation of user bind classes not implemented yet so there is no
    // offset.
    return B_SetBindClass(classID, type);
}

/*
static void queEventsForHeldControls(uint deviceID, uint classID)
{
    uint        l, i, k, count;
    binding_t  *bind;
    boolean     isDone;
    devcontrolbinds_t *dCBinds = &devCtrlBinds[deviceID];
    inputdev_t *dev = I_GetDevice(deviceID, true);

    if(!dev)
        return;

    for(l = 0; l < NUM_BIND_LISTS; ++l)
    {        
        // Skip default lists.
//        if(l == BL_KEYSD || l == BL_AXESD)
//            continue;

        for(i = 0, bind = dCBinds->binds[l]; i < dCBinds->numBinds[l];
            ++i, bind++)
        {
            bindcommand_t *com;
            bindaxis_t *axis;

            switch(bind->binds[classID].type)
            {
            case BND_UNUSED:
                continue;

            case BND_COMMAND:
                com = &bind->binds[classID].data.command;

                // We're only interested in bindings for down events currently
                // being pressed that have a binding in the class being
                // enabled/disabled (classID)
                if(!(com->command[EVS_DOWN] != NULL && bind->controlID >= 0 &&
                     I_IsDeviceKeyDown(deviceID, (uint) bind->controlID)))
                    continue;
                break;

            case BND_AXIS:
                axis = &bind->binds[classID].data.axiscontrol;

                // We're only interested in bindings for axes which are
                // currently outside their dead zone, that have a binding in the
                // class being enabled/disabled (classID)
                // \fixme Actually check the zone!
                if(!dev->axes[axis->playercontrol].position)
                    continue;
                break;
            };

            // Iterate all commands for this binding, count the number of
            // commands for this binding that are for currently active bind
            // classes with a lower id than the class being enabled/disabled
            // (classID).

            count = 0;
            k = 0;
            isDone = false;
            while(k < numBindClasses && !isDone)
            {
                if(bindClasses[k].active && bind->binds[k].type != BND_UNUSED)
                {
                    boolean     ignore = false;

                    if(bind->binds[k].type == BND_COMMAND)
                    {
                        com = &bind->binds[k].data.command;
                        if(!com->command[EVS_DOWN])
                            ignore = true;
                    }

                    if(!ignore)
                    {
                        // If there is a command for this event binding in a
                        // class that is currently active (current is k), that
                        // has a greater id than the class being enabled/disabled
                        // (classID) then we don't need to que any extra events
                        // at all as that will have been done when the binding
                        // class with the higher id was enabled. The commands in
                        // the lower classes can't have been active (for this
                        // event), as the highest class command is ALWAYS
                        // executed unless a specific class is requested.
                        if(k > classID)
                        {   // Don't need to que any extra events.
                            count = 0;
                            isDone = true;
                        }
                        else
                            count++;
                    }
                }

                if(!isDone)
                    k++;
            }

            if(count > 0)
            {
                // We need to send either up or axis center events, specifing a
                // bind class for all the bind classes with a lower id than the
                // class being enabled/disabled (classID) that are also active
                // (note the order does not matter).
                for(k = 0; k < classID; ++k)
                {
                    if(bindClasses[k].active &&
                       bind->binds[k].type != BND_UNUSED)
                    {
                        boolean     ignore = false;

                        if(bind->binds[k].type == BND_COMMAND)
                        {
                            com = &bind->binds[k].data.command;
                            if(!com->command[EVS_UP])
                                ignore = true;
                        }

                        if(!ignore)
                        {   // Que an up/center event for this.
                            ddevent_t ev;

                            ev.deviceID = deviceID;
                            ev.obsolete.controlID = bind->controlID;
                            if(bind->binds[k].type == BND_AXIS)
                            {
                                ev.isAxis = true;
                                ev.data1 = 0;
                            }
                            else
                            {
                                ev.isAxis = false;
                                ev.data1 = EVS_UP;
                            }

                            // Specify a bind class.
                            ev.useclass = bindClasses[k].id;
                            ev.noclass = false;

                            DD_PostEvent(&ev);
                        }
                    }
                }
            }

            // Also send an up/center event for this binding if the currently
            // active command is in the class being disabled and it has the
            // highest id of the active bindClass commands for this binding.
            k = 0;
            isDone = false;
            while(k < numBindClasses && !isDone)
            {
                uint        idx = numBindClasses - 1 - k;

                if(idx < classID)
                {
                    isDone = true;
                }
                else
                {
                    boolean     present = false;

                    switch(bind->binds[idx].type)
                    {
                    case BND_COMMAND:
                        com = &bind->binds[idx].data.command;
                        if(com->command[EVS_DOWN])
                            present = true;
                        break;

                    case BND_AXIS:
                        present = true;
                        break;

                    default:
                        break;
                    };
                    
                    if(present)
                        if(idx > classID && bindClasses[idx].active)
                        {
                            isDone = true;
                        }
                        else
                        {
                            if(!bindClasses[idx].active)
                            {   // Que an up/center event for this.
                                ddevent_t ev;
                                
                                ev.deviceID = deviceID;
                                ev.obsolete.controlID = bind->controlID;
                                if(bind->binds[idx].type == BND_AXIS)
                                {
                                    ev.isAxis = true;
                                    ev.data1 = 0;
                                }
                                else
                                {
                                    ev.isAxis = false;
                                    ev.data1 = EVS_UP;
                                }

                                // Specify a bind class.
                                ev.useclass = bindClasses[idx].id;
                                ev.noclass = false;

                                DD_PostEvent(&ev);
                            }
                        }
                }

                k++;
            }
        }
    }
}
*/

/**
 * Enables/disables binding classes
 * Ques extra input events as required
 */
boolean B_SetBindClass(unsigned int classID, unsigned int type)
{/*
    uint        g;

    // Change the active state of the bindClass.
    switch(type)
    {
    case 0:  // implicitly set
    case 1:
        bindClasses[classID].active = type? 1 : 0;
        break;

    case 2:  // toggle
        bindClasses[classID].active = bindClasses[classID].active? 0 : 1;
        break;

    default:
        Con_Error("B_SetBindClass: Unknown state change value %i", type);
    }

    VERBOSE2(Con_Printf("B_SetBindClass: %s %s %s.\n",
                        bindClasses[classID].name,
                        (type==2)? "TOGGLE" : "SET",
                        bindClasses[classID].active? "ON" : "OFF"));

    // Now we need to do a check in case there are keys currently
    // being pressed that should be released if the event binding they are
    // bound too has commands in the bind class being enabled/disabled.
    // Also, if there are any axes which are outside their dead zone and
    // if there axis bindings in the bind class being enabled/disabled, we
    // need to send centering events for them.

    for(g = 0; g < NUM_INPUT_DEVICES; ++g)
        queEventsForHeldControls(g, classID);
*/
    return true;
}
/*
static uint writeBindList(FILE *file, binding_t *list, uint num,
                          uint deviceID, uint bindClass)
{
    uint        i, j, count = 0;
    binding_t  *bnd;
    bindcontrol_t *ctrl;
    char        buffer[20];

    for(i = 0, bnd = list; i < num; ++i, bnd++)
    {
        ctrl = &bnd->binds[bindClass];
        switch(ctrl->type)
        {
        case BND_AXIS:
            {
            bindaxis_t *axis = &ctrl->data.axiscontrol;

            formEventString(buffer, deviceID, bnd->controlID, true, 0);
            // \fixme Using "after" is a hack...
            fprintf(file, "after 1 { bindaxis %s %s ",
                    bindClasses[bindClass].name, buffer);
            if(axis->invert)
                fprintf(file, "-");
            fprintf(file, "%s", P_ControlGetAxisName(axis->playercontrol));
            if(axis->localPlayer > 0)
                fprintf(file, "/%i", axis->localPlayer);
            fprintf(file, " }\n");

            count++;
            break;
            }

        case BND_COMMAND:
            {
            bindcommand_t *com = &ctrl->data.command;

            for(j = 0; j < NUM_EVENT_STATES; ++j)
            {
                formEventString(buffer, deviceID, bnd->controlID, false, j);
                if(com->command[j])
                {
                    fprintf(file, "bind ");
                    fprintf(file, "%s ", bindClasses[bindClass].name);
                    fprintf(file, "%s", buffer);
                    fprintf(file, " \"");

                    M_WriteTextEsc(file, com->command[j]);
                    fprintf(file, "\"\n");

                    count++;
                }
            }
            break;
            }

        case BND_UNUSED:
        default:
            break;
        }
    }

    return count;
}
*/

/**
 * Dump all the bindings to a text (cfg) file.  Outputs console commands.
 */
void B_WriteToFile(FILE *file)
{
    int     i;

    // Start with a clean slate when restoring the bindings.
    fprintf(file, "clearbindings\n\n");
    
    for(i = 0; i < B_ClassCount(); ++i)
    {
        B_WriteClassToFile(B_ClassByPos(i), file);
    }
}

/**
 * Return the key code that corresponds the given key identifier name.
 * Part of the Doomsday public API.
 */
int DD_GetKeyCode(const char *key)
{
    int         code = B_KeyForShortName(key);

    return (code ? code : key[0]);
}

/*
static uint printBindList(char *searchKey, uint deviceID, int bindClass,
                          binding_t *list, uint num)
{
    uint        i, j, k, count;
    binding_t  *bnd;
    bindcontrol_t *ctrl;
    boolean     noMatch;
    char       buffer[40];

    count = 0;
    for(i = 0, bnd = list; i < num; ++i, bnd++)
    {
        for(j = 0; j < numBindClasses; ++j)
        {
            ctrl = &bnd->binds[j];
            switch(ctrl->type)
            {
            case BND_COMMAND:
                {
                bindcommand_t *com = &ctrl->data.command;

                // for each event state.
                for(k = 0; k < NUM_EVENT_STATES; ++k)
                {
                    if(com->command[k])
                    {
                        formEventString(buffer, deviceID, bnd->controlID,
                                        (ctrl->type == BND_AXIS), k);

                        // Does this event match the search pattern?
                        noMatch = false;
                        if(bindClass >= 0)
                        {
                            if(j != (uint) bindClass)
                                noMatch = true;
                        }
                        if(!noMatch)
                            if(searchKey &&
                               strnicmp(buffer + 1, searchKey, strlen(searchKey)))
                                noMatch = true;

                        if(!noMatch)
                        {
                            if(bindClass >= 0)
                                Con_Printf("%-8s : %s\n", buffer,
                                           com->command[k]);
                            else
                                Con_Printf("%-8s : %-8s : %s\n", buffer,
                                           bindClasses[j].name,
                                           com->command[k]);
                            count++;
                        }
                    }
                }
                break;
                }

            case BND_AXIS:
                {
                bindaxis_t *ctl = &ctrl->data.axiscontrol;

                formEventString(buffer, deviceID, bnd->controlID,
                                (ctrl->type == BND_AXIS), 0);

                // Does this device control match the search pattern?
                noMatch = false;
                if(bindClass >= 0)
                {
                    if(j != (uint) bindClass)
                        noMatch = true;
                }
                if(!noMatch)
                    if(searchKey &&
                       strnicmp(buffer, searchKey, strlen(searchKey)))
                        noMatch = true;

                if(!noMatch)
                {
                    const char *axisName =
                        P_ControlGetAxisName(ctl->playercontrol);
                                            
                    if(bindClass >= 0)
                        Con_Printf("%-8s : %s%s\n", buffer,
			                       (ctl->invert? "-" : ""),
			                       axisName);
                    else
                        Con_Printf("%-8s : %-8s : %s%s\n", buffer,
                                   bindClasses[j].name,
			                       (ctl->invert? "-" : ""),
			                       axisName);
                    count++;
                }
                break;
                }

            case BND_UNUSED:
            default:
                break;
            }
        }
    }

    return count;
}*/

/**
 * The "bindaxis" console command creates and deletes axis bindings.
 *
 * Example:  bindaxis bindclass mouse-y (-)look/2
 */
D_CMD(BindAxis)
{/*
    uint        i;
	uint        deviceID, axis;
	boolean     invert = false;
	const char *name;
	char        *ptr, ctlName[20];
	int         local = 0;
	binding_t  *bind;
    uint        ctlidx, bc = 0;
    const char *axisptr = argv[2];
    const char *ctrlptr = argv[3];
    boolean     bindClassGiven = false;
    bindaxis_t *ctrl;

	if(argc < 3 || argc > 4)
	{
		Con_Printf("Usage: %s (class) (device-axis) (control)\n", argv[0]);
        Con_Printf("Binding Classes:\n");
        for(i = 0; i < numBindClasses; ++i)
            Con_Printf("  %s\n", bindClasses[i].name);
        return true;
	}

    // Check for a specified binding class.
    bindClassGiven = B_GetBindClassIDbyName(argv[1], &bc);

    if(argc == 4 && !bindClassGiven)
    {
        Con_Printf("'%s' is not a valid bindClass name/id.\n", argv[1]);
        return false;
    }

    // Has a binding class been specified?
    if(!bindClassGiven)
    {
        // No it hasn't! default to normal
        bc = DDBC_NORMAL;
        axisptr = argv[1];
        ctrlptr = argv[2];
    }
    
    // Get the device and the axis.
	if(!I_ParseDeviceAxis(axisptr, &deviceID, &axis))
	{
		Con_Printf("'%s' is not a valid device or device axis.\n", axisptr);
		return false;
	}

    // If no control is given, delete the binding.
    if(argc == 3 && bindClassGiven)
	{
        ddevent_t ev;

        ev.deviceID = deviceID;
        ev.obsolete.controlID = axis;
        ev.isAxis = true;

        B_Bind(&ev, NULL, -1, bc);
		return true;
	}

    name = ctrlptr;
	// A minus in front of the control name means inversion.
	if(name[0] == '-')
	{
		invert = true;
		name = &name[1];
	}

	memset(ctlName, 0, sizeof(ctlName));
	strncpy(ctlName, name, sizeof(ctlName) - 1);
	
	ptr = strchr(ctlName, '/');
	if(ptr)
	{
		local = strtol(ptr + 1, NULL, 10);
		if(local < 0 || local >= DDMAXPLAYERS)
            local = 0;
		*ptr = 0;
	}

    ctlidx = P_ControlFindAxis(ctlName);
    if(ctlidx == 0)
    {
        Con_Printf("'%s' is not a valid axis control.\n", ctlName);
        return false;
    }

	// Create the binding.
    {
    ddevent_t ev;

    ev.deviceID = deviceID;
    ev.obsolete.controlID = axis;
    ev.isAxis = true;

    bind = B_Bind(&ev, NULL, ctlidx - 1, bc);
    ctrl = &bind->binds[bc].data.axiscontrol;
    ctrl->localPlayer = local;
	ctrl->invert = invert;
    }
	*/
	return true;
}

/**
 * (safe)bind(r) bindclass +space +jump
 */
D_CMD(Bind)
{/*
    boolean     prefixGiven = true;
    boolean     bindClassGiven = false;
    char        validEventName[16], buff[80];
    char        prefix = '+', *begin;
    char       *evntptr = argv[2];
    char       *cmdptr = argv[3];
    ddevent_t   event;
    int         repeat = !stricmp(argv[0], "bindr") ||
                    !stricmp(argv[0], "safebindr");
    int         safe = !strnicmp(argv[0], "safe", 4);
    uint        bc = 0;
    uint        i;
    binding_t *existing;

    if(argc < 2 || argc > 4)
    {
        Con_Printf("Usage: %s (class) (event) (cmd)\n", argv[0]);
        Con_Printf("Binding Classes:\n");
        for(i = 0; i < numBindClasses; ++i)
            Con_Printf("  %s\n", bindClasses[i].name);

        return true;
    }

    // Check for a specified binding class
    bindClassGiven = B_GetBindClassIDbyName(argv[1], &bc);

    // Has a binding class been specified?
    if(!bindClassGiven)
    {
        if(argc == 4)
        {
            Con_Printf("'%s' is not a valid bindClass name/id.\n", argv[1]);
            return false;
        }

        // No it hasn't! default to normal
        bc = DDBC_NORMAL;
        evntptr = argv[1];
        cmdptr = argv[2];
    }

    begin = evntptr;
    if(strlen(evntptr) > 1) // Can the event have a prefix?
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

    if((argc == 3 && bindClassGiven) || (argc == 2 && !bindClassGiven))
    {
        // We're clearing a binding.
        // If no prefix has been given event states (+,-,*) are cleared.
        if(!prefixGiven)
        {
            uint    i;
            for(i = 0; i < NUM_EVENT_STATES; ++i)
            {
                sprintf(buff, "%c%s", evStatePrefixes[i], evntptr);
                eventBuilder(buff, &event);
                B_Bind(&event, NULL, -1, bc);
            }
        }
        else
        {
            eventBuilder(evntptr, &event);
            B_Bind(&event, NULL, -1, bc);
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
            if(P_IsValidControl(buff))
            {
                eventBuilder(validEventName, &event);
                if(safe && (existing = B_GetBinding(event.deviceID, event.controlID, false, false)))
                {
                    bindcommand_t *com = &existing->binds[bc].data.command;

                    if(com->command[event.data1])
                        return false;
                }

                B_Bind(&event, buff, -1, bc);
                sprintf(validEventName, "+%s", evntptr);
                sprintf(buff, "+%s", cmdptr);
                eventBuilder(validEventName, &event);
                B_Bind(&event, buff, -1, bc);
                return true;
            }
        }
    }
    sprintf(validEventName, "%c%s", prefix, begin);

//Con_Printf("Binding %s : %s.\n", validEventName,
//           argc==2? "(nothing)" : cmdptr);

    // Convert the name to an event.
    eventBuilder(validEventName, &event);
    if(safe && (existing = B_GetBinding(event.deviceID, event.controlID, false, false)))
    {
        bindcommand_t *com = &existing->binds[bc].data.command;

        if(com->command[event.data1])
            return false;
    }

    // Now we can create a binding for it.
    if((argc == 2 && !bindClassGiven && !prefixGiven) ||
       (argc == 3 && bindClassGiven))
        B_Bind(&event, NULL, -1, bc);
    else
        B_Bind(&event, cmdptr, -1, bc);

    // A repeater?
    if(repeat && event.deviceID == IDEV_KEYBOARD && event.data1 == EVS_DOWN)
    {
        event.data1 = EVS_REPEAT;

        if((argc == 2 && !bindClassGiven && !prefixGiven) ||
           (argc == 3 && bindClassGiven))
            B_Bind(&event, NULL, -1, bc);
        else
            B_Bind(&event, cmdptr, -1, bc);
    }*/
    return true;
}

D_CMD(DeleteBind)
{/*
    Con_Printf("%s is not currently implemented\n", argv[0]);
   */ return true;
}

D_CMD(ListBindClasses)
{/*
    uint        k;

    // Show the available binding classes
    Con_Printf("Binding Classes:\n");

    for(k = 0; k < numBindClasses; ++k)
        Con_Printf("  %s\n", bindClasses[k].name);

   */ return true;
}

/**
 * List all control bindings for all devices (including inactive devices).
 */
/*
D_CMD(ListBindings)
{
    uint        i, g, comcount, bindClass = 0;
    char       *searchKey;
    uint       *num;
    binding_t **list;
    uint        totalBinds;
    devcontrolbinds_t *devBinds;
    inputdev_t *device;
    boolean     inClassOnly = false;

    // Are we showing bindings in a particular class only?
    searchKey = NULL;
    if(argc >= 2)
    {
        for(i = 0; i < numBindClasses; ++i)
            if(!stricmp(argv[1], bindClasses[i].name))
            {
                // Only show bindings in this class.
                bindClass = bindClasses[i].id;
                inClassOnly= true;
            }

        if(!inClassOnly)
            searchKey = argv[1];
        else if(argc >= 3)
            searchKey = argv[2];
    }

    totalBinds = 0;
    comcount = 0;
    for(g = 0; g < NUM_INPUT_DEVICES; ++g)
    {
        uint        l;

        device = I_GetDevice(g, false);
        if(!device)
            continue;

        devBinds = &devCtrlBinds[g];

        for(l = 0; l < NUM_BIND_LISTS; ++l)
        {
            // Skip the default binds.
//            if(l == BL_KEYSD || l == BL_AXESD)
//                continue;

            list = &devBinds->binds[l];
            num  = &devBinds->numBinds[l];
            totalBinds += *num;
            if(*list)
                comcount += printBindList(searchKey, g, (inClassOnly? bindClass:-1), *list, *num);
        }
    }

    if(inClassOnly)
    {
        Con_Printf("Showing %i (%s class) commands from %i bindings.\n",
                   comcount, bindClasses[bindClass].name, totalBinds);
    }
    else
    {
        Con_Printf("Showing %i commands from %i bindings.\n",
                   comcount, totalBinds);
    }
    return true;
}
*/

/**
 * Enables/disables binding classes.
 * Ques extra input events as required.
 */
D_CMD(EnableBindClass)
{/*
    uint        i, idx = 0;
    boolean     found;

    if(argc < 2 || argc > 3)
    {
        for(i = 0; i < numBindClasses; ++i)
            Con_Printf("%d: %s is %s\n", i, bindClasses[i].name,
                       (bindClasses[i].active)? "On" : "Off");

        Con_Printf("Usage: %s (binding class) (1=On, 0=Off (omit to toggle))\n",
                   argv[0]);
        return true;
    }

    // Look for a binding class with a name that matches the argument.
    i = 0;
    found = false;
    while(i < numBindClasses && !found)
    {
        if(!(stricmp(argv[1], bindClasses[i].name)))
        {
            idx = bindClasses[i].id;
            found = true;
        }
        else
            i++;
    }

    if(!found || idx >= numBindClasses)
    {
        Con_Printf("Not a valid binding class. Enter listbindclasses.\n");
        return false;
    }

    if(B_SetBindClass(idx, (argc == 3)? atoi(argv[2]) : 2))
        return true;
    else
        return false;
*/    return true;
}
