/** @file system.cpp  Base class for application subsystems.
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

#include "de/System"

namespace de {

DENG2_PIMPL(System)
{
    Flags behavior;

    Instance(Public *i) : Base(*i)
    {}
};

System::System(Flags const &behavior) : d(new Instance(this))
{
    d->behavior = behavior;
}

void System::setBehavior(Flags const &behavior)
{
    d->behavior = behavior;
}

System::Flags System::behavior() const
{
    return d->behavior;
}

bool System::processEvent(Event const &)
{
    return false;
}

void System::timeChanged(Clock const &)
{}

} // namespace de
