/** @file fontlinewrapping.cpp  Font line wrapping.
 *
 * @todo Performance|Refactor: Add a class dedicated for measuring text. Allow
 * measuring in increments, one character at a time, without re-measuring the
 * whole range. Allow seeking forward and backward with the measurer.
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

#include <QMap>

using namespace de;
using namespace de::shell;

static QChar const NEWLINE('\n');

DENG2_PIMPL_NOREF(FontLineWrapping)
{
    Font const *font;

    struct Line
    {
        WrappedLine line;
        LineInfo info;
        int width; ///< Total width of the line (in pixels).
        bool isTabbed;

        Line(WrappedLine const &ln = WrappedLine(Rangei()), int lineWidth = 0, int leftIndent = 0)
            : line(ln), width(lineWidth), isTabbed(false)
        {
            info.indent = leftIndent;
        }
    };

    typedef QList<Line *> Lines;
    Lines lines;

    int maxWidth;
    String text;                ///< Plain text.
    Font::RichFormat format;
    int indent;                 ///< Current left indentation (in pixels).
    int tabStop;

    Instance() : font(0), indent(0), tabStop(0) {}

    ~Instance()
    {
        clearLines();
    }

    void clearLines()
    {
        qDeleteAll(lines);
        lines.clear();
    }

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

    int rangeIndentMarkWidth(Rangei const &range) const
    {
        Font::RichFormat rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);
        int markWidth = 0;
        while(iter.hasNext())
        {
            iter.next();
            if(iter.markIndent())
            {
                markWidth = rangeAdvanceWidth(Rangei(0, iter.range().start) + range.start);
            }
        }
        return markWidth;
    }

    /**
     * Constructs a wrapped line. Note that indent and tabStop are modified.
     * @param range
     * @param width
     * @return Caller gets ownership of the Line.
     */
    Line *makeLine(Rangei const &range, int width = -1)
    {
        if(width < 0)
        {
            // Determine the full width now.
            width = rangeVisibleWidth(range);
        }

        Line *line = new Line(WrappedLine(range), width, indent);

        // Determine segments in the line.
        int pos = range.start;

        Font::RichFormat rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);

        while(iter.hasNext())
        {
            iter.next();

            if(iter.tabStop() != tabStop)
            {
                int const start = range.start + iter.range().start;
                if(start > pos)
                {
                    line->info.segs << LineInfo::Segment(Rangei(pos, start), tabStop);
                    pos = start;
                }

                tabStop = iter.tabStop();
            }
        }

        // The final segment.
        line->info.segs << LineInfo::Segment(Rangei(pos, range.end), tabStop);

        // Determine segment widths.
        if(line->info.segs.size() == 1)
        {
            line->info.segs[0].width = width;
        }
        else
        {
            for(int i = 0; i < line->info.segs.size(); ++i)
            {
                line->info.segs[i].width = (
                            i < line->info.segs.size() - 1?
                                rangeAdvanceWidth(line->info.segs[i].range) :
                                rangeVisibleWidth(line->info.segs[i].range));
            }
        }

        // Check for possible indent for following lines.
        indent += rangeIndentMarkWidth(range);

        return line;
    }

    bool isAllSpace(Rangei const &range) const
    {
        for(int i = range.start; i < range.end; ++i)
        {
            if(!text.at(i).isSpace())
                return false;
        }
        return true;
    }

    bool containsNewline(Rangei const &range) const
    {
        for(int i = range.start; i < range.end; ++i)
        {
            if(text.at(i) == NEWLINE)
                return true;
        }
        return false;
    }

    bool containsTabs(Rangei const &range) const
    {
        Font::RichFormat rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);
        while(iter.hasNext())
        {
            iter.next();
            if(iter.tabStop() > 0) return true;
        }
        return false;
    }

    QList<int> findTabsInUse(Rangei const &range) const
    {
        QList<int> stops;
        Font::RichFormat rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);
        while(iter.hasNext())
        {
            iter.next();
            if(iter.tabStop() > 0) stops << iter.tabStop();
        }
        return stops;
    }

    int findMaxWrapWithStep(int const stepSize, int const begin, int end,
                            int const availableWidth,
                            int *wrapPosMax)
    {
        int bestEnd = end;
        int stepCounter = stepSize;

        if(wrapPosMax) *wrapPosMax = begin + 1;

        while(end < text.size() && text.at(end) != NEWLINE)
        {
            ++end;

            if(!--stepCounter)
            {
                stepCounter = stepSize;

                if(rangeAdvanceWidth(Rangei(begin, end)) > availableWidth)
                {
                    // Went too far.
                    if(wrapPosMax) *wrapPosMax = --end;
                    break;
                }
                // We have verified this fits in the available width.
                bestEnd = end;
            }
        }
        return bestEnd;
    }

    int findMaxWrap(int const begin, int const availableWidth, int &wrapPosMax)
    {
        // Crude search.
        int end = findMaxWrapWithStep(8, begin, begin, availableWidth, NULL);

        // Accurate search.
        return findMaxWrapWithStep(1, begin, end, availableWidth, &wrapPosMax);
    }

    bool isWrappable(int at)
    {
        if(text.at(at).isSpace()) return true;
        if(at > 0)
        {
            QChar const prev = text.at(at - 1);
            if(prev == '/' || prev == '\\') return true;
        }
        return false;
    }

    Rangei untilNextNewline(int start) const
    {
        int pos = start;
        while(pos < text.size())
        {
            // The newline is omitted from the range.
            if(text[pos] == '\n') break;

            ++pos;
        }
        return Rangei(start, pos);
    }

    /**
     * Wraps the range onto one or more lines.
     *
     * @param rangeToWrap    Range in the content string.
     * @param maxWidth       Maximum width of a line.
     * @param initialIndent
     * @param initialTabStop
     *
     * @return Produced wrapped lines. Caller gets ownership.
     */
    Lines wrapRange(Rangei const &rangeToWrap, int maxWidth, int initialIndent = 0, int initialTabStop = 0)
    {
        int const MIN_LINE_WIDTH = 120;

        indent    = initialIndent;
        tabStop   = initialTabStop;
        int begin = rangeToWrap.start;

        Lines wrappedLines;
        forever
        {
            // How much width is available, taking indentation into account?
            if(maxWidth - indent < MIN_LINE_WIDTH)
            {
                // There is no room for this indent...
                indent = de::max(0, maxWidth - MIN_LINE_WIDTH);
            }
            int availWidth = maxWidth - indent;

            // Range for the remainder of the text.
            Rangei const range(begin, rangeToWrap.end);

            // Quick check: does the complete remainder fit?
            if(!containsNewline(range))
            {
                int visWidth = rangeAdvanceWidth(range);
                if(visWidth <= availWidth)
                {
                    wrappedLines << makeLine(range, visWidth);
                    break;
                }
            }

            // Newlines always cause a wrap.
            int wrapPosMax;
            int end = findMaxWrap(begin, availWidth, wrapPosMax);
            if(end <= begin) break;

            if(text.at(end) == NEWLINE)
            {
                // The newline will be omitted from the wrapped lines.
                wrappedLines << makeLine(Rangei(begin, end));
                begin = end + 1;
            }
            else
            {
                // Rewind to find a good (whitespace) break point.
                while(!isWrappable(end))
                {
                    if(--end == begin)
                    {
                        // Ran out of non-space chars, force a break.
                        end = wrapPosMax;
                        break;
                    }
                }

                DENG2_ASSERT(end > begin);

                // If there is only whitespace remaining on the line,
                // just use the max wrap -- blank lines are not pretty.
                if(isAllSpace(Rangei(begin, end)))
                {
                    end = wrapPosMax;
                }

                while(end < text.size() && text.at(end).isSpace()) ++end;
                wrappedLines << makeLine(Rangei(begin, end));
                begin = end;
            }
        }

        return wrappedLines;
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
    reset();
    d->text.clear();
}

