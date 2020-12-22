/** @file keyeventsource.h  Object that produces keyboard input events.
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

#ifndef LIBGUI_KEYEVENTSOURCE_H
#define LIBGUI_KEYEVENTSOURCE_H

#include "libgui.h"
#include "keyevent.h"
#include <de/observers.h>
#include <de/string.h>

namespace de {

/**
 * Object that produces keyboard events.
 * @ingroup gui
 */
class LIBGUI_PUBLIC KeyEventSource
{
public:
    DE_AUDIENCE(KeyEvent, void keyEvent(const KeyEvent &))

public:
    KeyEventSource();
    virtual ~KeyEventSource() {}

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_KEYEVENTSOURCE_H
