/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * b_main.c: Event/Command Binding
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"
#include "de_play.h"

#include "ui/b_command.h"
#include "ui/p_control.h"
#include "ui/ui_main.h"

#include <ctype.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    int             key; // DDKEY
    char*           name;
} keyname_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(BindEventToCommand);
D_CMD(BindControlToDevice);
D_CMD(ListBindings);
D_CMD(ListBindingContexts);
//D_CMD(ClearBindingContexts);
D_CMD(ClearBindings);
D_CMD(DeleteBindingById);
D_CMD(ActivateBindingContext);
D_CMD(DefaultBindings);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int symbolicEchoMode = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int bindingIdCounter;

static const keyname_t keyNames[] = {
    {DDKEY_PAUSE,       "pause"},
    {DDKEY_ESCAPE,      "escape"},
    {DDKEY_ESCAPE,      "esc"},
    {DDKEY_RIGHTARROW,  "right"},
    {DDKEY_LEFTARROW,   "left"},
    {DDKEY_UPARROW,     "up"},
    {DDKEY_DOWNARROW,   "down"},
    {DDKEY_RETURN,      "return"},
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
    {'/',               "slash"},
    {DDKEY_BACKSLASH,   "backslash"},
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
    {DDKEY_CAPSLOCK,    "capslock"},
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
    {DDKEY_DECIMAL,     "decimal"},
    {DDKEY_DECIMAL,     "padcomma"},
    {DDKEY_SUBTRACT,    "padminus"},
    {DDKEY_ADD,         "padplus"},
    {DDKEY_PRINT,       "print"},
    {DDKEY_PRINT,       "prtsc"},
    {DDKEY_ENTER,       "enter"},
    {DDKEY_DIVIDE,      "divide"},
    {DDKEY_MULTIPLY,    "multiply"},
    {DDKEY_SECTION,     "section"},
    {0, NULL} // The terminator
};

// CODE --------------------------------------------------------------------

void B_Register(void)
{
    extern byte zeroControlUponConflict;

    C_VAR_BYTE("input-conflict-zerocontrol", &zeroControlUponConflict, 0, 0, 1);

#define PROTECTED_FLAGS     (CMDF_NO_DEDICATED|CMDF_DED|CMDF_CLIENT)

    C_CMD_FLAGS("bindevent",      "ss",   BindEventToCommand, PROTECTED_FLAGS);
    C_CMD_FLAGS("bindcontrol",    "ss",   BindControlToDevice, PROTECTED_FLAGS);
    C_CMD_FLAGS("listbcontexts",  NULL,   ListBindingContexts, PROTECTED_FLAGS);
    C_CMD_FLAGS("listbindings",   NULL,   ListBindings, PROTECTED_FLAGS);
    C_CMD_FLAGS("clearbindings",  "",     ClearBindings, PROTECTED_FLAGS);
    //C_CMD_FLAGS("clearbcontexts", "",     ClearBindingContexts, PROTECTED_FLAGS);
    C_CMD_FLAGS("delbind",        "i",    DeleteBindingById, PROTECTED_FLAGS);
    C_CMD_FLAGS("defaultbindings", "",    DefaultBindings, PROTECTED_FLAGS);
    C_CMD_FLAGS("activatebcontext", "s",  ActivateBindingContext, PROTECTED_FLAGS);
    C_CMD_FLAGS("deactivatebcontext", "s", ActivateBindingContext, PROTECTED_FLAGS);

#undef PROTECTED_FLAGS
}

/**
 * Binding context fallback for the "global" context.
 *
 * @param ddev  Event being processed.
 *
 * @return @c true, if the event was eaten and can be processed by the rest of
 * the binding context stack.
 */
static int globalContextFallback(const ddevent_t* ddev)
{
#ifdef __CLIENT__
    if(UI_Responder(ddev)) return true;     // Eaten.
#endif
    if(Con_Responder(ddev)) return true;    // Eaten.

    if(DD_GameLoaded())
    {
        event_t ev;
        DD_ConvertEvent(ddev, &ev);

        // The game's normal responder only returns true if the bindings can't
        // be used (like when chatting). Note that if the event is eaten here,
        // the rest of the bindings contexts won't get a chance to process the
        // event.
        if(gx.Responder && gx.Responder(&ev))
            return true;
    }

    return false;
}

