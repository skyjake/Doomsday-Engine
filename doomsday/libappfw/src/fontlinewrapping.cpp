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

#include "de/FontLineWrapping"

#include <QMap>

namespace de {

using namespace shell;

static QChar const NEWLINE('\n');

DENG2_PIMPL_NOREF(FontLineWrapping)
{
    Font const *font;

    /**
     * A wrapped line of text.
     */
    struct Line
    {
        WrappedLine line;
        LineInfo info;
        int width; ///< Total width of the line (in pixels).

        Line(WrappedLine const &ln = WrappedLine(Rangei()), int lineWidth = 0, int leftIndent = 0)
            : line(ln), width(lineWidth)
        {
            info.indent = leftIndent;
        }

        /// Tab stops are disabled if there is a tab stop < 0 anywhere on the line.
        bool tabsDisabled() const
        {
            for(int i = 0; i < info.segs.size(); ++i)
            {
                if(info.segs[i].tabStop < 0) return true;
            }
            return false;
        }
    };

    typedef QList<Line *> Lines;
    Lines lines;

    int maxWidth;
    String text;                ///< Plain text.
    Font::RichFormat format;
    int indent;                 ///< Current left indentation (in pixels).
    QList<int> prevIndents;
    int tabStop;

    Instance() : font(0), maxWidth(0), indent(0), tabStop(0) {}

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

    void updateIndentMarkWidth(Rangei const &range)
    {
        Font::RichFormatRef rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);
        int const origIndent = indent;
        while(iter.hasNext())
        {
            iter.next();
            if(iter.markIndent())
            {
                prevIndents.append(indent);
                indent = origIndent + rangeAdvanceWidth(Rangei(0, iter.range().start) + range.start);
            }

            if(iter.resetIndent())
            {
                if(!prevIndents.isEmpty())
                {
                    indent = prevIndents.takeLast();
                }
                else
                {
                    indent = 0;
                }
            }
        }
    }

    /**
     * Constructs a wrapped line. Note that indent and tabStop are modified;
     * this is expected to be called in the right order as lines are being
     * processed.
     *
     * @param range   Range in the content for the line.
     * @param width   Width of the line in pixel. If -1, will be calculated.
     *
     * @return Line instance. Caller gets ownership.
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

        Font::RichFormatRef rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);

        // Divide the line into segments based on tab stops.
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
                line->info.segs[i].width = rangeAdvanceWidth(line->info.segs[i].range);
            }
        }

        // Check for possible indent for following lines.
        updateIndentMarkWidth(range);

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
        Font::RichFormatRef rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);
        while(iter.hasNext())
        {
            iter.next();
            if(iter.tabStop() > 0) return true;
        }
        return false;
    }

    int findMaxWrap(int begin, int availableWidth) const
    {
        int width = 0;
        int end = begin;
        while(end < text.size() && text.at(end) != NEWLINE)
        {
            int const charWidth = rangeAdvanceWidth(Rangei(end, end + 1));
            if(width + charWidth > availableWidth)
            {
                // Does not fit any more.
                break;
            }
            width += charWidth;
            ++end;
        }
        // Fine-tune the result to be accurate (kerning is ignored and rouding errors
        // affect the end result when checking width character by character).
        while(end > begin && rangeAdvanceWidth(Rangei(begin, end)) > availableWidth)
        {
            // Came out too long.
            end--;
        }
        return end;
    }

    bool isWrappable(int at)
    {
        if(at >= text.size()) return true;
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
     * @param rangeToWrap         Range in the content string.
     * @param maxWidth            Maximum width of a line.
     * @param subsequentMaxWidth  Maximum width of lines beyond the first one.
     *                            Note: if larger than zero, the line is considered
     *                            to contain tabbed segments.
     * @param initialIndent       Initial value for the indent.
     *
     * @return The produced wrapped lines. Caller gets ownership.
     */
    Lines wrapRange(Rangei const &rangeToWrap, int maxWidth, int subsequentMaxWidth = 0,
                    int initialIndent = 0)
    {
        int const MIN_LINE_WIDTH = 150;
        bool const isTabbed = (subsequentMaxWidth > 0);

        indent    = initialIndent;
        tabStop   = 0;
        int begin = rangeToWrap.start;

        Lines wrappedLines;
        while(begin < rangeToWrap.end)
        {
            int mw = maxWidth;
            if(!wrappedLines.isEmpty() && subsequentMaxWidth > 0) mw = subsequentMaxWidth;

            // How much width is available, taking indentation into account?
            if(mw - indent < MIN_LINE_WIDTH)
            {
                if(!isTabbed)
                {
                    // Regular non-tabbed line -- there is no room for this indent,
                    // fall back to the previous one.
                    if(prevIndents.isEmpty())
                    {
                        indent = 0;
                    }
                    else
                    {
                        indent = prevIndents.last();
                    }
                }
                else
                {
                    // We can't alter indentation with tabs, so just extend the line instead.
                    mw = MIN_LINE_WIDTH + indent;
                }
            }
            int availWidth = mw - indent;

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
#if 0
            int wrapPosMax;
            int end = findMaxWrap(begin, availWidth, wrapPosMax);
#else
            int end = findMaxWrap(begin, availWidth);
            int wrapPosMax = end;
#endif

            if(end < rangeToWrap.end && text.at(end) == NEWLINE)
            {
                // The newline will be omitted from the wrapped lines.
                wrappedLines << makeLine(Rangei(begin, end));
                begin = end + 1;
            }
            else
            {
                if(end <= begin) break;

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

                while(end < rangeToWrap.end && text.at(end).isSpace()) ++end;
                wrappedLines << makeLine(Rangei(begin, end));
                begin = end;
            }
        }

        return wrappedLines;
    }

