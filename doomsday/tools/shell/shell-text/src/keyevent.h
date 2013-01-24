/** @file keyevent.h Key event.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef KEYEVENT_H
#define KEYEVENT_H

#include <de/Event>
#include <de/String>
#include <QFlags>

/**
 * Key press event generated when the user presses a key on the keyboard.
 */
class KeyEvent : public de::Event
{
public:
    enum Modifier
    {
        None = 0x0,
        Control = 0x1
    };
    Q_DECLARE_FLAGS(Modifiers, Modifier)

public:
    KeyEvent(de::String const &keyText) : Event(KeyPress), _text(keyText), _code(0) {}
    KeyEvent(int keyCode, Modifiers mods = None) : Event(KeyPress), _code(keyCode), _modifiers(mods) {}

    de::String text() const { return _text; }
    int key() const { return _code; }
    Modifiers modifiers() const { return _modifiers; }

private:
    de::String _text;
    int _code;
    Modifiers _modifiers;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KeyEvent::Modifiers)

#endif // KEYEVENT_H
