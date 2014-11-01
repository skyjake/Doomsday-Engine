/** @file inputdevicebuttoncontrol.cpp  Button control for a logical input device.
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

#include "ui/inputdevicebuttoncontrol.h"
#include <de/timer.h> // SECONDSPERTIC

using namespace de;

InputDeviceButtonControl::InputDeviceButtonControl(String const &name)
{
    setName(name);
}

InputDeviceButtonControl::~InputDeviceButtonControl()
{}

bool InputDeviceButtonControl::isDown() const
{
    return _isDown;
}

void InputDeviceButtonControl::setDown(bool yes)
{
    bool const oldDown = _isDown;

    _isDown = yes;

    if(_isDown != oldDown)
    {
        // Remember when the change occurred.
        _time = Timer_RealMilliseconds();
    }

    if(_isDown)
    {
        // This will get cleared after the state is checked by someone.
        setBindContextAssociation(Triggered);
    }
    else
    {
        // We can clear the expiration when the key is released.
        setBindContextAssociation(Triggered, UnsetFlags);
    }

}

String InputDeviceButtonControl::description() const
{
    return String(_E(b) "%1 " _E(.) "(Button)").arg(fullName());
}

bool InputDeviceButtonControl::inDefaultState() const
{
    return !_isDown; // Not depressed?
}

void InputDeviceButtonControl::reset()
{
    if(_isDown)
    {
        setBindContextAssociation(Expired);
    }
    else
    {
        _isDown = false;
        _time   = 0;
        setBindContextAssociation(Triggered | Expired, UnsetFlags);
    }
}

duint InputDeviceButtonControl::time() const
{
    return _time;
}
