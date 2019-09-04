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

//#ifdef WIN32
//#  define WIN32_LEAN_AND_MEAN
//#  include <windows.h>
//#endif

#include "de/KeyEvent"
#include <de/Log>

#include <SDL_keycode.h>

namespace de {

//#if defined (DE_X11)
//#  include <QX11Info>
//#  include <X11/keysym.h>
//#  include <X11/Xlib.h>
//#  include "../src/input/imKStoUCS_x11.c"
//#  define XFREE_KEYMAPPING
//static int x11ScancodeToDDKey(int scancode);
//#  ifdef KeyPress
//#    undef KeyPress
//#  endif
//#  ifdef KeyRelease
//#    undef KeyRelease
//#  endif
//#  ifdef Always
//#    undef Always
//#  endif
//#  ifdef None
//#    undef None
//#  endif
//#endif

#if 0
#if defined (WIN32)
static int win32Keymap[256];

/**
 * Initialize the Windows virtual key => DDKEY translation map.
 */
static void checkWin32Keymap()
{
    if (win32Keymap[VK_BACK] == DDKEY_BACKSPACE)
    {
        // Already go it.
        return;
    }

    int* keymap = win32Keymap;

    keymap[VK_BACK] = DDKEY_BACKSPACE; // Backspace
    keymap[VK_TAB ] = DDKEY_TAB;
    //keymap[VK_CLEAR] = ;
    keymap[VK_RETURN] = DDKEY_RETURN;
    keymap[VK_SHIFT] = DDKEY_RSHIFT;
    keymap[VK_CONTROL] = DDKEY_RCTRL;
    keymap[VK_MENU] = DDKEY_RALT;
    keymap[VK_PAUSE] = DDKEY_PAUSE;
    keymap[VK_CAPITAL] = DDKEY_CAPSLOCK;
    //keymap[VK_KANA] = ;
    //keymap[VK_HANGEUL] = ;
    //keymap[VK_HANGUL] = ;
    //keymap[VK_JUNJA] = ;
    //keymap[VK_FINAL] = ;
    //keymap[VK_HANJA] = ;
    //keymap[VK_KANJI] = ;
    keymap[VK_ESCAPE] = DDKEY_ESCAPE;
    //keymap[VK_CONVERT] = ;
    //keymap[VK_NONCONVERT] = ;
    //keymap[VK_ACCEPT] = ;
    //keymap[VK_MODECHANGE] = ;
    keymap[VK_SPACE] = ' ';
    keymap[VK_OEM_PLUS] = '='; //+';
    keymap[VK_OEM_COMMA] = ',';
    keymap[VK_OEM_MINUS] = '-';
    keymap[VK_OEM_PERIOD] = '.';
    keymap[VK_OEM_1] = ';';
    keymap[VK_OEM_2] = '/';
    keymap[VK_OEM_3] = '\'';
    keymap[VK_OEM_4] = '[';
    keymap[VK_OEM_5] = DDKEY_BACKSLASH;
    keymap[VK_OEM_6] = ']';
    keymap[VK_OEM_7] = '#';
    keymap[VK_OEM_8] = '`';
    keymap[VK_OEM_102] = '`';
    keymap[VK_PRIOR] = DDKEY_PGUP;
    keymap[VK_NEXT] = DDKEY_PGDN;
    keymap[VK_END] = DDKEY_END;
    keymap[VK_HOME] = DDKEY_HOME;
    keymap[VK_LEFT] = DDKEY_LEFTARROW;
    keymap[VK_UP] = DDKEY_UPARROW;
    keymap[VK_RIGHT] = DDKEY_RIGHTARROW;
    keymap[VK_DOWN] = DDKEY_DOWNARROW;
    //keymap[VK_SELECT] = ;
    //keymap[VK_PRINT] = ;
    //keymap[VK_EXECUTE] = ;
    //keymap[VK_SNAPSHOT] = ;
    keymap[VK_INSERT] = DDKEY_INS;
    keymap[VK_DELETE] = DDKEY_DEL;
    //keymap[VK_HELP] = ;
    //keymap[VK_LWIN] = ;
    //keymap[VK_RWIN] = ;
    //keymap[VK_APPS] = ;
    //keymap[VK_SLEEP] = ;
    keymap[VK_NUMPAD0] = DDKEY_NUMPAD0;
    keymap[VK_NUMPAD1] = DDKEY_NUMPAD1;
    keymap[VK_NUMPAD2] = DDKEY_NUMPAD2;
    keymap[VK_NUMPAD3] = DDKEY_NUMPAD3;
    keymap[VK_NUMPAD4] = DDKEY_NUMPAD4;
    keymap[VK_NUMPAD5] = DDKEY_NUMPAD5;
    keymap[VK_NUMPAD6] = DDKEY_NUMPAD6;
    keymap[VK_NUMPAD7] = DDKEY_NUMPAD7;
    keymap[VK_NUMPAD8] = DDKEY_NUMPAD8;
    keymap[VK_NUMPAD9] = DDKEY_NUMPAD9;
    keymap[VK_MULTIPLY] = DDKEY_MULTIPLY;
    keymap[VK_ADD] = DDKEY_ADD;
    //keymap[VK_SEPARATOR] = ;
    keymap[VK_SUBTRACT] = DDKEY_SUBTRACT;
    keymap[VK_DECIMAL] = DDKEY_DECIMAL;
    keymap[VK_DIVIDE] = DDKEY_DIVIDE;
    keymap[VK_F1] = DDKEY_F1;
    keymap[VK_F2] = DDKEY_F2;
    keymap[VK_F3] = DDKEY_F3;
    keymap[VK_F4] = DDKEY_F4;
    keymap[VK_F5] = DDKEY_F5;
    keymap[VK_F6] = DDKEY_F6;
    keymap[VK_F7] = DDKEY_F7;
    keymap[VK_F8] = DDKEY_F8;
    keymap[VK_F9] = DDKEY_F9;
    keymap[VK_F10] = DDKEY_F10;
    keymap[VK_F11] = DDKEY_F11;
    keymap[VK_F12] = DDKEY_F12;
    keymap[VK_SNAPSHOT] = DDKEY_PRINT;

    keymap[0x30] = '0';
    keymap[0x31] = '1';
    keymap[0x32] = '2';
    keymap[0x33] = '3';
    keymap[0x34] = '4';
    keymap[0x35] = '5';
    keymap[0x36] = '6';
    keymap[0x37] = '7';
    keymap[0x38] = '8';
    keymap[0x39] = '9';
    keymap[0x41] = 'a';
    keymap[0x42] = 'b';
    keymap[0x43] = 'c';
    keymap[0x44] = 'd';
    keymap[0x45] = 'e';
    keymap[0x46] = 'f';
    keymap[0x47] = 'g';
    keymap[0x48] = 'h';
    keymap[0x49] = 'i';
    keymap[0x4A] = 'j';
    keymap[0x4B] = 'k';
    keymap[0x4C] = 'l';
    keymap[0x4D] = 'm';
    keymap[0x4E] = 'n';
    keymap[0x4F] = 'o';
    keymap[0x50] = 'p';
    keymap[0x51] = 'q';
    keymap[0x52] = 'r';
    keymap[0x53] = 's';
    keymap[0x54] = 't';
    keymap[0x55] = 'u';
    keymap[0x56] = 'v';
    keymap[0x57] = 'w';
    keymap[0x58] = 'x';
    keymap[0x59] = 'y';
    keymap[0x5A] = 'z';
}
#endif // WIN32
#endif // 0

int KeyEvent::ddKeyFromSDL(int sdlKey, int scancode)
{
//#ifdef MACOSX
//    switch (qtKey)
//    {
//    case Qt::Key_Meta:          return DDKEY_RCTRL;
//    case Qt::Key_Control:       return 0; // Don't map the Command key.
//    case Qt::Key_F14:           return DDKEY_PAUSE; // No pause key on the Mac.
//    case Qt::Key_F15:           return DDKEY_PRINT;

//    default:
//        break;
//    }
//#endif

//#ifdef XFREE_KEYMAPPING
//    // We'll check before the generic Qt keys to detect the numpad.
//    int mapped = x11ScancodeToDDKey(nativeScanCode);
//    if (mapped) return mapped;
//#else
//    DE_UNUSED(nativeScanCode);
//#endif

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

//#ifdef WIN32
//    case Qt::Key_Meta:          return 0; // Ignore Windows key.
//#endif
    default:
        break;
    }

#if 0
    /// We'll have to use the native virtual keys to make a distinction, e.g.,
    /// between the number row and the keypad. These are the real physical keys
    /// -- the insertion text is provided outside this mapping.

#ifdef WIN32
    /// @todo Would the native scancodes be more appropriate than virtual keys?
    /// (no influence from language settings)
    checkWin32Keymap();
    if (win32Keymap[nativeVirtualKey] > 0)
    {
        // We know a mapping for this.
        return win32Keymap[nativeVirtualKey];
    }
#endif

#ifdef MACOSX
    switch (nativeVirtualKey)
    {
    case 0x00:                  return 'a';
    case 0x01:                  return 's';
    case 0x02:                  return 'd';
    case 0x03:                  return 'f';
    case 0x04:                  return 'h';
    case 0x05:                  return 'g';
    case 0x06:                  return 'z';
    case 0x07:                  return 'x';
    case 0x08:                  return 'c';
    case 0x09:                  return 'v';
    case 0x0A:                  return DDKEY_SECTION;
    case 0x0B:                  return 'b';
    case 0x0C:                  return 'q';
    case 0x0D:                  return 'w';
    case 0x0E:                  return 'e';
    case 0x0F:                  return 'r';
    case 0x10:                  return 'y';
    case 0x11:                  return 't';
    case 0x12:                  return '1';
    case 0x13:                  return '2';
    case 0x14:                  return '3';
    case 0x15:                  return '4';
    case 0x16:                  return '6';
    case 0x17:                  return '5';
    case 0x18:                  return '=';
    case 0x19:                  return '9';
    case 0x1A:                  return '7';
    case 0x1B:                  return '-';
    case 0x1C:                  return '8';
    case 0x1D:                  return '0';
    case 0x1E:                  return ']';
    case 0x1F:                  return 'o';
    case 0x20:                  return 'u';
    case 0x21:                  return '[';
    case 0x22:                  return 'i';
    case 0x23:                  return 'p';
    case 0x25:                  return 'l';
    case 0x26:                  return 'j';
    case 0x27:                  return '\'';
    case 0x28:                  return 'k';
    case 0x29:                  return ';';
    case 0x2A:                  return '\\';
    case 0x2B:                  return ',';
    case 0x2C:                  return '/';
    case 0x2D:                  return 'n';
    case 0x2E:                  return 'm';
    case 0x2F:                  return '.';
    case 0x32:                  return '`';
    case 82:                    return DDKEY_NUMPAD0;
    case 83:                    return DDKEY_NUMPAD1;
    case 84:                    return DDKEY_NUMPAD2;
    case 85:                    return DDKEY_NUMPAD3;
    case 86:                    return DDKEY_NUMPAD4;
    case 87:                    return DDKEY_NUMPAD5;
    case 88:                    return DDKEY_NUMPAD6;
    case 89:                    return DDKEY_NUMPAD7;
    case 91:                    return DDKEY_NUMPAD8;
    case 92:                    return DDKEY_NUMPAD9;
    case 65:                    return DDKEY_DECIMAL;
    case 69:                    return DDKEY_ADD;
    case 78:                    return DDKEY_SUBTRACT;
    case 75:                    return DDKEY_DIVIDE;
    case 0x43:                  return DDKEY_MULTIPLY;

    default:
        break;
    }
#endif
#endif
    // Not supported!
    LOGDEV_INPUT_WARNING("Ignored unknown key: SDL key %i (%x), scancode %i (%x)")
        << sdlKey << sdlKey << scancode << scancode;
    return 0;
}