/**
 * Called once on init.
 */
void B_Init(void)
{
    bcontext_t* bc = 0;

    // In dedicated mode we have fewer binding contexts available.
    if(!isDedicated)
    {
        // The contexts are defined in reverse order, with the context of lowest
        // priority defined first.

        B_NewContext(DEFAULT_BINDING_CONTEXT_NAME);

        // Game contexts.
        /// @todo Game binding context setup obviously belong to the game plugin, so shouldn't be here.
        B_NewContext("map");
        B_NewContext("map-freepan");
        B_NewContext("finale"); // uses a fallback responder to handle script events
        B_AcquireAll(B_NewContext("menu"), true);
        B_NewContext("gameui");
        B_NewContext("shortcut");
        B_AcquireKeyboard(B_NewContext("chat"), true);
        B_AcquireAll(B_NewContext("message"), true);

        // Binding context for the console.
        bc = B_NewContext(CONSOLE_BINDING_CONTEXT_NAME);
        bc->flags |= BCF_PROTECTED; // Only we can (de)activate.
        B_AcquireKeyboard(bc, true); // Console takes over all keyboard events.

        // UI doesn't let anything past it.
        B_AcquireAll(bc = B_NewContext(UI_BINDING_CONTEXT_NAME), true);
    }

    // Top-level context that is always active and overrides every other context.
    // To be used only for system-level functionality.
    bc = B_NewContext(GLOBAL_BINDING_CONTEXT_NAME);
    bc->flags |= BCF_PROTECTED;
    bc->ddFallbackResponder = globalContextFallback;
    B_ActivateContext(bc, true);

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

    // Bind all the defaults for the engine only.
    B_BindDefaults();

    B_InitialContextActivations();
}

void B_InitialContextActivations(void)
{
    int i;

    // Disable all contexts.
    for(i = 0; i < B_ContextCount(); ++i)
    {
        B_ActivateContext(B_ContextByPos(i), false);
   }

    // These are the contexts active by default.
    B_ActivateContext(B_ContextByName(GLOBAL_BINDING_CONTEXT_NAME), true);
    B_ActivateContext(B_ContextByName(DEFAULT_BINDING_CONTEXT_NAME), true);

    if(Con_IsActive())
    {
        B_ActivateContext(B_ContextByName(CONSOLE_BINDING_CONTEXT_NAME), true);
    }
}

void B_BindDefaults(void)
{
    // Engine's highest priority context: opening control panel, opening the console.
    B_BindCommand("global:key-f11-down + key-alt-down", "releasemouse");
    B_BindCommand("global:key-f11-down", "togglefullscreen");

    // Console bindings (when open).

    // Bias editor.
}

void B_BindGameDefaults(void)
{
    if(!DD_GameLoaded()) return;
    Con_Executef(CMDS_DDAY, false, "defaultgamebindings");
}

/**
 * Deallocates the memory for the commands and bindings.
 */
void B_Shutdown(void)
{
    B_DestroyAllContexts();
}

/**
 * @return  Never returns zero, as that is reserved for list roots.
 */
int B_NewIdentifier(void)
{
    int                 id = 0;

    while(!id)
    {
        id = ++bindingIdCounter;
    }

    return id;
}

const char* B_ParseContext(const char* desc, bcontext_t** bc)
{
    AutoStr* str;

    *bc = 0;
    if(!strchr(desc, ':'))
    {
        // No context defined.
        return desc;
    }

    str = AutoStr_NewStd();
    desc = Str_CopyDelim(str, desc, ':');
    *bc = B_ContextByName(Str_Text(str));

    return desc;
}