void FontLineWrapping::reset()
{
    d->clearLines();
    d->indent = 0;
    d->tabStop = 0;
}

void FontLineWrapping::wrapTextToWidth(String const &text, int maxWidth)
{
    wrapTextToWidth(text, Font::RichFormat::fromPlainText(text), maxWidth);
}

void FontLineWrapping::wrapTextToWidth(String const &text, Font::RichFormat const &format, int maxWidth)
{
    String newText = text;

    clear();

    if(maxWidth <= 1 || !d->font) return;

    // This is the text that we will be wrapping.
    d->maxWidth = maxWidth;
    d->text     = newText;
    d->format   = format;

    // When tabs are used, we must first determine the maximum width of each
    // tab stop.
    if(d->containsTabs(Rangei(0, text.size())))
    {
        d->indent = 0;
        d->tabStop = 0;

        int pos = 0;
        while(pos < text.size())
        {
            Rangei const wholeLine = d->untilNextNewline(pos);
            d->lines << d->makeLine(wholeLine);
            pos = wholeLine.end + 1;
            qDebug() << "wrapped tab line:" << wholeLine.asText();
        }

        // Determine the actual positions of each tab stop according to segment widths.
        QMap<int, int> stopMaxWidths; // stop => maxWidth

        for(int i = 0; i < d->lines.size(); ++i)
        {
            Instance::Line *line = d->lines[i];
            for(int k = 0; k < line->info.segs.size(); ++k)
            {
                LineInfo::Segment const &seg = line->info.segs[k];
                stopMaxWidths[seg.tabStop] = de::max(stopMaxWidths[seg.tabStop], seg.width);
            }
        }

        // Now we can wrap the lines that area too long.
        for(int i = 0; i < d->lines.size(); ++i)
        {
            Instance::Line *line = d->lines[i];
            int lineWidth = 0;
            for(int k = 0; k < line->info.segs.size(); ++k)
            {
                lineWidth += stopMaxWidths[line->info.segs[k].tabStop];
                if(lineWidth > maxWidth)
                {
                    // Wrap the line starting from this segment.

                }
            }
        }
    }
    else
    {
        // Doesn't have tabs -- just wrap it without any extra processing.
        d->lines = d->wrapRange(Rangei(0, text.size()), maxWidth);
    }

    if(!d->lines.isEmpty())
    {
        // Mark the final line.
        d->lines.last()->line.isFinal = true;
    }

#if 1
    qDebug() << "Wrapped:" << d->text;
    foreach(Instance::Line const *ln, d->lines)
    {
        qDebug() << ln->line.range.asText() << d->text.substr(ln->line.range)
                 << "indent:" << ln->info.indent << "segments:" << ln->info.segs.size();
        foreach(LineInfo::Segment const &s, ln->info.segs)
        {
            qDebug() << "- seg" << s.range.asText() << d->text.substr(s.range)
                     << "tab:" << s.tabStop;
        }
    }
#endif
}