#ifdef XFREE_KEYMAPPING
static int x11ScancodeToDDKey(int scancode)
{
    int symCount;
    KeySym *syms = XGetKeyboardMapping(QX11Info::display(), scancode, 1, &symCount);
    if (!symCount)
    {
        XFree(syms);
        return 0;
    }
    KeySym sym = syms[0];
    XFree(syms);
    syms = nullptr;

    if (sym == NoSymbol) return 0;

    unsigned int ucs4 = X11_KeySymToUcs4(sym);
    if (ucs4)
    {
        // ASCII range.
        if (ucs4 > ' ' && ucs4 < 128) return ucs4;
        //qDebug() << "ucs4:" << ucs4 << hex << ucs4 << dec;
        return 0;
    }
    //qDebug() << "sym:" << hex << sym << dec;
    switch (sym)
    {
    case XK_KP_Insert:          return DDKEY_NUMPAD0;
    case XK_KP_End:             return DDKEY_NUMPAD1;
    case XK_KP_Down:            return DDKEY_NUMPAD2;
    case XK_KP_Page_Down:       return DDKEY_NUMPAD3;
    case XK_KP_Left:            return DDKEY_NUMPAD4;
    case XK_KP_Begin:           return DDKEY_NUMPAD5;
    case XK_KP_Right:           return DDKEY_NUMPAD6;
    case XK_KP_Home:            return DDKEY_NUMPAD7;
    case XK_KP_Up:              return DDKEY_NUMPAD8;
    case XK_KP_Page_Up:         return DDKEY_NUMPAD9;

    case XK_KP_0:               return DDKEY_NUMPAD0;
    case XK_KP_1:               return DDKEY_NUMPAD1;
    case XK_KP_2:               return DDKEY_NUMPAD2;
    case XK_KP_3:               return DDKEY_NUMPAD3;
    case XK_KP_4:               return DDKEY_NUMPAD4;
    case XK_KP_5:               return DDKEY_NUMPAD5;
    case XK_KP_6:               return DDKEY_NUMPAD6;
    case XK_KP_7:               return DDKEY_NUMPAD7;
    case XK_KP_8:               return DDKEY_NUMPAD8;
    case XK_KP_9:               return DDKEY_NUMPAD9;

    case XK_KP_Equal:           return '=';
    case XK_KP_Multiply:        return DDKEY_MULTIPLY;
    case XK_KP_Add:             return DDKEY_ADD;
    case XK_KP_Separator:       return DDKEY_DECIMAL;
    case XK_KP_Delete:          return DDKEY_DECIMAL;
    case XK_KP_Subtract:        return DDKEY_SUBTRACT;
    case XK_KP_Decimal:         return DDKEY_DECIMAL;
    case XK_KP_Divide:          return DDKEY_DIVIDE;

    default:
        break;
    }
    //qDebug() << "=>failed to map";
    return 0;
}
#endif

