/** @file keyactions.h  Callbacks to be called on key events.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include <de/widget.h>
#include "de/keyevent.h"

namespace de {

/**
 * Callbacks to be called on key events.
 */
class LIBGUI_PUBLIC KeyActions : public Widget
{
public:
    KeyActions();
    void add(const KeyEvent &key, const std::function<void()> &callback);

    bool handleEvent(const Event &) override;

private:
    DE_PRIVATE(d)
};

} // namespace de
