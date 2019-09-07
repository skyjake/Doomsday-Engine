/** @file cursestextcanvas.cpp Text-based drawing surface for curses.
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

#include "cursestextcanvas.h"

using namespace de;

DE_PIMPL_NOREF(CursesTextCanvas)
{
    WINDOW *window;
    Coord   origin;
    Vec2i   cursorPos;

    Impl(WINDOW *window, const Coord &originInWindow)
        : window(window), origin(originInWindow)
    {}
};

CursesTextCanvas::CursesTextCanvas(const Size &size, WINDOW *window, const Coord &originInWindow)
    : TextCanvas(size), d(new Impl(window, originInWindow))
{}

void CursesTextCanvas::setCursorPosition(const Vec2i &pos)
{
    d->cursorPos = pos;
}

void CursesTextCanvas::show()
{
    const Size dims = size();

    // All dirty characters are drawn.
    for (duint row = 0; row < dims.y; ++row)
    {
        bool needMove = true;

        for (duint col = 0; col < dims.x; ++col)
        {
            const Coord pos(col, row);
            const AttribChar &ch = at(pos);

            if (!ch.isDirty())
            {
                needMove = true;
                continue;
            }

            if (needMove)
            {
                // Advance cursor.
                wmove(d->window, d->origin.y + row, d->origin.x + col);
                needMove = false;
            }

            // Set attributes.
            wattrset(d->window,
                     (ch.attribs.testFlag(AttribChar::Bold)?      A_BOLD : 0) |
                     (ch.attribs.testFlag(AttribChar::Reverse)?   A_REVERSE : 0) |
                     (ch.attribs.testFlag(AttribChar::Underline)? A_UNDERLINE : 0) |
                     (ch.attribs.testFlag(AttribChar::Blink)?     A_BLINK : 0));

            /// @todo What about Unicode output? (libncursesw?)
            waddch(d->window, ch.ch); // cursor advanced
        }
    }

    // Mark everything clean.
    TextCanvas::show();

    wmove(d->window, d->cursorPos.y, d->cursorPos.x);
    wrefresh(d->window);
}
