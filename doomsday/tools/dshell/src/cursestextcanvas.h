/** @file cursestextcanvas.h Text-based drawing surface for curses.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/term/textcanvas.h>

class CursesTextCanvas : public de::term::TextCanvas
{
public:
    CursesTextCanvas(const Size &size, WINDOW *window, const Coord &originInWindow = Coord(0, 0));

    void setCursorPosition(const de::Vec2i &pos);

    void show();

private:
    DE_PRIVATE(d)
};

#endif // CURSESTEXTCANVAS_H
