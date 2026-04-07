/** @file textcanvas.cpp Text-based drawing surface.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/term/textcanvas.h"

namespace de { namespace term {

DE_PIMPL_NOREF(TextCanvas)
{
    Size               size;
    List<AttribChar *> lines;

    struct RichFormat {
        AttribChar::Attribs attrib;
        String::ByteRange   range;
    };
    List<RichFormat> richFormats;

    Impl(const Size &initialSize) : size(initialSize)
    {
        // Allocate lines based on supplied initial size.
        for (duint row = 0; row < size.y; ++row)
        {
            lines.append(makeLine());
        }
    }

    ~Impl()
    {
        for (duint i = 0; i < lines.size(); ++i)
        {
            delete [] lines[i];
        }
    }

    dsize lineCount() const
    {
        return lines.size();
    }

    AttribChar *makeLine()
    {
        return new AttribChar[size.x];
    }

    void resize(const Size &newSize)
    {
        if (newSize == size) return;

        // Allocate or free lines.
        while (newSize.y < lineCount())
        {
            lines.removeLast();
        }
        while (newSize.y > lineCount())
        {
            lines.append(makeLine());
        }

        DE_ASSERT(lineCount() == newSize.y);
        size.y = newSize.y;

        // Make sure all lines are the correct width.
        for (duint row = 0; row < lines.size(); ++row)
        {
            AttribChar *newLine = new AttribChar[newSize.x];
            std::memcpy(newLine, lines[row], sizeof(AttribChar) * de::min(size.x, newSize.x));
            delete [] lines[row];
            lines[row] = newLine;
        }

        size.x = newSize.x;
    }

    void markAllAsDirty(bool markDirty)
    {
        for (duint row = 0; row < lines.size(); ++row)
        {
            auto *line = lines[row];
            for (duint col = 0; col < size.x; ++col)
            {
                applyFlagOperation(line[col].attribs, AttribChar::Dirty, markDirty);
            }
        }
    }

    AttribChar::Attribs richAttribsForTextIndex(BytePos pos, BytePos offset = BytePos(0)) const
    {
        AttribChar::Attribs attr;
        for (const RichFormat &rf : richFormats)
        {
            if (rf.range.contains(pos + offset))
            {
                attr |= rf.attrib;
            }
        }
        return attr;
    }
};

TextCanvas::TextCanvas(const Size &size) : d(new Impl(size))
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
    return Rectanglei(0, 0, size().x, size().y);
}

void TextCanvas::resize(const Size &newSize)
{
    d->resize(newSize);
}

TextCanvas::AttribChar &TextCanvas::at(const Coord &pos)
{
    DE_ASSERT(isValid(pos));
    return d->lines[pos.y][pos.x];
}

const TextCanvas::AttribChar &TextCanvas::at(const Coord &pos) const
{
    DE_ASSERT(isValid(pos));
    return d->lines[pos.y][pos.x];
}

bool TextCanvas::isValid(const Coord &pos) const
{
    return (pos.x >= 0 && pos.y >= 0 && pos.x < int(d->size.x) && pos.y < int(d->size.y));
}

void TextCanvas::markDirty()
{
    d->markAllAsDirty(true);
}

void TextCanvas::clear(const AttribChar &ch)
{
    fill(Rectanglei(0, 0, d->size.x, d->size.y), ch);
}

void TextCanvas::fill(const Rectanglei &rect, const AttribChar &ch)
{
    for (int y = rect.top(); y < rect.bottom(); ++y)
    {
        for (int x = rect.left(); x < rect.right(); ++x)
        {
            Coord const xy(x, y);
            if (isValid(xy)) at(xy) = ch;
        }
    }
}

void TextCanvas::put(const Vec2i &pos, const AttribChar &ch)
{
    if (isValid(pos))
    {
        at(pos) = ch;
    }
}

void TextCanvas::clearRichFormat()
{
    d->richFormats.clear();
}

void TextCanvas::setRichFormatRange(const AttribChar::Attribs &attribs, const String::ByteRange &range)
{
    Impl::RichFormat rf;
    rf.attrib = attribs;
    rf.range  = range;
    d->richFormats.append(rf);
}

void TextCanvas::drawText(const Vec2i &              pos,
                          const String &             text,
                          const AttribChar::Attribs &attribs,
                          BytePos                    richOffset)
{
    Vec2i p = pos;
//    duint i = 0;
    //for (duint i = 0; i < text.size(); ++i)
    for (String::const_iterator i = text.begin(), end = text.end(); i != end; ++i/*, ++i*/)
    {
        if (isValid(p))
        {
            at(p) = AttribChar(*i, attribs | d->richAttribsForTextIndex(i.pos(), richOffset));
        }
        p.x++;
    }
}

void TextCanvas::drawWrappedText(const Vec2i &              pos,
                                 const String &             text,
                                 const ILineWrapping &      wraps,
                                 const AttribChar::Attribs &attribs,
                                 const Alignment &          lineAlignment)
{
    const int width = wraps.width();

    for (int y = 0; y < wraps.height(); ++y)
    {
        const WrappedLine span = wraps.line(y);
        const auto part = span.range;
        int x = 0;
        if (lineAlignment.testFlag(AlignRight))
        {
            x = width - span.width; //part.sizei();
        }
        else if (!lineAlignment.testFlag(AlignLeft))
        {
            x = width/2 - span.width/2;
        }
        drawText(pos + Vec2i(x, y), part, attribs, span.range.begin().pos(text));
    }
}

void TextCanvas::drawLineRect(const Rectanglei &rect, const AttribChar::Attribs &attribs)
{
    AttribChar const corner('+', attribs);
    AttribChar const hEdge ('-', attribs);
    AttribChar const vEdge ('|', attribs);

    // Horizontal edges.
    for (duint x = 1; x < rect.width() - 1; ++x)
    {
        put(rect.topLeft + Vec2i(x, 0), hEdge);
        put(rect.bottomLeft() + Vec2i(x, -1), hEdge);
    }

    // Vertical edges.
    for (duint y = 1; y < rect.height() - 1; ++y)
    {
        put(rect.topLeft + Vec2i(0, y), vEdge);
        put(rect.topRight() + Vec2i(-1, y), vEdge);
    }

    put(rect.topLeft, corner);
    put(rect.topRight() - Vec2i(1, 0), corner);
    put(rect.bottomRight - Vec2i(1, 1), corner);
    put(rect.bottomLeft() - Vec2i(0, 1), corner);
}

void TextCanvas::draw(const TextCanvas &canvas, const Coord &topLeft)
{
    for (duint y = 0; y < canvas.d->size.y; ++y)
    {
        for (duint x = 0; x < canvas.d->size.x; ++x)
        {
            Coord const xy(x, y);
            const Coord p = topLeft + xy;
            if (isValid(p))
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

void TextCanvas::setCursorPosition(const Vec2i &) {}

}} // namespace de::term
