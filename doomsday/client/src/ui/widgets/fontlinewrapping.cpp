/** @file fontlinewrapping.cpp  Font line wrapping.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/fontlinewrapping.h"

using namespace de;
using namespace de::shell;

DENG2_PIMPL_NOREF(FontLineWrapping)
{
    Font const *font;
    struct Line {
        WrappedLine line;
        int width;
        int indent;

        Line(WrappedLine const &ln = WrappedLine(Rangei()), int w = 0, int ind = 0)
            : line(ln), width(w), indent(ind) {}
    };
    QList<Line> lines;
    String text;
    Font::RichFormat format;
    int indent;

    Instance() : font(0), indent(0) {}

    String rangeText(Rangei const &range) const
    {
        return text.substr(range.start, range.size());
    }

    int rangeVisibleWidth(Rangei const &range) const
    {
        if(font)
        {
            return font->measure(rangeText(range), format.subRange(range)).width();
        }
        return 0;
    }

    int rangeAdvanceWidth(Rangei const &range) const
    {
        if(font)
        {
            return font->advanceWidth(rangeText(range), format.subRange(range));
        }
        return 0;
    }

    void appendLine(Rangei const &range)
    {
        lines.append(Line(WrappedLine(range), rangeVisibleWidth(range), indent));

        // Check for possible indent for following lines.
        Font::RichFormat rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);
        int newIndent = indent;
        while(iter.hasNext())
        {
            iter.next();
            if(iter.markIndent())
            {
                newIndent = indent + rangeAdvanceWidth(Rangei(range.start, iter.range().start));
            }
        }
        indent = newIndent;
    }
};

FontLineWrapping::FontLineWrapping() : d(new Instance)
{}

void FontLineWrapping::setFont(Font const &font)
{
    d->font = &font;
}

Font const &FontLineWrapping::font() const
{
    DENG2_ASSERT(d->font != 0);
    return *d->font;
}

bool FontLineWrapping::isEmpty() const
{
    return d->lines.isEmpty();
}

void FontLineWrapping::clear()
{
    d->lines.clear();
    d->text.clear();
}

void FontLineWrapping::wrapTextToWidth(String const &text, int maxWidth)
{
    wrapTextToWidth(text, Font::RichFormat::fromPlainText(text), maxWidth);
}

void FontLineWrapping::wrapTextToWidth(String const &text, Font::RichFormat const &format, int maxWidth)
{
    QChar const newline('\n');

    clear();

    int const MIN_LINE_WIDTH = 120;

    if(maxWidth <= 1 || !d->font) return;

    // This is the text that we will be wrapping.
    d->text   = text;
    d->format = format;

    int begin = 0;
    forever
    {
        // How much width is available, taking indentation into account?
        if(maxWidth - d->indent < MIN_LINE_WIDTH)
        {
            // There is no room for this indent...
            d->indent = de::max(0, maxWidth - MIN_LINE_WIDTH);
        }
        int availWidth = maxWidth - d->indent;

        // Quick check: does the remainder fit?
        Rangei range(begin, text.size());
        int visWidth = d->rangeVisibleWidth(range);
        if(visWidth <= availWidth)
        {
            d->lines.append(Instance::Line(WrappedLine(Rangei(begin, text.size())),
                                           visWidth, d->indent));
            break;
        }

        // Newlines always cause a wrap.
        int end = begin;
        int wrapPosMax = begin + 1;
        while(end < text.size() && text.at(end) != newline)
        {
            ++end;

            if(d->rangeVisibleWidth(Rangei(begin, end)) > availWidth)
            {
                // Went too far.
                wrapPosMax = --end;
                break;
            }
        }

        DENG2_ASSERT(end != text.size());

        // Find a good (whitespace) break point.
        while(!text.at(end).isSpace())
        {
            if(--end == begin)
            {
                // Ran out of non-space chars, force a break.
                end = wrapPosMax;
                break;
            }
        }

        if(text.at(end) == newline)
        {
            // The newline will be omitted from the wrapped lines.
            d->appendLine(Rangei(begin, end));
            begin = end + 1;
        }
        else
        {
            while(end < text.size() && text.at(end).isSpace()) ++end;
            d->appendLine(Rangei(begin, end));
            begin = end;
        }
    }

    // Mark the final line.
    d->lines.last().line.isFinal = true;
}

String const &FontLineWrapping::text() const
{
    return d->text;
}

WrappedLine FontLineWrapping::line(int index) const
{
    DENG2_ASSERT(index >= 0 && index < height());
    return d->lines[index].line;
}

int FontLineWrapping::width() const
{
    int w = 0;
    for(int i = 0; i < d->lines.size(); ++i)
    {
        w = de::max(w, d->lines[i].width);
    }
    return w;
}

int FontLineWrapping::height() const
{
    return d->lines.size();
}

int FontLineWrapping::rangeWidth(Rangei const &range) const
{
    return d->rangeAdvanceWidth(range);
}

int FontLineWrapping::indexAtWidth(Rangei const &range, int width) const
{
    int prevWidth = 0;

    for(int i = range.start; i < range.end; ++i)
    {
        int const rw = d->rangeAdvanceWidth(Rangei(range.start, i));
        if(rw >= width)
        {
            // Which is closer, this or the previous char?
            if(de::abs(rw - width) <= de::abs(prevWidth - width))
            {
                return i;
            }
            return i - 1;
        }
        prevWidth = rw;
    }
    return range.end;
}

int FontLineWrapping::totalHeightInPixels() const
{
    if(!d->font) return 0;

    int const lines = height();
    int pixels = 0;

    if(lines > 1)
    {
        // Full baseline-to-baseline spacing.
        pixels += (lines - 1) * d->font->lineSpacing().value();
    }
    if(lines > 0)
    {
        // The last (or a single) line is just one 'font height' tall.
        pixels += d->font->height().value();
    }
    return pixels;
}

Vector2i FontLineWrapping::charTopLeftInPixels(int line, int charIndex)
{    
    if(line >= height()) return Vector2i();

    WrappedLine const span = d->lines[line].line;
    Rangei const range(span.range.start,
                       de::min(span.range.end, span.range.start + charIndex));

    Vector2i cp;
    cp.x = d->font->advanceWidth(d->rangeText(range));
    cp.y = line * d->font->lineSpacing().valuei();

    return cp;
}

int FontLineWrapping::lineIndent(int index) const
{
    return d->lines[index].indent;
}
