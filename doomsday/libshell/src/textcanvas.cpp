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

#include "de/shell/TextCanvas"
#include <QList>
#include <QDebug>

namespace de {
namespace shell {

DENG2_PIMPL_NOREF(TextCanvas)
{
    Size size;
    QList<Char *> lines;

    struct RichFormat
    {
        Char::Attribs attrib;
        Range range;
    };
    QList<RichFormat> richFormats;

    Instance(Size const &initialSize) : size(initialSize)
    {
        // Allocate lines based on supplied initial size.
        for(int row = 0; row < size.y; ++row)
        {
            lines.append(makeLine());
        }
    }

    ~Instance()
    {
        for(int i = 0; i < lines.size(); ++i)
        {
            delete [] lines[i];
        }
    }

    Char *makeLine()
    {
        return new Char[size.x];
    }

    void resize(Size const &newSize)
    {
        if(newSize == size) return;

        // Allocate or free lines.
        while(newSize.y < lines.size())
        {
            lines.removeLast();
        }
        while(newSize.y > lines.size())
        {
            lines.append(makeLine());
        }

        Q_ASSERT(lines.size() == newSize.y);
        size.y = newSize.y;

        // Make sure all lines are the correct width.
        for(int row = 0; row < lines.size(); ++row)
        {
            Char *newLine = new Char[newSize.x];
            memcpy(newLine, lines[row], sizeof(Char) * de::min(size.x, newSize.x));
            delete [] lines[row];
            lines[row] = newLine;
        }

        size.x = newSize.x;
    }

    void markAllAsDirty(bool markDirty)
    {
        for(int row = 0; row < lines.size(); ++row)
        {
            Char *line = lines[row];
            for(int col = 0; col < size.x; ++col)
            {
                Char &c = line[col];
                if(markDirty)
                    c.attribs |= Char::Dirty;
                else
                    c.attribs &= ~Char::Dirty;
            }
        }
    }

    Char::Attribs richAttribsForTextIndex(int pos, int offset = 0) const
    {
        Char::Attribs attr;
        foreach(RichFormat const &rf, richFormats)
        {
            if(rf.range.contains(offset + pos))
            {
                attr |= rf.attrib;
            }
        }
        return attr;
    }
};

TextCanvas::TextCanvas(Size const &size) : d(new Instance(size))
{
    d->size = size;
}

TextCanvas::~TextCanvas()
{}

TextCanvas::Size TextCanvas::size() const
{
    return d->size;
}

int TextCanvas::width() const
{
    return d->size.x;
}

int TextCanvas::height() const
{
    return d->size.y;
}

Rectanglei TextCanvas::rect() const
{
    return Rectanglei(Vector2i(0, 0), size());
}

void TextCanvas::resize(Size const &newSize)
{
    d->resize(newSize);
}

TextCanvas::Char &TextCanvas::at(Coord const &pos)
{
    Q_ASSERT(isValid(pos));
    return d->lines[pos.y][pos.x];
}

TextCanvas::Char const &TextCanvas::at(Coord const &pos) const
{
    Q_ASSERT(isValid(pos));
    return d->lines[pos.y][pos.x];
}

bool TextCanvas::isValid(const Coord &pos) const
{
    return (pos.x >= 0 && pos.y >= 0 && pos.x < d->size.x && pos.y < d->size.y);
}

void TextCanvas::markDirty()
{
    d->markAllAsDirty(true);
}

void TextCanvas::clear(Char const &ch)
{
    fill(Rectanglei(Vector2i(0, 0), d->size), ch);
}

void TextCanvas::fill(Rectanglei const &rect, Char const &ch)
{
    for(int y = rect.top(); y < rect.bottom(); ++y)
    {
        for(int x = rect.left(); x < rect.right(); ++x)
        {
            Coord const xy(x, y);
            if(isValid(xy)) at(xy) = ch;
        }
    }
}

void TextCanvas::put(Vector2i const &pos, Char const &ch)
{
    if(isValid(pos))
    {
        at(pos) = ch;
    }
}

void TextCanvas::clearRichFormat()
{
    d->richFormats.clear();
}

void TextCanvas::setRichFormatRange(Char::Attribs const &attribs, Range const &range)
{
    Instance::RichFormat rf;
    rf.attrib = attribs;
    rf.range = range;
    d->richFormats.append(rf);
}

void TextCanvas::drawText(Vector2i const &pos, String const &text,
                          Char::Attribs const &attribs, int richOffset)
{
    Vector2i p = pos;
    for(int i = 0; i < text.size(); ++i)
    {
        if(isValid(p))
        {
            at(p) = Char(text[i], attribs | d->richAttribsForTextIndex(i, richOffset));
        }
        p.x++;
    }
}

void TextCanvas::drawWrappedText(Vector2i const &pos, String const &text,
                                 LineWrapping const &wraps, Char::Attribs const &attribs,
                                 Alignment lineAlignment)
{
    int const width = wraps.width();

    for(int y = 0; y < wraps.size(); ++y)
    {
        WrappedLine const &span = wraps[y];
        String part = text.substr(span.range.start, span.range.size());
        int x = 0;
        if(lineAlignment.testFlag(AlignRight))
        {
            x = width - part.size();
        }
        else if(!lineAlignment.testFlag(AlignLeft))
        {
            x = width/2 - part.size()/2;
        }
        drawText(pos + Vector2i(x, y), part, attribs, span.range.start);
    }
}

void TextCanvas::drawLineRect(Rectanglei const &rect, Char::Attribs const &attribs)
{
    Char const corner('+', attribs);
    Char const hEdge ('-', attribs);
    Char const vEdge ('|', attribs);

    // Horizontal edges.
    for(int x = 1; x < rect.width() - 1; ++x)
    {
        put(rect.topLeft + Vector2i(x, 0), hEdge);
        put(rect.bottomLeft() + Vector2i(x, -1), hEdge);
    }

    // Vertical edges.
    for(int y = 1; y < rect.height() - 1; ++y)
    {
        put(rect.topLeft + Vector2i(0, y), vEdge);
        put(rect.topRight() + Vector2i(-1, y), vEdge);
    }

    put(rect.topLeft, corner);
    put(rect.topRight() - Vector2i(1, 0), corner);
    put(rect.bottomRight - Vector2i(1, 1), corner);
    put(rect.bottomLeft() - Vector2i(0, 1), corner);
}

void TextCanvas::draw(TextCanvas const &canvas, Coord const &topLeft)
{
    for(int y = 0; y < canvas.d->size.y; ++y)
    {
        for(int x = 0; x < canvas.d->size.x; ++x)
        {
            Coord const xy(x, y);
            Coord const p = topLeft + xy;
            if(isValid(p))
            {
                at(p) = canvas.at(xy);
            }
        }
    }
}

void TextCanvas::show()
{
    d->markAllAsDirty(false);
}

void TextCanvas::setCursorPosition(Vector2i const &)
{}

} // namespace shell
} // namespace de
