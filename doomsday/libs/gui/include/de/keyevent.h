/** @file input/keyevent.h  Input event from a keyboard.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_KEYEVENT_H
#define LIBGUI_KEYEVENT_H

#include "libgui.h"
#include "ddkey.h"
#include <de/event.h>
#include <de/string.h>

namespace de {

/**
 * Input event generated by a keyboard device. @ingroup gui
 */
class LIBGUI_PUBLIC KeyEvent : public Event
{
public:
    enum State {
        Released, ///< Released key.
        Pressed,  ///< Pressed key.
        Repeat,   ///< Repeat while held pressed.
    };

    enum Modifier {
        NoModifiers = 0,
        Shift       = 1,
        Control     = 2,
        Alt         = 4,
        Meta        = 8, // Windows key; or Command key on Mac
#if defined (MACOSX)
        Command     = Meta,
#else
        Command     = Control,
#endif
    };
    using Modifiers = Flags;

public:
    KeyEvent();

    KeyEvent(State         keyState,
             int           sdlKey,
             int           scancode,
             Modifiers     mods = NoModifiers);

    KeyEvent(const String &insertText);

    State                state()      const;
    bool                 isModifier() const;
    inline int           sdlKey()     const { return _sdlKey; }
    inline int           scancode()   const { return _scancode; }
    inline int           ddKey()      const { return _ddKey; }
    inline const String &text()       const { return _text; }
    inline Modifiers     modifiers()  const { return _mods; }

    bool operator<(const KeyEvent &) const;

    /**
     * Translates an SDL key code to a Doomsday key code (see ddkey.h).
     *
     * @param sdlKey     SDL key code.
     * @param scancode   SDL scan code.
     *
     * @return DDKEY code.
     */
    static int ddKeyFromSDL(int sdlKey, int scancode);

    static Modifiers modifiersFromSDL(int mods);

    /**
     * Constructs a key press event for UI actions. Use with de::KeyActions.
     */
    static KeyEvent press(int sdlKey, Modifiers mods = NoModifiers);

private:
    int       _sdlKey;
    int       _scancode;
    int       _ddKey;
    String    _text;
    Modifiers _mods;
};

} // namespace de

#endif // LIBGUI_KEYEVENT_H

