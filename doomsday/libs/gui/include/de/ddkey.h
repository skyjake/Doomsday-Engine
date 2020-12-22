/** @file ddkey.h  DDKEY codes.
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBGUI_DDKEY_H
#define LIBGUI_DDKEY_H

/** @defgroup keyConstants Key Constants
 * @ingroup gui */

/**
 * DDKEY codes. Most key data is regular ASCII so key constants correspond to
 * ASCII codes. Note that these are used when dealing with the physical state
 * of individual keys; text is entered separately as Unicode.
 *
 * @ingroup keyConstants
 */
enum {
    DDKEY_TAB = 9,
    DDKEY_RETURN = 13,
    DDKEY_ESCAPE = 27,
    DDKEY_MINUS = 0x2d,
    DDKEY_EQUALS = 0x3d,
    DDKEY_BACKSLASH = 0x5c,
    DDKEY_BACKSPACE = 127,
    DDKEY_RIGHTARROW = 0x80,
    DDKEY_LEFTARROW,
    DDKEY_UPARROW,
    DDKEY_DOWNARROW,
    DDKEY_F1,
    DDKEY_F2,
    DDKEY_F3,
    DDKEY_F4,
    DDKEY_F5,
    DDKEY_F6,
    DDKEY_F7,
    DDKEY_F8,
    DDKEY_F9,
    DDKEY_F10,
    DDKEY_F11,
    DDKEY_F12,
    DDKEY_NUMLOCK,
    DDKEY_CAPSLOCK,
    DDKEY_SCROLL,
    DDKEY_NUMPAD7,
    DDKEY_NUMPAD8,
    DDKEY_NUMPAD9,
    DDKEY_NUMPAD4,
    DDKEY_NUMPAD5,
    DDKEY_NUMPAD6,
    DDKEY_NUMPAD1,
    DDKEY_NUMPAD2,
    DDKEY_NUMPAD3,
    DDKEY_NUMPAD0,
    DDKEY_DECIMAL,
    DDKEY_NUMPADEQUALS,
    DDKEY_PAUSE,
    DDKEY_RSHIFT,
    DDKEY_LSHIFT = DDKEY_RSHIFT,
    DDKEY_RCTRL,
    DDKEY_LCTRL = DDKEY_RCTRL,
    DDKEY_RALT,
    DDKEY_LALT = DDKEY_RALT,
    DDKEY_INS,
    DDKEY_DEL,
    DDKEY_PGUP,
    DDKEY_PGDN,
    DDKEY_HOME,
    DDKEY_END,
    DDKEY_SUBTRACT,     ///< '-' on numeric keypad.
    DDKEY_ADD,          ///< '+' on numeric keypad.
    DDKEY_PRINT,
    DDKEY_ENTER,        ///< on the numeric keypad.
    DDKEY_DIVIDE,       ///< '/' on numeric keypad.
    DDKEY_MULTIPLY,     ///< '*' on the numeric keypad.
    DDKEY_SECTION,      ///< §
    DDKEY_WINMENU,      ///< Windows-specific context menu key.
    DDKEY_ISOEXTRAKEY,  ///< Non-US: backslash, or tilde/accent, or </> (depending on country).
    DD_HIGHEST_KEYCODE
};

#endif // LIBGUI_DDKEY_H
