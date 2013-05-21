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
    QList<WrappedLine> lines;
    String text;

    Instance() : font(0) {}

    String rangeText(Range const &range) const
    {
        return text.substr(range.start, range.size());
    }

    int rangeVisibleWidth(Range const &range) const
    {
        if(font)
        {
            return font->measure(rangeText(range)).width();
        }
        return 0;
    }

    int rangeAdvanceWidth(Range const &range) const
    {
        if(font)
        {
            return font->advanceWidth(rangeText(range));
        }
        return 0;
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
    /**
     * @note This is quite similar to MonospaceLineWrapping::wrapTextToWidth().
     * Perhaps a generic method could be abstracted from these two.
     */

    QChar const newline('\n');

    clear();

    if(maxWidth <= 1 || !d->font) return;

    // This is the text that we will be wrapping.
    d->text = text;

    int begin = 0;
    forever
    {
        // Quick check: does the remainder fit?
        if(d->rangeVisibleWidth(Range(begin, text.size())) <= maxWidth)
        {
            d->lines.append(WrappedLine(Range(begin, text.size())));
            break;
        }

        // Newlines always cause a wrap.
        int end = begin;
        int wrapPosMax = begin + 1;
        while(end < text.size() && text.at(end) != newline)
        {
            ++end;

            if(d->rangeVisibleWidth(Range(begin, end)) > maxWidth)
            {
                // Went too far.
                wrapPosMax = --end;
                break;
            }
        }

        if(end == text.size())
        {
            // Out of characters; time to stop.
            d->lines.append(WrappedLine(Range(begin, text.size())));
            break;
        }

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
            d->lines.append(WrappedLine(Range(begin, end)));
            begin = end + 1;
        }
        else
        {
            while(text.at(end).isSpace()) ++end;
            d->lines.append(WrappedLine(Range(begin, end)));
            begin = end;
            //while(text.at(begin).isSpace()) ++begin;
        }
    }

    // Mark the final line.
    d->lines.last().isFinal = true;
}

WrappedLine FontLineWrapping::line(int index) const
{
    DENG2_ASSERT(index >= 0 && index < height());
    return d->lines[index];
}

int FontLineWrapping::width() const
{
    if(!d->font) return 0;

    int w = 0;
    for(int i = 0; i < d->lines.size(); ++i)
    {
        WrappedLine const &span = d->lines[i];
        w = de::max(w, d->rangeVisibleWidth(span.range));
    }
    return w;
}

int FontLineWrapping::height() const
{
    return d->lines.size();
}

int FontLineWrapping::rangeWidth(Range const &range) const
{
    return d->rangeAdvanceWidth(range);
}

int FontLineWrapping::indexAtWidth(Range const &range, int width) const
{
    int prevWidth = 0;

    for(int i = range.start; i < range.end; ++i)
    {
        int const rw = d->rangeAdvanceWidth(Range(range.start, i));
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
    WrappedLine const span = d->lines[line];
    Range const range(span.range.start, de::min(span.range.end, span.range.start + charIndex));

    Vector2i cp;
    cp.x = d->font->advanceWidth(d->rangeText(range));
    cp.y = line * d->font->lineSpacing().valuei();

    return cp;
}
