/** @file widgets/action.cpp  Abstract base class for UI actions.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/action.h"

namespace de {

DE_PIMPL_NOREF(Action)
{
    DE_PIMPL_AUDIENCE(Triggered)
};

DE_AUDIENCE_METHOD(Action, Triggered)

Action::Action() : d(new Impl)
{}

Action::~Action()
{}

void Action::trigger()
{
    DE_NOTIFY(Triggered, i)
    {
        i->actionTriggered(*this);
    }
}

} // namespace de
