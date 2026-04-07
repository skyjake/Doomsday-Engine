/** @file guiloop.h  Continually triggered loop.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef GUILOOP_H
#define GUILOOP_H

#include "libgui.h"
#include <de/loop.h>

namespace de {

class GLWindow;

/**
 * Loop that gets triggered after each time GLWindow contents have been refreshed.
 */
class LIBGUI_PUBLIC GuiLoop : public Loop
{
public:
    static GuiLoop &get();

public:
    GuiLoop();

    void setWindow(GLWindow *window);

    void nextLoopIteration() override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // GUILOOP_H