KeyEvent::KeyEvent()
    : Event(KeyPress)
    , _ddKey(0)
    , _sdlKey(0)
    , _scancode(0)
{}

KeyEvent::KeyEvent(State         keyState,
                   int           ddKey,
                   int           sdlKey,
                   int           scancode,
                   const String &keyText,
                   Modifiers     modifiers)
    : Event(keyState == Pressed ? KeyPress : keyState == Repeat ? KeyRepeat : KeyRelease)
    , _ddKey(ddKey)
    , _sdlKey(sdlKey)
    , _scancode(scancode)
    , _text(keyText)
    , _mods(modifiers)
{}

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
    if (_ddKey && ev._ddKey)
    {
        if (_ddKey == ev._ddKey)
        {
            return _mods < ev._mods;
        }
        return _ddKey < ev._ddKey;
    }

    if (_sdlKey && ev._sdlKey)
    {
        if (_sdlKey == ev._sdlKey)
        {
            return _mods < ev._mods;
        }
        return _sdlKey < ev._sdlKey;
    }

    if (_scancode && ev._scancode)
    {
        if (_scancode == ev._scancode)
        {
            return _mods < ev._mods;
        }
        return _scancode < ev._scancode;
    }

    if (_text && ev._text)
    {
        if (_text == ev._text)
        {
            return _mods < ev._mods;
        }
        return _text < ev._text;
    }

    return false;
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

KeyEvent KeyEvent::press(int ddKey, KeyEvent::Modifiers mods)
{
    return {Pressed, ddKey, 0, 0, "", mods};
}

} // namespace de
