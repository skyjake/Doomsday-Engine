/** @file inputsystem.cpp  Input subsystem.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/inputsystem.h"
#include "ui/dd_input.h"
#include "ui/sys_input.h"
#include "con_main.h"

using namespace de;

DENG2_PIMPL(InputSystem)
{
    Instance(Public *i) : Base(i)
    {
        // Initialize the system.
        DD_InitInput();

        if(!I_Init())
        {
            Con_Error("Failed to initialize Input subsystem.\n");
        }

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
{
}
