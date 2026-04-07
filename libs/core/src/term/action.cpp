/** @file libshell/src/action.cpp  Maps a key event to a signal.
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

#include "de/term/action.h"

namespace de { namespace term {

Action::Action(const String &label) : _event(KeyEvent("")), _label(label)
{}

Action::Action(const String &label, const Func &func)
    : _event(KeyEvent("")), _label(label)
{
    audienceForTriggered() += func;
}

Action::Action(const String &label, const KeyEvent &event, const Func &func)
    : _event(event), _label(label)
{
    audienceForTriggered() += func;
}

Action::Action(const KeyEvent &event, const Action::Func &func)
    : _event(event)
{
    audienceForTriggered() += func;
}

Action::~Action()
{}

void Action::setLabel(const String &label)
{
    _label = label;
}

String Action::label() const
{
    return _label;
}

bool Action::tryTrigger(const KeyEvent &ev)
{
    if (ev == _event)
    {
        trigger();
        return true;
    }
    return false;
}

}} // namespace de::term
