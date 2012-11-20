/**
 * @file keycode.h
 * Keycode translation. @ingroup input
 *
 * @see DDKEY_* in dd_share.h
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

#ifndef LIBDENG_INPUT_KEYCODE_H
#define LIBDENG_INPUT_KEYCODE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Translates a Qt key code to a DDKEY.
 *
 * @param qtKey             Qt key code.
 * @param nativeVirtualKey  Native virtual key code.
 * @param nativeScanCode    Native scan code.
 */
int Keycode_TranslateFromQt(int qtKey, int nativeVirtualKey, int nativeScanCode);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_INPUT_KEYCODE_H