String const &FontLineWrapping::text() const
{
    return d->text;
}

WrappedLine FontLineWrapping::line(int index) const
{
    DENG2_ASSERT(index >= 0 && index < height());
    return d->lines[index]->line;
}

int FontLineWrapping::width() const
{
    int w = 0;
    for(int i = 0; i < d->lines.size(); ++i)
    {
        w = de::max(w, d->lines[i]->width);
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

int FontLineWrapping::maximumWidth() const
{
    return d->maxWidth;
}

Vector2i FontLineWrapping::charTopLeftInPixels(int line, int charIndex)
{    
    if(line >= height()) return Vector2i();

    WrappedLine const span = d->lines[line]->line;
    Rangei const range(span.range.start,
                       de::min(span.range.end, span.range.start + charIndex));

    Vector2i cp;
    cp.x = d->rangeAdvanceWidth(range);
    cp.y = line * d->font->lineSpacing().valuei();

    return cp;
}

FontLineWrapping::LineInfo const &FontLineWrapping::lineInfo(int index) const
{
    return d->lines[index]->info;
}

int FontLineWrapping::LineInfo::highestTabStop() const
{
    int stop = 0;
    foreach(Segment const &seg, segs)
    {
        stop = de::max(stop, seg.tabStop);
    }
    return stop;
}
