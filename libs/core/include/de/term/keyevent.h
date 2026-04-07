#include <utility>

/** @file shell/keyevent.h Key event.
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

#ifndef LIBSHELL_KEYEVENT_H
#define LIBSHELL_KEYEVENT_H

#include "de/event.h"
#include "de/string.h"

namespace de { namespace term {

enum class Key {
    None,
    Escape,
    Break,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    PageUp,
    PageDown,
    Insert,
    Delete,
    Enter,
    Backspace,
    Kill,
    Tab,
    Backtab,
    Cancel,
    Substitute,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
};

/**
 * Key press event generated when the user presses a key on the keyboard.
 *
 * @ingroup textUi
 */
class DE_PUBLIC KeyEvent : public Event
{
public:
    enum Modifier { None = 0x0, Control = 0x1 };
    using Modifiers = Flags;

public:
    KeyEvent(const String &keyText, Modifiers mods = None)
        : Event(KeyPress)
        , _text(keyText)
        , _code(Key::None)
        , _modifiers(std::move(mods))
    {}
    KeyEvent(Key keyCode, Modifiers mods = None)
        : Event(KeyPress)
        , _code(keyCode)
        , _modifiers(std::move(mods))
    {}

    String    text() const { return _text; }
    Key       key() const { return _code; }
    Modifiers modifiers() const { return _modifiers; }

    bool operator==(const KeyEvent &other) const
    {
        return _text == other._text && _code == other._code && _modifiers == other._modifiers;
    }

private:
    String    _text;      ///< Text to be inserted by the event.
    Key       _code;      ///< Key code.
    Modifiers _modifiers; ///< Modifiers in effect.
};

}} // namespace de::term

#endif // LIBSHELL_KEYEVENT_H
