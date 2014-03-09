/** @file b_main.cpp Event and device state bindings system.
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include <ctype.h>
#include <math.h>

#include "de_console.h"
#include "de_misc.h"
#include "dd_main.h"
#include "dd_def.h"

#include "clientapp.h"
#include "ui/b_command.h"
#include "ui/p_control.h"
#include "ui/ui_main.h"

#include <ctype.h>

typedef struct {
    int key;            ///< DDKEY
    char const *name;
} keyname_t;

D_CMD(BindEventToCommand);
D_CMD(BindControlToDevice);
D_CMD(ListBindings);
D_CMD(ListBindingContexts);
//D_CMD(ClearBindingContexts);
D_CMD(ClearBindings);
D_CMD(DeleteBindingById);
D_CMD(ActivateBindingContext);
D_CMD(DefaultBindings);

int symbolicEchoMode = false;

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
    if(App_GameLoaded())
    {
        event_t ev;
        if(DD_ConvertEvent(ddev, &ev))
        {
            // The game's normal responder only returns true if the bindings can't
            // be used (like when chatting). Note that if the event is eaten here,
            // the rest of the bindings contexts won't get a chance to process the
            // event.
            if(gx.Responder && gx.Responder(&ev))
                return true;
        }
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

    /*
    if(Con_IsActive())
    {
        B_ActivateContext(B_ContextByName(CONSOLE_BINDING_CONTEXT_NAME), true);
    }
    */
}

void B_BindDefaults(void)
{
    // Engine's highest priority context: opening control panel, opening the console.
    B_BindCommand("global:key-f11-down + key-alt-down", "releasemouse");
    B_BindCommand("global:key-f11-down", "togglefullscreen");
    B_BindCommand("global:key-tilde-down + key-shift-up", "taskbar");

    // Console bindings (when open).
    B_BindCommand("console:key-tilde-down + key-shift-up", "taskbar"); // without this, key would be entered into command line

    // Bias editor.
}

void B_BindGameDefaults(void)
{
    if(!App_GameLoaded()) return;
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
            LOG_INPUT_VERBOSE("Deleting binding %i, it has been overridden by binding %i")
                    << bid << (eventBinding? eventBinding->bid : deviceBinding->bid);
            B_DeleteBinding(bc, bid);
        }
    }
}

