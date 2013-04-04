/** @file keyeventsource.h  Object that produces keyboard input events.
 *
 * Long summary of the functionality.
 *
 * @todo Update the fields above as appropriate.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_KEYEVENTSOURCE_H
#define LIBGUI_KEYEVENTSOURCE_H

#include "libgui.h"
#include "ddkey.h"
#include <de/Observers>
#include <de/String>

namespace de {

/**
 * Object that produces keyboard events.
 */
class LIBGUI_PUBLIC KeyEventSource
{
public:
    enum KeyState
    {
        Released,   ///< Released button.
        Pressed     ///< Pressed button.
    };

    DENG2_DEFINE_AUDIENCE(KeyEvent, void keyEvent(KeyState state, int ddKey, int nativeCode, String const &text))

public:
    virtual ~KeyEventSource() {}

    /**
     * Translates a Qt key code to a Doomsday key code (see ddkey.h).
     *
     * @param qtKey             Qt key code.
     * @param nativeVirtualKey  Native virtual key code.
     * @param nativeScanCode    Native scan code.
     *
     * @return DDKEY code.
     */
    static int ddKeyFromQt(int qtKey, int nativeVirtualKey, int nativeScanCode);
};

} // namespace de

#endif // LIBGUI_KEYEVENTSOURCE_H