void B_DeleteMatching(bcontext_t* bc, evbinding_t* eventBinding,
                      dbinding_t* deviceBinding)
{
    dbinding_t*         devb = NULL;
    evbinding_t*        evb = NULL;

    while(B_FindMatchingBinding(bc, eventBinding, deviceBinding, &evb, &devb))
    {
        // Only either evb or devb is returned as non-NULL.
        int bid = (evb? evb->bid : (devb? devb->bid : 0));

        if(bid)
        {
            Con_Message("B_BindCommand: Deleting binding %i, it has been overridden by "
                        "binding %i.\n", bid, eventBinding? eventBinding->bid : deviceBinding->bid);
            B_DeleteBinding(bc, bid);
        }
    }
}

evbinding_t* B_BindCommand(const char* eventDesc, const char* command)
{
    bcontext_t*         bc;
    evbinding_t*        b;

    if(isDedicated)
        return NULL;

    // The context may be included in the descriptor.
    eventDesc = B_ParseContext(eventDesc, &bc);
    if(!bc)
    {
        bc = B_ContextByName(DEFAULT_BINDING_CONTEXT_NAME);
    }

    if((b = B_NewCommandBinding(&bc->commandBinds, eventDesc, command)) != NULL)
    {
        /**
         * \todo: In interactive binding mode, should ask the user if the
         * replacement is ok. For now, just delete the other binding.
         */
        B_DeleteMatching(bc, b, NULL);
        B_UpdateDeviceStateAssociations();
    }

    return b;
}

