/** @file cursestextcanvas.cpp Text-based drawing surface for curses.
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

#include "cursestextcanvas.h"

DENG2_PIMPL_NOREF(CursesTextCanvas)
{
    WINDOW *window;
    Coord origin;
    de::Vector2i cursorPos;

    Instance(WINDOW *window, Coord const &originInWindow)
        : window(window), origin(originInWindow)
    {}
};

CursesTextCanvas::CursesTextCanvas(Size const &size, WINDOW *window, Coord const &originInWindow)
    : TextCanvas(size), d(new Instance(window, originInWindow))
{}

void CursesTextCanvas::setCursorPosition(const de::Vector2i &pos)
{
    d->cursorPos = pos;
}

void CursesTextCanvas::show()
{
    Size const dims = size();

    // All dirty characters are drawn.
    for(int row = 0; row < dims.y; ++row)
    {
        bool needMove = true;

        for(int col = 0; col < dims.x; ++col)
        {
            Coord const pos(col, row);
            Char const &ch = at(pos);

            if(!ch.isDirty())
            {
                needMove = true;
                continue;
            }

            if(needMove)
            {
                // Advance cursor.
                wmove(d->window, d->origin.y + row, d->origin.x + col);
                needMove = false;
            }

            // Set attributes.
            wattrset(d->window,
                     (ch.attribs.testFlag(Char::Bold)?      A_BOLD : 0) |
                     (ch.attribs.testFlag(Char::Reverse)?   A_REVERSE : 0) |
                     (ch.attribs.testFlag(Char::Underline)? A_UNDERLINE : 0) |
                     (ch.attribs.testFlag(Char::Blink)?     A_BLINK : 0));

            /// @todo What about Unicode output? (libncursesw?)
            waddch(d->window, ch.ch.toLatin1()); // cursor advanced
        }
    }

    // Mark everything clean.
    TextCanvas::show();

    wmove(d->window, d->cursorPos.y, d->cursorPos.x);
    wrefresh(d->window);
}
