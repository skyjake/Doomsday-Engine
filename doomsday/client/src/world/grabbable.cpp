/** @file grabbable.cpp Grabbable.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "world/grabbable.h"

using namespace de;

Grabbable::Grabbable(): _grabs(0), _locked(true)
{}

Grabbable::~Grabbable()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->grabbableBeingDeleted(*this);
}

void Grabbable::grab()
{
    addGrab(); // Succeeded.
}

void Grabbable::ungrab()
{
    decGrab(); // Succeeded.
}

bool Grabbable::isGrabbed() const
{
    return _grabs != 0;
}

bool Grabbable::isLocked() const
{
    return _locked;
}

void Grabbable::move(Vector3d const &newOrigin)
{
    if(!_locked)
    {
        setOrigin(newOrigin);
        return;
    }
    //qDebug() << "Grabbable" << de::dintptr(this) << "is locked, move denied.";
}

void Grabbable::addGrab()
{
    ++_grabs;
    DENG2_ASSERT(_grabs >= 0);
}

void Grabbable::decGrab()
{
    --_grabs;
    DENG2_ASSERT(_grabs >= 0);
}

void Grabbable::setLock(bool enable)
{
    if(_locked != enable)
    {
        _locked = enable;
        DENG2_FOR_AUDIENCE(LockChange, i) i->grabbableLockChanged(*this);
    }
}