dbinding_t* B_BindControl(const char* controlDesc, const char* device)
{
    bcontext_t*         bc = 0;
    int                 localNum = 0;
    controlbinding_t*   conBin = 0;
    dbinding_t*         devBin = 0;
    AutoStr*            str = 0;
    const char*         ptr = 0;
    playercontrol_t*    control = 0;
    boolean             justCreated = false;

    if(isDedicated)
        return NULL;

    // The control description may begin with the local player number.
    str = AutoStr_NewStd();
    ptr = Str_CopyDelim(str, controlDesc, '-');
    if(!strncasecmp(Str_Text(str), "local", 5) && Str_Length(str) > 5)
    {
        localNum = strtoul(Str_Text(str) + 5, NULL, 10) - 1;
        if(localNum < 0 || localNum >= DDMAXPLAYERS)
        {
            Con_Message("B_BindControl: Local player number %i is invalid.\n", localNum);
            return NULL;
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
        return NULL;
    }

    bc = B_ContextByName(control->bindContextName);
    if(!bc)
    {
        bc = B_ContextByName(DEFAULT_BINDING_CONTEXT_NAME);
    }
    VERBOSE( Con_Message("B_BindControl: Control '%s' in context '%s' of local player %i to be "
                         "bound to '%s'.\n", control->name, bc->name, localNum, device) );

    if((conBin = B_FindControlBinding(bc, control->id)) == NULL)
    {
        justCreated = true;
        conBin = B_GetControlBinding(bc, control->id);
    }

    if(!(devBin = B_NewDeviceBinding(&conBin->deviceBinds[localNum], device)))
    {
        // Failure in the parsing.
        if(justCreated)
        {
            B_DestroyControlBinding(conBin);
        }
        conBin = 0;
        return NULL;
    }

    /**
     * \todo: In interactive binding mode, should ask the user if the
     * replacement is ok. For now, just delete the other binding.
     */
    B_DeleteMatching(bc, NULL, devBin);
    B_UpdateDeviceStateAssociations();

    return devBin;
}

dbinding_t* B_GetControlDeviceBindings(int localNum, int control,
                                       bcontext_t** bContext)
{
    playercontrol_t*    pc;
    bcontext_t*         bc;

    if(localNum < 0 || localNum >= DDMAXPLAYERS)
        return NULL;

    pc = P_PlayerControlById(control);
    bc = B_ContextByName(pc->bindContextName);
    if(bContext)
        *bContext = bc;

    if(bc)
        return &B_GetControlBinding(bc, control)->deviceBinds[localNum];

    return NULL;
}

boolean B_Delete(int bid)
{
    int                 i;

    for(i = 0; i < B_ContextCount(); ++i)
    {
        if(B_DeleteBinding(B_ContextByPos(i), bid))
            return true;
    }

    return false;
}

D_CMD(BindEventToCommand)
{
    evbinding_t*        b = B_BindCommand(argv[1], argv[2]);

    if(b)
    {
        VERBOSE( Con_Printf("Binding %i created.\n", b->bid) );
    }

    return (b != NULL);
}

D_CMD(BindControlToDevice)
{
    dbinding_t*         b = B_BindControl(argv[1], argv[2]);

    if(b)
    {
        VERBOSE( Con_Printf("Binding %i created.\n", b->bid) );
    }

    return (b != NULL);
}

D_CMD(ListBindingContexts)
{
    B_PrintContexts();
    return true;
}

D_CMD(ListBindings)
{
    B_PrintAllBindings();
    return true;
}

/*
D_CMD(ClearBindingContexts)
{
    B_DestroyAllContexts();
    return true;
}
*/

D_CMD(ClearBindings)
{
    int i;

    for(i = 0; i < B_ContextCount(); ++i)
    {
        Con_Printf("Clearing binding context '%s'...\n", B_ContextByPos(i)->name);
        B_ClearContext(B_ContextByPos(i));
    }

    // We can restart the id counter, all the old bindings were destroyed.
    bindingIdCounter = 0;
    return true;
}

D_CMD(DeleteBindingById)
{
    int                 bid = strtoul(argv[1], NULL, 10);

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
    B_BindGameDefaults();

    return true;
}

D_CMD(ActivateBindingContext)
{
    boolean doActivate = !stricmp(argv[0], "activatebcontext");
    bcontext_t* bc = B_ContextByName(argv[1]);

    if(!bc)
    {
        Con_Printf("Binding context '%s' does not exist.\n", argv[1]);
        return false;
    }

    if(bc->flags & BCF_PROTECTED)
    {
        Con_Message("Binding Context '%s' is protected. It can not be manually %s.\n", bc->name,
                    doActivate? "activated" : "deactivated");
        return false;
    }

    B_ActivateContext(bc, doActivate);
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

static uint searchBindListForControlID(binding_t** list, uint num,
                                       int controlID)
{
    uint                i;
    boolean             found;

    i = 0;
    found = false;
    while(i < num && !found)
    {
        // @todo Use a faster than O(n) linear search.
        if(controlID == (*list)[i].controlID)
            found = true;
        else
            i++;
    }
    if(found)
        return i+1; // Index + 1.

    return 0;
}

static binding_t __inline* bindingForEvent(ddevent_t* ev)
{
    uint                num, idx;
    binding_t**         list;
    devcontrolbinds_t*  devBinds = &devCtrlBinds[ev->device];

    list = &devBinds->binds[ev->isAxis? BL_AXES : BL_KEYS];
    num  = devBinds->numBinds[ev->isAxis? BL_AXES : BL_KEYS];

    idx = searchBindListForControlID(list, num, (int) ev->obsolete.controlID);
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
 * @return              Ptr to the found bindcontrol_t ELSE @c NULL,.
 */
static bindcontrol_t* B_GetBindControlForEvent(ddevent_t* ev)
{
    uint                i;
    binding_t*          bnd;
    bindcontrol_t*      ctrl = NULL;
    boolean             found;

    if(!I_GetDevice(ev->device, true))
        return NULL;

    bnd = bindingForEvent(ev);
    if(bnd == NULL)
        return NULL;

    found = false;
    if(!ev->noclass) // Use a specific context? (active or not)
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
        // command in the highest binding context slot that is currently
        // active is executed.
        i = 0;
        done = false;
        while(i < numBindClasses && !done)
        {
            idx = numBindClasses - 1 - i;

            if(bindContexts[idx].active == 1)
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
                        // a down binding for this event in this context.
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
                    if(bindContexts[idx].flags & BCF_ABSOLUTE)
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
 * @return              @c true, If an action was executed.
 */
boolean B_Responder(ddevent_t* ev)
{
    if(symbolicEchoMode && ev->type != E_SYMBOLIC)
    {
        // Make an echo.
        ddstring_t name;
        ddevent_t echo;

        // Axis events need a bit of filtering.
        if(ev->type == E_AXIS)
        {
            float pos = I_TransformAxis(I_GetDevice(ev->device, false), ev->axis.id, ev->axis.pos);
            if((ev->axis.type == EAXIS_ABSOLUTE && fabs(pos) < .5f) ||
               (ev->axis.type == EAXIS_RELATIVE && fabs(pos) < .02f))
            {
                // Not significant enough for an echo.
                return B_TryEvent(ev);
            }
        }

        Str_Init(&name);
        Str_Set(&name, "echo-");
        B_AppendEventToString(ev, &name);
        echo.device = ev->device;
        echo.type = E_SYMBOLIC;
        echo.symbolic.id = 0;
        echo.symbolic.name = Str_Text(&name);
        VERBOSE( Con_Message("B_Responder: Symbolic echo: %s\n", echo.symbolic.name) );
        DD_PostEvent(&echo);
        Str_Free(&name);
        return true;
    }

    return B_TryEvent(ev);
}

#if 0
/**
 * Retrieve a binding for the given device control.
 *
 * @param deviceID      Device ident for the binding.
 * @param controlID     Device control index for the binding, either a
 *                      key/button number or axis index number.
 * @param isAxis        If @c true, @param controlID is an axis
 *                      index number.
 * @param createNew     If @c true, a new binding_t will be
 *                      allocated if an existing one cannot be found.
 *
 * @return              Binding for the given event OR @c NULL,
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
 * @param active    If @c true, clear those from active lists.
 * @param default   If @c true, clear those from defaults lists.
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
 * 1) Binding to NULL without specifying a context: deletes the binding.
 *
 * 2) Bindind to NULL and specifying a context: clears the command and
 *    if no more commands exist for this binding - will delete it
 */
binding_t *B_Bind(ddevent_t *ev, char *command, int control, uint bindContext)
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

        // Clear the command in bindContext only.
        for(i = 0; i < numBindClasses; ++i)
        {
            if(bnd->binds[i].type == BND_UNUSED)
                continue;

            if(i == bindContext)
            {
                unused = true;
                // Implicit; axis bindings which match bindContext are unused.

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

    if(bnd->binds[bindContext].type == BND_COMMAND)
    {
        // If, changing from a command to an axis bind, free all commands
        // in all states. Else, just free the command in the state being
        // updated (if one already present).
        com = &bnd->binds[bindContext].data.command;
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
    bnd->binds[bindContext].type = (ev->isAxis? BND_AXIS : BND_COMMAND);
    if(bnd->binds[bindContext].type == BND_AXIS)
    {
        ctrl = &bnd->binds[bindContext].data.axiscontrol;
        ctrl->playercontrol = control;
//#if _DEBUG
//Con_Printf("B_Bind: (%s) axis '%s' ctrl: '%s'\n",
//           device->name, I_GetAxisByID(device, bnd->obsolete.controlID)->name,
//           P_ControlGetAxisName(control));
//#endif
    }
    else
    {
        com = &bnd->binds[bindContext].data.command;
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

const char* B_ShortNameForKey2(int ddKey, boolean forceLowercase)
{
    static char nameBuffer[40];
    uint idx;

    for(idx = 0; keyNames[idx].key; ++idx)
    {
        if(ddKey == keyNames[idx].key)
            return keyNames[idx].name;
    }

    if(isalnum(ddKey))
    {
        // Printable character, fabricate a single-character name.
        nameBuffer[0] = forceLowercase? tolower(ddKey) : ddKey;
        nameBuffer[1] = 0;
        return nameBuffer;
    }

    return NULL;
}

const char* B_ShortNameForKey(int ddKey)
{
    return B_ShortNameForKey2(ddKey, true/*force lowercase*/);
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

#if 0 // Currently unused.
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
#endif

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
 * @param  controlID   Usage depends on @c type, eg keynumber.
 * @param  isAxis      If @c true, @param controlID is treated
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
 * Retrieve the id of the named bind context.
 *
 * @param           The symbolic bind context name to search for OR identifier
 *                  in the form "bdc#" where '#' = bind context id.
 *
 * @return          @c true, if one is found.
 */
static boolean B_GetBindClassIDbyName(const char *name, uint *id)
{
    uint        i;
    boolean     found = false;

    if(!name || !name[0])
        return false;

    // By bindContext id first.
    if(!strnicmp(name, "bdc", 3))
    {
        uint        idx = (uint) atoi(name+3);
        i = 0;
        while(i < numBindClasses && !found)
        {
            if(idx == bindContexts[i].id)
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
            if(!stricmp(name, bindContexts[i].name))
            {
                if(id)
                    *id = bindContexts[i].id;
                found = true;
            }
            else
                ++i;
        }
    }

    return found;
}
#endif

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
                // being pressed that have a binding in the context being
                // enabled/disabled (classID)
                if(!(com->command[EVS_DOWN] != NULL && bind->controlID >= 0 &&
                     I_IsKeyDown(dev, (uint) bind->controlID)))
                    continue;
                break;

            case BND_AXIS:
                axis = &bind->binds[classID].data.axiscontrol;

                // We're only interested in bindings for axes which are
                // currently outside their dead zone, that have a binding in the
                // context being enabled/disabled (classID)
                // @todo Actually check the zone!
                if(!dev->axes[axis->playercontrol].position)
                    continue;
                break;
            };

            // Iterate all commands for this binding, count the number of
            // commands for this binding that are for currently active bind
            // classes with a lower id than the context being enabled/disabled
            // (classID).

            count = 0;
            k = 0;
            isDone = false;
            while(k < numBindClasses && !isDone)
            {
                if(bindContexts[k].active && bind->binds[k].type != BND_UNUSED)
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
                        // context that is currently active (current is k), that
                        // has a greater id than the context being enabled/disabled
                        // (classID) then we don't need to que any extra events
                        // at all as that will have been done when the binding
                        // context with the higher id was enabled. The commands in
                        // the lower classes can't have been active (for this
                        // event), as the highest context command is ALWAYS
                        // executed unless a specific context is requested.
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
                // bind context for all the bind classes with a lower id than the
                // context being enabled/disabled (classID) that are also active
                // (note the order does not matter).
                for(k = 0; k < classID; ++k)
                {
                    if(bindContexts[k].active &&
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

                            // Specify a bind context.
                            ev.useclass = bindContexts[k].id;
                            ev.noclass = false;

                            DD_PostEvent(&ev);
                        }
                    }
                }
            }

            // Also send an up/center event for this binding if the currently
            // active command is in the context being disabled and it has the
            // highest id of the active bindContext commands for this binding.
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
                        if(idx > classID && bindContexts[idx].active)
                        {
                            isDone = true;
                        }
                        else
                        {
                            if(!bindContexts[idx].active)
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

                                // Specify a bind context.
                                ev.useclass = bindContexts[idx].id;
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

#if 0
/**
 * Enables/disables binding classes
 * Ques extra input events as required
 */
boolean B_SetBindClass(unsigned int classID, unsigned int type)
{
    uint        g;

    // Change the active state of the bindContext.
    switch(type)
    {
    case 0:  // implicitly set
    case 1:
        bindContexts[classID].active = type? 1 : 0;
        break;

    case 2:  // toggle
        bindContexts[classID].active = bindContexts[classID].active? 0 : 1;
        break;

    default:
        Con_Error("B_SetBindClass: Unknown state change value %i", type);
    }

    VERBOSE2(Con_Printf("B_SetBindClass: %s %s %s.\n",
                        bindContexts[classID].name,
                        (type==2)? "TOGGLE" : "SET",
                        bindContexts[classID].active? "ON" : "OFF"));

    // Now we need to do a check in case there are keys currently
    // being pressed that should be released if the event binding they are
    // bound too has commands in the bind context being enabled/disabled.
    // Also, if there are any axes which are outside their dead zone and
    // if there axis bindings in the bind context being enabled/disabled, we
    // need to send centering events for them.

    for(g = 0; g < NUM_INPUT_DEVICES; ++g)
        queEventsForHeldControls(g, classID);

    return true;
}
#endif

/*
static uint writeBindList(FILE *file, binding_t *list, uint num,
                          uint deviceID, uint bindContext)
{
    uint        i, j, count = 0;
    binding_t  *bnd;
    bindcontrol_t *ctrl;
    char        buffer[20];

    for(i = 0, bnd = list; i < num; ++i, bnd++)
    {
        ctrl = &bnd->binds[bindContext];
        switch(ctrl->type)
        {
        case BND_AXIS:
            {
            bindaxis_t *axis = &ctrl->data.axiscontrol;

            formEventString(buffer, deviceID, bnd->controlID, true, 0);
            // @todo Using "after" is a hack...
            fprintf(file, "after 1 { bindaxis %s %s ",
                    bindContexts[bindContext].name, buffer);
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
                    fprintf(file, "%s ", bindContexts[bindContext].name);
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
 * Dump all the bindings to a text (cfg) file. Outputs console commands.
 */
void B_WriteToFile(FILE* file)
{
    int                 i;

    // Start with a clean slate when restoring the bindings.
    fprintf(file, "clearbindings\n\n");

    for(i = 0; i < B_ContextCount(); ++i)
    {
        B_WriteContextToFile(B_ContextByPos(i), file);
    }
}

/**
 * Return the key code that corresponds the given key identifier name.
 * Part of the Doomsday public API.
 */
#undef DD_GetKeyCode
int DD_GetKeyCode(const char* key)
{
    int code = B_KeyForShortName(key);
    return (code ? code : key[0]);
}

/*
static uint printBindList(char *searchKey, uint deviceID, int bindContext,
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
                        if(bindContext >= 0)
                        {
                            if(j != (uint) bindContext)
                                noMatch = true;
                        }
                        if(!noMatch)
                            if(searchKey &&
                               strnicmp(buffer + 1, searchKey, strlen(searchKey)))
                                noMatch = true;

                        if(!noMatch)
                        {
                            if(bindContext >= 0)
                                Con_Printf("%-8s : %s\n", buffer,
                                           com->command[k]);
                            else
                                Con_Printf("%-8s : %-8s : %s\n", buffer,
                                           bindContexts[j].name,
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
                if(bindContext >= 0)
                {
                    if(j != (uint) bindContext)
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

                    if(bindContext >= 0)
                        Con_Printf("%-8s : %s%s\n", buffer,
                                   (ctl->invert? "-" : ""),
                                   axisName);
                    else
                        Con_Printf("%-8s : %-8s : %s%s\n", buffer,
                                   bindContexts[j].name,
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

#if 0
/**
 * The "bindaxis" console command creates and deletes axis bindings.
 *
 * Example:  bindaxis bindcontext mouse-y (-)look/2
 */
D_CMD(BindAxis)
{
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
    boolean     bindContextGiven = false;
    bindaxis_t *ctrl;

    if(argc < 3 || argc > 4)
    {
        Con_Printf("Usage: %s (context) (device-axis) (control)\n", argv[0]);
        Con_Printf("Binding Classes:\n");
        for(i = 0; i < numBindClasses; ++i)
            Con_Printf("  %s\n", bindContexts[i].name);
        return true;
    }

    // Check for a specified binding context.
    bindContextGiven = B_GetBindClassIDbyName(argv[1], &bc);

    if(argc == 4 && !bindContextGiven)
    {
        Con_Printf("'%s' is not a valid bindContext name/id.\n", argv[1]);
        return false;
    }

    // Has a binding context been specified?
    if(!bindContextGiven)
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
    if(argc == 3 && bindContextGiven)
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

    return true;
}

/**
 * (safe)bind(r) bindcontext +space +jump
 */
D_CMD(Bind)
{
    boolean     prefixGiven = true;
    boolean     bindContextGiven = false;
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
        Con_Printf("Usage: %s (context) (event) (cmd)\n", argv[0]);
        Con_Printf("Binding Classes:\n");
        for(i = 0; i < numBindClasses; ++i)
            Con_Printf("  %s\n", bindContexts[i].name);

        return true;
    }

    // Check for a specified binding context
    bindContextGiven = B_GetBindClassIDbyName(argv[1], &bc);

    // Has a binding context been specified?
    if(!bindContextGiven)
    {
        if(argc == 4)
        {
            Con_Printf("'%s' is not a valid bindContext name/id.\n", argv[1]);
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

    if((argc == 3 && bindContextGiven) || (argc == 2 && !bindContextGiven))
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
    if(argc == 4 || (argc == 3 && !bindContextGiven))
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
    if((argc == 2 && !bindContextGiven && !prefixGiven) ||
       (argc == 3 && bindContextGiven))
        B_Bind(&event, NULL, -1, bc);
    else
        B_Bind(&event, cmdptr, -1, bc);

    // A repeater?
    if(repeat && event.deviceID == IDEV_KEYBOARD && event.data1 == EVS_DOWN)
    {
        event.data1 = EVS_REPEAT;

        if((argc == 2 && !bindContextGiven && !prefixGiven) ||
           (argc == 3 && bindContextGiven))
            B_Bind(&event, NULL, -1, bc);
        else
            B_Bind(&event, cmdptr, -1, bc);
    }*/
    return true;
}

D_CMD(DeleteBind)
{
    Con_Printf("%s is not currently implemented\n", argv[0]);
    return true;
}

D_CMD(ListBindClasses)
{
    uint        k;

    // Show the available binding classes
    Con_Printf("Binding Classes:\n");

    for(k = 0; k < numBindClasses; ++k)
        Con_Printf("  %s\n", bindContexts[k].name);

    return true;
}

/**
 * List all control bindings for all devices (including inactive devices).
 */
D_CMD(ListBindings)
{
    uint        i, g, comcount, bindContext = 0;
    char       *searchKey;
    uint       *num;
    binding_t **list;
    uint        totalBinds;
    devcontrolbinds_t *devBinds;
    inputdev_t *device;
    boolean     inClassOnly = false;

    // Are we showing bindings in a particular context only?
    searchKey = NULL;
    if(argc >= 2)
    {
        for(i = 0; i < numBindClasses; ++i)
            if(!stricmp(argv[1], bindContexts[i].name))
            {
                // Only show bindings in this context.
                bindContext = bindContexts[i].id;
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
                comcount += printBindList(searchKey, g, (inClassOnly? bindContext:-1), *list, *num);
        }
    }

    if(inClassOnly)
    {
        Con_Printf("Showing %i (%s context) commands from %i bindings.\n",
                   comcount, bindContexts[bindContext].name, totalBinds);
    }
    else
    {
        Con_Printf("Showing %i commands from %i bindings.\n",
                   comcount, totalBinds);
    }
    return true;
}

/**
 * Enables/disables binding classes.
 * Ques extra input events as required.
 */
D_CMD(EnableBindContext)
{
    uint        i, idx = 0;
    boolean     found;

    if(argc < 2 || argc > 3)
    {
        for(i = 0; i < numBindClasses; ++i)
            Con_Printf("%d: %s is %s\n", i, bindContexts[i].name,
                       (bindContexts[i].active)? "On" : "Off");

        Con_Printf("Usage: %s (binding context) (1=On, 0=Off (omit to toggle))\n",
                   argv[0]);
        return true;
    }

    // Look for a binding context with a name that matches the argument.
    i = 0;
    found = false;
    while(i < numBindClasses && !found)
    {
        if(!(stricmp(argv[1], bindContexts[i].name)))
        {
            idx = bindContexts[i].id;
            found = true;
        }
        else
            i++;
    }

    if(!found || idx >= numBindClasses)
    {
        Con_Printf("Not a valid binding context. Enter listbindcontextes.\n");
        return false;
    }

    if(B_SetBindClass(idx, (argc == 3)? atoi(argv[2]) : 2))
        return true;

    return false;
}
#endif
