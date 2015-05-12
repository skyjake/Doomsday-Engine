/** @file inputdevicehatcontrol.cpp  Hat control for a logical input device.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "ui/inputdevicehatcontrol.h"
#include <de/timer.h> // Timer_RealMilliseconds()

using namespace de;

InputDeviceHatControl::InputDeviceHatControl(String const &name)
{
    setName(name);
}

InputDeviceHatControl::~InputDeviceHatControl()
{}

dint InputDeviceHatControl::position() const
{
    return _pos;
}

void InputDeviceHatControl::setPosition(dint newPosition)
{
    _pos  = newPosition;
    _time = Timer_RealMilliseconds(); // Remember when the change occured.

    // We can clear the expiration when centered.
    if(_pos < 0)
    {
        setBindContextAssociation(Expired, UnsetFlags);
    }
}

duint InputDeviceHatControl::time() const
{
    return _time;
}

String InputDeviceHatControl::description() const
{
    return String(_E(b) "%1 " _E(.) "(Hat)").arg(fullName());
}

bool InputDeviceHatControl::inDefaultState() const
{
    return _pos < 0; // Centered?
}
