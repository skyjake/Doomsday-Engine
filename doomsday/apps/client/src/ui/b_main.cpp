/** @file b_main.cpp  Event and device state bindings system.
 *
 * @todo Pretty much everything in this file belongs in InputSystem.
 *
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#include "ui/b_main.h"
#include "ui/inputsystem.h"

#include "dd_main.h" // App_GameLoaded
#include "dd_def.h"
#include "clientapp.h"
#include "ui/bindcontext.h"

using namespace de;

/**
 * Binding context fallback for the "global" context.
 *
 * @param ddev  Event being processed.
 *
 * @return @c true if the event was eaten and can be processed by the rest of the
 * binding context stack.
 */
static int globalContextFallback(const ddevent_t *ddev)
{
    if (App_GameLoaded() && !BusyMode_Active())
    {
        event_t ev;
        if (InputSystem::convertEvent(*ddev, ev))
        {
            // The game's normal responder only returns true if the bindings can't
            // be used (like when chatting). Note that if the event is eaten here,
            // the rest of the bindings contexts won't get a chance to process the
            // event.
            if (gx.Responder && gx.Responder(&ev))
            {
                return true;
            }
        }
    }

    return false;
}

/// @note Called once on init.
void B_Init()
{
    InputSystem &isys = ClientApp::input();

    // In dedicated mode we have fewer binding contexts available.

    // The contexts are defined in reverse order, with the context of lowest
    // priority defined first.

    isys.newContext(DEFAULT_BINDING_CONTEXT_NAME);

    // Game contexts.
    /// @todo Game binding context setup obviously belong to the game plugin, so shouldn't be here.
    isys.newContext("map");
    isys.newContext("map-freepan");
    isys.newContext("finale"); // uses a fallback responder to handle script events
    isys.newContext("menu")->acquireAll();
    isys.newContext("gameui");
    isys.newContext("shortcut");
    isys.newContext("chat")->acquire(IDEV_KEYBOARD);
    isys.newContext("message")->acquireAll();

    // Binding context for the console.
    BindContext *bc = isys.newContext(CONSOLE_BINDING_CONTEXT_NAME);
    bc->protect();              // Only we can (de)activate.
    bc->acquire(IDEV_KEYBOARD); // Console takes over all keyboard events.

    // UI doesn't let anything past it.
    isys.newContext(UI_BINDING_CONTEXT_NAME)->acquireAll();

    // Top-level context that is always active and overrides every other context.
    // To be used only for system-level functionality.
    bc = isys.newContext(GLOBAL_BINDING_CONTEXT_NAME);
    bc->protect();
    bc->setDDFallbackResponder(globalContextFallback);
    bc->activate();

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
    isys.bindDefaults();

    isys.initialContextActivations();
}
