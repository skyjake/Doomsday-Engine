/** @file buttoninputcontrol.cpp  Button control for a logical input device.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/buttoninputcontrol.h"
#include <de/legacy/timer.h> // Timer_RealMilliseconds()

using namespace de;

ButtonInputControl::ButtonInputControl(const String &name)
{
    setName(name);
}

ButtonInputControl::~ButtonInputControl()
{}

bool ButtonInputControl::isDown() const
{
    return _isDown;
}

void ButtonInputControl::setDown(bool yes)
{
    const bool oldDown = _isDown;

    _isDown = yes;

    if (_isDown != oldDown)
    {
        // Remember when the change occurred.
        _time = Timer_RealMilliseconds();
    }

    if (_isDown)
    {
        // This will get cleared after the state is checked by someone.
        setBindContextAssociation(Triggered);
    }
    else
    {
        // We can clear the expiration when the key is released.
        setBindContextAssociation(Expired, UnsetFlags);
    }

}

String ButtonInputControl::description() const
{
    return Stringf(_E(b) "%s " _E(.) "(Button)", fullName().c_str());
}

bool ButtonInputControl::inDefaultState() const
{
    return !_isDown; // Not depressed?
}

void ButtonInputControl::reset()
{
    setBindContextAssociation(Triggered | Expired, UnsetFlags);
    _isDown = false;
    _time   = 0;
}

duint ButtonInputControl::time() const
{
    return _time;
}