    Rangei findNextTabbedRange(int startLine) const
    {
        for(int i = startLine + 1; i < lines.size(); ++i)
        {
            if(lines[i]->tabsDisabled()) return Rangei(startLine, i);
        }
        return Rangei(startLine, lines.size());
    }

    /**
     * Wraps a range of lines that contains tab stops. Wrapping takes into
     * account the space available for each tab stop.
     *
     * @param lineRange  Range of lines to wrap.
     *
     * @return End of the range, taking into account possible extra lines produced
     * when wrapping long lines.
     */
    int wrapLinesWithTabs(Rangei const &lineRange)
    {
        int extraLinesProduced = 0;

        // Determine the actual positions of each tab stop according to segment widths.
        QMap<int, int> stopMaxWidths; // stop => maxWidth

        for(int i = lineRange.start; i < lineRange.end; ++i)
        {
            Line *line = lines[i];
            for(int k = 0; k < line->info.segs.size(); ++k)
            {
                LineInfo::Segment const &seg = line->info.segs[k];
                if(seg.tabStop < 0) continue;
                int sw = seg.width;

                // Include overall indent into the first segment width.
                if(!k) sw += line->info.indent;

                stopMaxWidths[seg.tabStop] = de::max(stopMaxWidths[seg.tabStop], sw);
            }
        }

        // Now we can wrap the lines that area too long.
        for(int i = lineRange.start; i < lineRange.end + extraLinesProduced; ++i)
        {
            Line *line = lines[i];
            int curLeft = 0;
            int prevRight = 0;

            for(int k = 0; k < line->info.segs.size(); ++k)
            {
                LineInfo::Segment const &seg = line->info.segs[k];
                int const tab = seg.tabStop;
                int const stopWidth = (tab >= 0? stopMaxWidths[tab] : seg.width);

                if(curLeft + stopWidth >= maxWidth)
                {
                    // Wrap the line starting from this segment.

                    // The maximum width of the first line is reduced by the
                    // added amount of tab space: the difference between the
                    // left edge of the current segment and the right edge of
                    // the previous one. The maximum widths of subsequent lines
                    // is also adjusted, so that the available space depends on
                    // where the current tab is located (indent is added
                    // because wrapRange automatically subtracts it).

                    Lines wrapped = wrapRange(line->line.range,
                                              maxWidth - (curLeft - prevRight),
                                              maxWidth - curLeft + line->info.indent,
                                              line->info.indent);

                    extraLinesProduced += wrapped.size() - 1;

                    // Replace the original line with these wrapped lines.
                    delete lines.takeAt(i);
                    foreach(Line *wl, wrapped)
                    {
                        lines.insert(i++, wl);
                    }
                    --i;
                    break; // Proceed to next line.
                }

                // Update the coordinate of the previous segment's right edge.
                prevRight = curLeft + seg.width;
                if(!k) prevRight += line->info.indent;

                // Move on to the next segment's left edge.
                curLeft += stopWidth;
            }
        }

        return lineRange.end + extraLinesProduced;
    }
};

FontLineWrapping::FontLineWrapping() : d(new Instance)
{}

