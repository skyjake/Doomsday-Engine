/** @file keyevent.cpp  Input event from a keyboard.
 *
 * Depends on Qt GUI.
 *
 * @authors Copyright (c) 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/keyevent.h"
#include <de/log.h>

#include <SDL_keycode.h>

namespace de {

int KeyEvent::ddKeyFromSDL(int sdlKey, int scancode)
{
    if (sdlKey >= ' ' && sdlKey <= '~')
    {
        // Basic ASCII.
        return sdlKey;
    }

    if (scancode == SDL_SCANCODE_NONUSBACKSLASH)
    {
        return DDKEY_ISOEXTRAKEY;
    }

    // Non-character-inserting keys.
    switch (sdlKey)
    {
    case SDLK_ESCAPE:        return DDKEY_ESCAPE;
    case SDLK_TAB:           return DDKEY_TAB;
    case SDLK_BACKSPACE:     return DDKEY_BACKSPACE;
    case SDLK_PAUSE:         return DDKEY_PAUSE;
    case SDLK_UP:            return DDKEY_UPARROW;
    case SDLK_DOWN:          return DDKEY_DOWNARROW;
    case SDLK_LEFT:          return DDKEY_LEFTARROW;
    case SDLK_RIGHT:         return DDKEY_RIGHTARROW;
    case SDLK_RCTRL:         return DDKEY_RCTRL;
    case SDLK_LCTRL:         return DDKEY_LCTRL;
    case SDLK_RSHIFT:        return DDKEY_RSHIFT;
    case SDLK_LSHIFT:        return DDKEY_LSHIFT;
    case SDLK_RALT:          return DDKEY_RALT;
    case SDLK_LALT:          return DDKEY_LALT;
    case SDLK_APPLICATION:   return DDKEY_WINMENU;
    case SDLK_RETURN:        return DDKEY_RETURN;
    case SDLK_F1:            return DDKEY_F1;
    case SDLK_F2:            return DDKEY_F2;
    case SDLK_F3:            return DDKEY_F3;
    case SDLK_F4:            return DDKEY_F4;
    case SDLK_F5:            return DDKEY_F5;
    case SDLK_F6:            return DDKEY_F6;
    case SDLK_F7:            return DDKEY_F7;
    case SDLK_F8:            return DDKEY_F8;
    case SDLK_F9:            return DDKEY_F9;
    case SDLK_F10:           return DDKEY_F10;
    case SDLK_F11:           return DDKEY_F11;
    case SDLK_F12:           return DDKEY_F12;
    case SDLK_F14:           return DDKEY_PAUSE;
    case SDLK_F15:           return DDKEY_PRINT;
    case SDLK_NUMLOCKCLEAR:  return DDKEY_NUMLOCK;
    case SDLK_SCROLLLOCK:    return DDKEY_SCROLL;
    case SDLK_KP_ENTER:      return DDKEY_ENTER;
    case SDLK_INSERT:        return DDKEY_INS;
    case SDLK_DELETE:        return DDKEY_DEL;
    case SDLK_HOME:          return DDKEY_HOME;
    case SDLK_END:           return DDKEY_END;
    case SDLK_PAGEUP:        return DDKEY_PGUP;
    case SDLK_PAGEDOWN:      return DDKEY_PGDN;
    case SDLK_SYSREQ:        return DDKEY_PRINT;
    case SDLK_PRINTSCREEN:   return DDKEY_PRINT;
    case SDLK_CAPSLOCK:      return DDKEY_CAPSLOCK;
    case SDLK_KP_0:          return DDKEY_NUMPAD0;
    case SDLK_KP_1:          return DDKEY_NUMPAD1;
    case SDLK_KP_2:          return DDKEY_NUMPAD2;
    case SDLK_KP_3:          return DDKEY_NUMPAD3;
    case SDLK_KP_4:          return DDKEY_NUMPAD4;
    case SDLK_KP_5:          return DDKEY_NUMPAD5;
    case SDLK_KP_6:          return DDKEY_NUMPAD6;
    case SDLK_KP_7:          return DDKEY_NUMPAD7;
    case SDLK_KP_8:          return DDKEY_NUMPAD8;
    case SDLK_KP_9:          return DDKEY_NUMPAD9;
    case SDLK_KP_PLUS:       return DDKEY_ADD;
    case SDLK_KP_MINUS:      return DDKEY_SUBTRACT;
    case SDLK_KP_MULTIPLY:   return DDKEY_MULTIPLY;
    case SDLK_KP_DIVIDE:     return DDKEY_DIVIDE;
    case SDLK_KP_PERIOD:     return DDKEY_DECIMAL;

    default:
        break;
    }

    // Not supported!
    LOGDEV_INPUT_WARNING("Ignored unknown key: SDL key %i (%x), scancode %i (%x)")
        << sdlKey << sdlKey << scancode << scancode;
    return 0;
}

KeyEvent::KeyEvent()
    : Event(KeyPress)
    , _sdlKey(0)
    , _scancode(0)
    , _ddKey(0)
{}

KeyEvent::KeyEvent(const String &insertText)
    : Event(KeyPress)
    , _sdlKey(0)
    , _scancode(0)
    , _ddKey(0)
    , _text(insertText)
{}

KeyEvent::KeyEvent(State         keyState,
                   int           sdlKey,
                   int           scancode,
                   Modifiers     modifiers)
    : Event(keyState == Pressed ? KeyPress : keyState == Repeat ? KeyRepeat : KeyRelease)
    , _sdlKey(sdlKey)
    , _scancode(scancode)
    , _ddKey(ddKeyFromSDL(sdlKey, scancode))
    , _mods(modifiers)
{
    DE_ASSERT(_sdlKey != 0);
}

KeyEvent::State KeyEvent::state() const
{
    switch (type())
    {
    case KeyPress:  return Pressed;
    case KeyRepeat: return Repeat;
    default:        return Released;
    }
}

bool KeyEvent::operator<(const KeyEvent &ev) const
{
    if (type() < ev.type())
    {
        return true;
    }
    if (_text || ev._text)
    {
        return _text.compare(ev._text);
    }
    if (_mods < ev._mods)
    {
        return true;
    }
    if (_scancode && ev._scancode)
    {
        return _scancode < ev._scancode;
    }
    return _sdlKey < ev._sdlKey;
}

bool KeyEvent::isModifier() const
{
    switch (_sdlKey)
    {
        case SDLK_RSHIFT:
        case SDLK_RALT:
        case SDLK_RCTRL:
        case SDLK_RGUI:
        case SDLK_LSHIFT:
        case SDLK_LALT:
        case SDLK_LCTRL:
        case SDLK_LGUI:
            return true;

        default:
            return false;
    }
}

KeyEvent::Modifiers KeyEvent::modifiersFromSDL(int mods) // static
{
    Modifiers m;
    if (mods & KMOD_SHIFT) m |= Shift;
    if (mods & KMOD_ALT)   m |= Alt;
    if (mods & KMOD_CTRL)  m |= Control;
    if (mods & KMOD_GUI)   m |= Meta;
    return m;
}

KeyEvent KeyEvent::press(int sdlKey, KeyEvent::Modifiers mods)
{
    return {Pressed, sdlKey, 0, mods};
}

} // namespace de
