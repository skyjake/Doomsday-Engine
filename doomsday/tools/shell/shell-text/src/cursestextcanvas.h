/** @file cursestextcanvas.h Text-based drawing surface for curses.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CURSESTEXTCANVAS_H
#define CURSESTEXTCANVAS_H

#include <curses.h>
#include <de/shell/TextCanvas>

class CursesTextCanvas : public de::shell::TextCanvas
{
public:
    CursesTextCanvas(Size const &size, WINDOW *window, Coord const &originInWindow = Coord(0, 0));

    void setCursorPosition(de::Vector2i const &pos);

    void show();

private:
    DENG2_PRIVATE(d)
};

#endif // CURSESTEXTCANVAS_H
