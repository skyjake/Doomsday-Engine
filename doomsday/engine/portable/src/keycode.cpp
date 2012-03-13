/**
 * @file keycode.cpp
 * Keycode translation. @ingroup input
 *
 * Depends on Qt GUI.
 *
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "keycode.h"
#include "dd_share.h"
#include <QKeyEvent>
#include <QDebug>

int Keycode_TranslateFromQt(int qtKey, int nativeVirtualKey)
{   
    // Non-character-inserting keys.
    switch(qtKey)
    {
    case Qt::Key_Escape:        return DDKEY_ESCAPE;
    case Qt::Key_Tab:           return DDKEY_TAB;
    case Qt::Key_Backspace:     return DDKEY_BACKSPACE;
    case Qt::Key_Space:         return ' ';
    case Qt::Key_Pause:         return DDKEY_PAUSE;
    case Qt::Key_Up:            return DDKEY_UPARROW;
    case Qt::Key_Down:          return DDKEY_DOWNARROW;
    case Qt::Key_Left:          return DDKEY_LEFTARROW;
    case Qt::Key_Right:         return DDKEY_RIGHTARROW;
    case Qt::Key_Control:       return DDKEY_RCTRL;
    case Qt::Key_Shift:         return DDKEY_RSHIFT;
    case Qt::Key_Alt:           return DDKEY_RALT;
    case Qt::Key_AltGr:         return DDKEY_LALT;
    case Qt::Key_Return:        return DDKEY_RETURN;
    case Qt::Key_F1:            return DDKEY_F1;
    case Qt::Key_F2:            return DDKEY_F2;
    case Qt::Key_F3:            return DDKEY_F3;
    case Qt::Key_F4:            return DDKEY_F4;
    case Qt::Key_F5:            return DDKEY_F5;
    case Qt::Key_F6:            return DDKEY_F6;
    case Qt::Key_F7:            return DDKEY_F7;
    case Qt::Key_F8:            return DDKEY_F8;
    case Qt::Key_F9:            return DDKEY_F9;
    case Qt::Key_F10:           return DDKEY_F10;
    case Qt::Key_F11:           return DDKEY_F11;
    case Qt::Key_F12:           return DDKEY_F12;
    case Qt::Key_NumLock:       return DDKEY_NUMLOCK;
    case Qt::Key_ScrollLock:    return DDKEY_SCROLL;
    case Qt::Key_Enter:         return DDKEY_ENTER;
    case Qt::Key_Insert:        return DDKEY_INS;
    case Qt::Key_Delete:        return DDKEY_DEL;
    case Qt::Key_Home:          return DDKEY_HOME;
    case Qt::Key_End:           return DDKEY_END;
    case Qt::Key_PageUp:        return DDKEY_PGUP;
    case Qt::Key_PageDown:      return DDKEY_PGDN;
    case Qt::Key_SysReq:        return DDKEY_PRINT;
    case Qt::Key_Print:         return DDKEY_PRINT;
    case Qt::Key_CapsLock:      return DDKEY_CAPSLOCK;

    default:
        break;
    }

#ifdef MACOSX
    switch(qtKey)
    {
    case Qt::Key_Meta:          return DDKEY_RCTRL;
    case Qt::Key_Control:       return 0; // Don't map the Command key.
    case Qt::Key_F14:           return DDKEY_PAUSE; // No pause key on the Mac.
    case Qt::Key_F15:           return DDKEY_PRINT;

    default:
        break;
    }

    /// We'll have to use the native virtual keys to make a distinction, e.g.,
    /// between the number row and the keypad. These are the real physical keys
    /// -- the insertion text is provided outside this mapping.

    switch(nativeVirtualKey)
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

        // kVK_ANSI_KeypadClear     0x47
        // kVK_ANSI_KeypadMultiply  0x43

    default:
        break;
    }
#endif

    // TODO: Windows virtual keys

    // Not supported.
    qDebug() << "Keycode" << qtKey << QString("0x%1").arg(qtKey, 0, 16)
             << "virtualKey" << nativeVirtualKey << "not translated.";

    return 0;
}
