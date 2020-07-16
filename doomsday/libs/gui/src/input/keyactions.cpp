/** @file keyactions.cpp  Callbacks to be called on key events.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/keyactions.h"
#include <de/keymap.h>

namespace de {

DE_PIMPL_NOREF(KeyActions)
{
    KeyMap<KeyEvent, std::function<void()>> actions;
};

KeyActions::KeyActions()
    : d(new Impl)
{}

void KeyActions::add(const KeyEvent &key, const std::function<void()> &callback)
{
    d->actions.insert(key, callback);
}

bool KeyActions::handleEvent(const Event &ev)
{
    if (ev.type() == Event::KeyPress)
    {
        const auto &key = ev.as<KeyEvent>();
        if (!key.isModifier()) // Don't do anything when modifier is pressed on its own.
        {
            const auto &m = *d.getConst();
            auto found = m.actions.find(key);
            if (found != m.actions.end())
            {
                found->second();
                return true;
            }
        }
    }
    return false;
}

} // namespace de