evbinding_t* B_BindCommand(const char* eventDesc, const char* command)
{
    bcontext_t*         bc;
    evbinding_t*        b;

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
    dd_bool             justCreated = false;

    LOG_AS("B_BindControl");

    // The control description may begin with the local player number.
    str = AutoStr_NewStd();
    ptr = Str_CopyDelim(str, controlDesc, '-');
    if(!strncasecmp(Str_Text(str), "local", 5) && Str_Length(str) > 5)
    {
        localNum = strtoul(Str_Text(str) + 5, NULL, 10) - 1;
        if(localNum < 0 || localNum >= DDMAXPLAYERS)
        {
            LOG_INPUT_WARNING("Local player number %i is invalid") << localNum;
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
        LOG_INPUT_WARNING("Player control \"%s\" not defined") << Str_Text(str);
        return NULL;
    }

    bc = B_ContextByName(control->bindContextName);
    if(!bc)
    {
        bc = B_ContextByName(DEFAULT_BINDING_CONTEXT_NAME);
    }

    LOG_INPUT_VERBOSE("Control '%s' in context '%s' of local player %i to be bound to '%s'")
            << control->name << bc->name << localNum << device;

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

dd_bool B_Delete(int bid)
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
    DENG2_UNUSED2(src, argc);

    evbinding_t *b = B_BindCommand(argv[1], argv[2]);

    if(b)
    {
        LOG_INPUT_VERBOSE("Binding %i created") <<  b->bid;
    }

    return (b != NULL);
}

D_CMD(BindControlToDevice)
{
    DENG2_UNUSED2(src, argc);

    dbinding_t*         b = B_BindControl(argv[1], argv[2]);

    if(b)
    {
        LOG_INPUT_VERBOSE("Binding %i created") << b->bid;
    }

    return (b != NULL);
}

D_CMD(ListBindingContexts)
{
    DENG2_UNUSED3(src, argc, argv);

    B_PrintContexts();
    return true;
}

D_CMD(ListBindings)
{
    DENG2_UNUSED3(src, argc, argv);

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
    DENG2_UNUSED3(src, argc, argv);

    int i;

    for(i = 0; i < B_ContextCount(); ++i)
    {
        LOG_INPUT_VERBOSE("Clearing binding context '%s'") << B_ContextByPos(i)->name;
        B_ClearContext(B_ContextByPos(i));
    }

    // We can restart the id counter, all the old bindings were destroyed.
    bindingIdCounter = 0;
    return true;
}

D_CMD(DeleteBindingById)
{
    DENG2_UNUSED2(src, argc);

    int bid = strtoul(argv[1], NULL, 10);

    if(B_Delete(bid))
    {
        LOG_INPUT_MSG("Binding %i deleted") << bid;
    }
    else
    {
        LOG_INPUT_ERROR("Cannot delete binding %i: not found") << bid;
    }

    return true;
}

D_CMD(DefaultBindings)
{
    DENG2_UNUSED3(src, argc, argv);

    B_BindDefaults();
    B_BindGameDefaults();

    return true;
}

D_CMD(ActivateBindingContext)
{
    DENG2_UNUSED2(src, argc);

    dd_bool doActivate = !stricmp(argv[0], "activatebcontext");
    bcontext_t* bc = B_ContextByName(argv[1]);

    if(!bc)
    {
        LOG_INPUT_WARNING("Binding context '%s' does not exist") << argv[1];
        return false;
    }

    if(bc->flags & BCF_PROTECTED)
    {
        LOG_INPUT_ERROR("Binding context '%s' is protected and cannot be manually %s")
                << bc->name << (doActivate? "activated" : "deactivated");
        return false;
    }

    B_ActivateContext(bc, doActivate);
    return true;
}

/**
 * Checks to see if we need to respond to the given input event in some way
 * and then if so executes the action associated to the event.
 *
 * @param ev            ddevent_t we may need to respond to.
 *
 * @return              @c true, If an action was executed.
 */
dd_bool B_Responder(ddevent_t* ev)
{
    if(symbolicEchoMode && ev->type != E_SYMBOLIC)
    {
        // Make an echo.
        ddstring_t name;
        ddevent_t echo;

        // Axis events need a bit of filtering.
        if(ev->type == E_AXIS)
        {
            float pos = I_TransformAxis(I_GetDevice(ev->device), ev->axis.id, ev->axis.pos);
            if((ev->axis.type == EAXIS_ABSOLUTE && fabs(pos) < .5f) ||
               (ev->axis.type == EAXIS_RELATIVE && fabs(pos) < .02f))
            {
                // Not significant enough for an echo.
                return ClientApp::widgetActions().tryEvent(ev);
            }
        }

        Str_Init(&name);
        Str_Set(&name, "echo-");
        B_AppendEventToString(ev, &name);
        echo.device = ev->device;
        echo.type = E_SYMBOLIC;
        echo.symbolic.id = 0;
        echo.symbolic.name = Str_Text(&name);
        LOG_INPUT_XVERBOSE("Symbolic echo: %s") << echo.symbolic.name;
        DD_PostEvent(&echo);
        Str_Free(&name);
        return true;
    }

    return ClientApp::widgetActions().tryEvent(ev);
}


const char* B_ShortNameForKey2(int ddKey, dd_bool forceLowercase)
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
DENG_EXTERN_C int DD_GetKeyCode(const char* key)
{
    int code = B_KeyForShortName(key);
    return (code ? code : key[0]);
}

bool B_UnbindCommand(const char *command)
{
    bool deleted = false;
    for(int i = 0; i < B_ContextCount(); ++i)
    {
        bcontext_t *bc = B_ContextByPos(i);
        while(evbinding_t *ev = B_FindCommandBinding(&bc->commandBinds, command, NUM_INPUT_DEVICES))
        {
            deleted |= B_DeleteBinding(bc, ev->bid);
        }
    }
    return deleted;
}
