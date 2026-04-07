/** @file hatinputcontrol.cpp  Hat control for a logical input device.
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

#include "ui/hatinputcontrol.h"
#include <de/legacy/timer.h> // Timer_RealMilliseconds()

using namespace de;

HatInputControl::HatInputControl(const String &name)
{
    setName(name);
}

HatInputControl::~HatInputControl()
{}

dint HatInputControl::position() const
{
    return _pos;
}

void HatInputControl::setPosition(dint newPosition)
{
    _pos  = newPosition;
    _time = Timer_RealMilliseconds(); // Remember when the change occured.

    // We can clear the expiration when centered.
    if (_pos < 0)
    {
        setBindContextAssociation(Expired, UnsetFlags);
    }
}

duint HatInputControl::time() const
{
    return _time;
}

String HatInputControl::description() const
{
    return Stringf(
        _E(b) "%s " _E(.) "(Hat) " _E(l) "Position: " _E(.) "%i", fullName().c_str(), _pos);
}

bool HatInputControl::inDefaultState() const
{
    return _pos < 0; // Centered?
}
