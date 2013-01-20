/** @file textcanvas.cpp Text-based drawing surface.
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

#include "textcanvas.h"
#include <QList>

struct TextCanvas::Instance
{
    Size size;

    typedef QList<Char> Line;
    QList<Line> lines;

    Instance(Size const &initialSize) : size(initialSize)
    {
        // Allocate lines based on supplied initial size.
        for(int row = 0; row < size.y; ++row)
        {
            lines.append(makeLine());
        }
    }

    Line makeLine()
    {
        Line line;
        for(int col = 0; col < size.x; ++col)
        {
            line.append(Char());
        }
        return line;
    }

    void resize(Size const &newSize)
    {
        if(newSize == size) return;

        // Allocate or free lines.
        while(newSize.y < size.y)
        {
            lines.removeLast();
            size.y--;
        }
        while(newSize.y > size.y)
        {
            lines.append(makeLine());
            size.y++;
        }

        // Make sure all lines are the correct width.
        for(int row = 0; row < lines.size(); ++row)
        {
            Line &line = lines[row];
            if(line.size() == newSize.x) continue;

            while(newSize.x < line.size())
            {
                line.removeLast();
            }
            while(newSize.x > line.size())
            {
                line.append(Char());
            }
        }

        Q_ASSERT(lines.size() == size.y);
    }

    void markAllAsDirty(bool markDirty)
    {
        for(int row = 0; row < lines.size(); ++row)
        {
            Line &line = lines[row];
            for(int col = 0; col < line.size(); ++col)
            {
                line[col].attrib.dirty = markDirty;
            }
        }
    }
};

TextCanvas::TextCanvas(Size const &size) : d(new Instance(size))
{
    d->size = size;
}

TextCanvas::~TextCanvas()
{
    delete d;
}

TextCanvas::Size TextCanvas::size() const
{
    return d->size;
}

void TextCanvas::resize(Size const &newSize)
{
    d->resize(newSize);
}

TextCanvas::Char &TextCanvas::at(Coord const &pos)
{
    Q_ASSERT(pos.x >= 0 && pos.x < d->size.x);
    Q_ASSERT(pos.y >= 0 && pos.y < d->size.y);
    Char &c = d->lines[pos.y][pos.x];
    c.attrib.dirty = true;
    return c;
}

TextCanvas::Char const &TextCanvas::at(Coord const &pos) const
{
    Q_ASSERT(pos.x >= 0 && pos.x < d->size.x);
    Q_ASSERT(pos.y >= 0 && pos.y < d->size.y);
    return d->lines[pos.y].at(pos.x);
}

void TextCanvas::show()
{
    d->markAllAsDirty(false);
}
