/** @file inputsystem.cpp  Input subsystem.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

//#include "de_platform.h"
#include "ui/inputsystem.h"

#include <de/Record>
#include <de/NumberValue>
#include "clientapp.h"
#include "ui/dd_input.h"
#include "ui/b_main.h"
#include "ui/sys_input.h"

using namespace de;

static Value *Function_InputSystem_BindEvent(Context &, Function::ArgumentValues const &args)
{
    String eventDesc = args[0]->asText();
    String command   = args[1]->asText();

    if(B_BindCommand(eventDesc.toLatin1(), command.toLatin1()))
    {
        // Success.
        return new NumberValue(true);
    }

    // Failed to create the binding...
    return new NumberValue(false);
}

DENG2_PIMPL(InputSystem)
{
    Binder binder;
    SettingsRegister settings;
    Record *scriptBindings;

    Instance(Public *i) : Base(i)
    {
        // Initialize settings.
        settings.define(SettingsRegister::ConfigVariable, "input.mouse.syncSensitivity")
                .define(SettingsRegister::FloatCVar,      "input-mouse-x-scale", 1.f/1000.f)
                .define(SettingsRegister::FloatCVar,      "input-mouse-y-scale", 1.f/1000.f)
                .define(SettingsRegister::IntCVar,        "input-mouse-x-flags", 0)
                .define(SettingsRegister::IntCVar,        "input-mouse-y-flags", 0)
                .define(SettingsRegister::IntCVar,        "input-joy", 1)
                .define(SettingsRegister::IntCVar,        "input-sharp", 1);

        // Initialize script bindings.
        binder.initNew()
                << DENG2_FUNC(InputSystem_BindEvent, "bindEvent", "event" << "command");

        App::scriptSystem().addNativeModule("Input", binder.module());

        // Initialize the system.
        DD_InitInput();
        I_Init();
        I_InitVirtualInputDevices();
    }

    ~Instance()
    {
        // Shutdown.
        I_ShutdownInputDevices();
        I_Shutdown();
    }
};

InputSystem::InputSystem() : d(new Instance(this))
{}

void InputSystem::timeChanged(Clock const &)
{}

SettingsRegister &InputSystem::settings()
{
    return d->settings;
}

void InputSystem::consoleRegister() // static
{
    B_ConsoleRegister(); // for control bindings
    I_ConsoleRegister();
}