void FontLineWrapping::setFont(Font const &font)
{
    DENG2_GUARD(this);

    d->font = &font;
}

Font const &FontLineWrapping::font() const
{
    DENG2_GUARD(this);

    DENG2_ASSERT(hasFont());
    return *d->font;
}

bool FontLineWrapping::hasFont() const
{
    return d->font != 0;
}

bool FontLineWrapping::isEmpty() const
{
    DENG2_GUARD(this);

    return d->lines.isEmpty();
}

void FontLineWrapping::clear()
{
    DENG2_GUARD(this);

    reset();
    d->text.clear();
}

void FontLineWrapping::reset()
{
    DENG2_GUARD(this);

    d->clearLines();
    d->indent = 0;
    d->prevIndents.clear();
    d->tabStop = 0;
}

void FontLineWrapping::wrapTextToWidth(String const &text, int maxWidth)
{
    wrapTextToWidth(text, Font::RichFormat::fromPlainText(text), maxWidth);
}

void FontLineWrapping::wrapTextToWidth(String const &text, Font::RichFormat const &format, int maxWidth)
{
    DENG2_GUARD(this);

    String newText = text;

    clear();

    if(maxWidth <= 1 || !d->font) return;

    // This is the text that we will be wrapping.
    d->maxWidth = maxWidth;
    d->text     = newText;
    d->format   = format;

    // When tabs are used, we must first determine the maximum width of each tab stop.
    if(d->containsTabs(Rangei(0, text.size())))
    {
        d->indent = 0;
        d->tabStop = 0;

        // Divide the content into lines by newlines.
        int pos = 0;
        while(pos < text.size())
        {
            Rangei const wholeLine = d->untilNextNewline(pos);
            d->lines << d->makeLine(wholeLine);
            pos = wholeLine.end + 1;
        }

        // Process the content is distinct ranges divided by untabbed content.
        Rangei tabRange = d->findNextTabbedRange(0);
        forever
        {
            int end = d->wrapLinesWithTabs(tabRange);
            if(end == d->lines.size())
            {
                // All lines processed.
                break;
            }
            tabRange = d->findNextTabbedRange(end);
        }
    }
    else
    {
        // Doesn't have tabs -- just wrap it without any extra processing.
        d->lines = d->wrapRange(Rangei(0, text.size()), maxWidth);
    }

    if(d->lines.isEmpty())
    {
        // Make sure at least one blank line exists.
       d->lines << new Instance::Line;
    }

    // Mark the final line.
    d->lines.last()->line.isFinal = true;

#if 0
    qDebug() << "Wrapped:" << d->text;
    foreach(Instance::Line const *ln, d->lines)
    {
        qDebug() << ln->line.range.asText() << d->text.substr(ln->line.range)
                 << "indent:" << ln->info.indent << "segments:" << ln->info.segs.size();
        foreach(LineInfo::Segment const &s, ln->info.segs)
        {
            qDebug() << "- seg" << s.range.asText() << d->text.substr(s.range)
                     << "tab:" << s.tabStop << "w:" << s.width;
        }
    }
#endif
}

String const &FontLineWrapping::text() const
{
    DENG2_GUARD(this);

    return d->text;
}

WrappedLine FontLineWrapping::line(int index) const
{
    DENG2_GUARD(this);

    DENG2_ASSERT(index >= 0 && index < height());
    return d->lines[index]->line;
}

int FontLineWrapping::width() const
{
    DENG2_GUARD(this);

    int w = 0;
    for(int i = 0; i < d->lines.size(); ++i)
    {
        w = de::max(w, d->lines[i]->width);
    }
    return w;
}

int FontLineWrapping::height() const
{
    DENG2_GUARD(this);

    return d->lines.size();
}

int FontLineWrapping::rangeWidth(Rangei const &range) const
{
    DENG2_GUARD(this);

    return d->rangeAdvanceWidth(range);
}

int FontLineWrapping::indexAtWidth(Rangei const &range, int width) const
{
    DENG2_GUARD(this);

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
    DENG2_GUARD(this);

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
    DENG2_GUARD(this);

    return d->maxWidth;
}

Vector2i FontLineWrapping::charTopLeftInPixels(int line, int charIndex)
{    
    DENG2_GUARD(this);

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
    int stop = -1;
    foreach(Segment const &seg, segs)
    {
        stop = de::max(stop, seg.tabStop);
    }
    return stop;
}

} // namespace de
