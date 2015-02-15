/** @file untrapper.cpp  Mouse untrapping utility.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Untrapper"

namespace de {

DENG2_PIMPL(Untrapper)
{
    CanvasWindow &window;
    bool wasTrapped;

    Instance(Public *i, CanvasWindow &w) : Base(i), window(w)
    {
        wasTrapped = window.canvas().isMouseTrapped();
        if(wasTrapped)
        {
            window.canvas().trapMouse(false);
        }
    }

    ~Instance()
    {
        if(wasTrapped)
        {
            window.canvas().trapMouse();
        }
    }
};

Untrapper::Untrapper(CanvasWindow &window) : d(new Instance(this, window))
{}

} // namespace de
