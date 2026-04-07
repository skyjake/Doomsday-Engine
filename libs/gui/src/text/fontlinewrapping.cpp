/** @file fontlinewrapping.cpp  Font line wrapping.
 *
 * @todo Performance|Refactor: Add a class dedicated for measuring text. Allow
 * measuring in increments, one character at a time, without re-measuring the
 * whole range. Allow seeking forward and backward with the measurer.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/fontlinewrapping.h"
#include "de/baseguiapp.h"
#include <de/image.h>

#include <de/keymap.h>
#include <atomic>

namespace de {

static constexpr Char NEWLINE('\n');

DE_PIMPL_NOREF(FontLineWrapping)
{
    /**
     * A wrapped line of text.
     */
    struct Line
    {
        WrappedLine line;
        LineInfo    info;

        Line(const WrappedLine &ln = {CString(), 0}, int leftIndent = 0)
            : line(ln)
        {
            info.indent = leftIndent;
        }

        /// Tab stops are disabled if there is a tab stop < 0 anywhere on the line.
        bool tabsDisabled() const
        {
            for (int i = 0; i < info.segs.sizei(); ++i)
            {
                if (info.segs[i].tabStop < 0) return true;
            }
            return false;
        }
    };
    typedef List<Line *> Lines;
    struct RasterizedLine
    {
        List<Image> segmentImages;
    };

    Lines                lines;
    List<RasterizedLine> rasterized;

    const Font *     font     = nullptr;
    int              maxWidth = 0;
    String           text; ///< Plain text.
    Font::RichFormat format;
    int              indent = 0; ///< Current left indentation (in pixels).
    List<int>        prevIndents;
    int              tabStop = -1;
    std::atomic_bool cancelled{false};

    DE_ERROR(CancelError);

    ~Impl()
    {
        clearLines();
    }

    inline void checkCancel() const
    {
        if (cancelled) throw CancelError("FontLineWrapping::checkCancel", "Cancelled");
    }

    void clearLines()
    {
        deleteAll(lines);
        lines.clear();
        rasterized.clear();
    }

    int rangeVisibleWidth(const CString &range) const
    {
        if (font)
        {
            return font->measure(format.subRange(range)).width();
        }
        return 0;
    }

    WrapWidth rangeAdvanceWidth(const CString &range) const
    {
        checkCancel();
        if (font)
        {
            return font->advanceWidth(format.subRange(range));
        }
        return 0;
    }

    void updateIndentMarkWidth(const CString &range)
    {
        Font::RichFormatRef rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);
        const int origIndent = indent;
        while (iter.hasNext())
        {
            iter.next();
            if (iter.markIndent())
            {
                prevIndents.append(indent);
                indent = origIndent + rangeAdvanceWidth({range.ptr(), iter.range().ptr()});
                    //Rangei(0, iter.range().start) + range.start);
            }
            if (iter.resetIndent())
            {
                if (!prevIndents.isEmpty())
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
    Line *makeLine(const CString &range, int width = -1)
    {
        checkCancel();

        if (width < 0)
        {
            // Determine the full width now.
            width = rangeVisibleWidth(range);
        }

        Line *line = new Line(WrappedLine(range, width), indent);

        // Determine segments in the line.
        const char *pos = range.begin();

        Font::RichFormatRef rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);

        // Divide the line into segments based on tab stops.
        while (iter.hasNext())
        {
            iter.next();
            if (iter.tabStop() != tabStop)
            {
                const char *start = iter.range().begin();
                if (start > pos)
                {
                    line->info.segs << LineInfo::Segment({pos, start}, tabStop);
                    pos = start;
                }
                tabStop = iter.tabStop();
            }
        }

        // The final segment.
        line->info.segs << LineInfo::Segment({pos, range.end()}, tabStop);

        // Determine segment widths.
        if (line->info.segs.size() == 1)
        {
            line->info.segs[0].width = width;
        }
        else
        {
            for (int i = 0; i < line->info.segs.sizei(); ++i)
            {
                line->info.segs[i].width = rangeAdvanceWidth(line->info.segs[i].range);
            }
        }

        // Check for possible indent for following lines.
        updateIndentMarkWidth(range);

        return line;
    }

    bool isAllSpace(const CString &range) const
    {
        for (auto i = range.begin(); i != range.end(); ++i)
        {
            if (!(*i).isSpace()) return false;
        }
        return true;
    }

    bool containsNewline(const CString &range) const
    {
        return range.contains('\n');
    }

    bool containsTabs(const CString &range) const
    {
        Font::RichFormatRef rich = format.subRange(range);
        Font::RichFormat::Iterator iter(rich);
        while (iter.hasNext())
        {
            iter.next();
            if (iter.tabStop() > 0) return true;
        }
        return false;
    }

    mb_iterator findMaxWrap(mb_iterator begin, int availableWidth) const
    {
        int width = 0;
        mb_iterator end = begin;
        while (end != text.end() && *end != NEWLINE)
        {
            const int charWidth = rangeAdvanceWidth({end, end + 1});
            if (width + charWidth > availableWidth)
            {
                // Does not fit any more.
                break;
            }
            width += charWidth;
            ++end;
        }
        // Fine-tune the result to be accurate (kerning is ignored and rouding errors
        // affect the end result when checking width character by character).
        while (end > begin && int(rangeAdvanceWidth({begin, end})) > availableWidth)
        {
            // Came out too long.
            --end;
        }
        return end;
    }

    bool isWrappable(mb_iterator at)
    {
        if (at == text.end()) return true;
        if ((*at).isSpace()) return true;
        if (at > text.begin())
        {
            Char prev = *(at - 1);
            if (prev == '/' || prev == '\\') return true;
        }
        return false;
    }

    CString untilNextNewline(mb_iterator start) const
    {
        mb_iterator pos = start;
        while (pos != text.end())
        {
            // The newline is omitted from the range.
            if (*pos == NEWLINE) break;

            ++pos;
        }
        return {start, pos};
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
    Lines wrapRange(const CString &rangeToWrap, int maxWidth, int subsequentMaxWidth = 0,
                    int initialIndent = 0)
    {
        const int  MIN_LINE_WIDTH = roundi(150.f * DE_BASE_GUI_APP->pixelRatio().value());
        const bool isTabbed       = (subsequentMaxWidth > 0);

        indent    = initialIndent;
        tabStop   = (isTabbed ? 0 : -1);

        mb_iterator begin = rangeToWrap.begin();

        Lines wrappedLines;
        while (begin != rangeToWrap.end())
        {
            checkCancel();

            int mw = maxWidth;
            if (!wrappedLines.isEmpty() && subsequentMaxWidth > 0) { mw = subsequentMaxWidth; }

            // How much width is available, taking indentation into account?
            if (mw - indent < MIN_LINE_WIDTH)
            {
                if (!isTabbed)
                {
                    // Regular non-tabbed line -- there is no room for this indent,
                    // fall back to the previous one.
                    if (prevIndents.isEmpty())
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
            const CString range(begin, rangeToWrap.end());

            // Quick check: does the complete remainder fit?
            if (!containsNewline(range))
            {
                int visWidth = rangeAdvanceWidth(range);
                if (visWidth <= availWidth)
                {
                    wrappedLines << makeLine(range, visWidth);
                    break;
                }
            }

            // Newlines always cause a wrap.
            mb_iterator end = findMaxWrap(begin, availWidth);
            mb_iterator wrapPosMax = end;

            if (end != rangeToWrap.end() && *end == NEWLINE)
            {
                // The newline will be omitted from the wrapped lines.
                wrappedLines << makeLine({begin, end});
                begin = end + 1;
            }
            else
            {
                if (end <= begin) break;

                // Rewind to find a good (whitespace) break point.
                while (!isWrappable(end))
                {
                    if (--end == begin)
                    {
                        // Ran out of non-space chars, force a break.
                        end = wrapPosMax;
                        break;
                    }
                }

                DE_ASSERT(end > begin);

                // If there is only whitespace remaining on the line,
                // just use the max wrap -- blank lines are not pretty.
                if (isAllSpace({begin, end}))
                {
                    end = wrapPosMax;
                }

                while (end != rangeToWrap.end() && (*end).isSpace()) { ++end; }
                wrappedLines << makeLine({begin, end});
                begin = end;
            }
        }

        return wrappedLines;
    }

    Rangei findNextTabbedRange(int startLine) const
    {
        for (int i = startLine + 1; i < lines.sizei(); ++i)
        {
            if (lines[i]->tabsDisabled()) return Rangei(startLine, i);
        }
        return Rangei(startLine, lines.sizei());
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
    int wrapLinesWithTabs(const Rangei &lineRange)
    {
        int extraLinesProduced = 0;

        // Determine the actual positions of each tab stop according to segment widths.
        KeyMap<int, int> stopMaxWidths; // stop => maxWidth

        for (int i = lineRange.start; i < lineRange.end; ++i)
        {
            Line *line = lines[i];
            for (int k = 0; k < line->info.segs.sizei(); ++k)
            {
                const LineInfo::Segment &seg = line->info.segs[k];
                if (seg.tabStop < 0) continue;
                int sw = seg.width;

                // Include overall indent into the first segment width.
                if (!k) sw += line->info.indent;

                stopMaxWidths[seg.tabStop] = de::max(stopMaxWidths[seg.tabStop], sw);
            }
        }

        // Now we can wrap the lines that area too long.
        for (int i = lineRange.start; i < lineRange.end + extraLinesProduced; ++i)
        {
            Line *line = lines[i];
            int curLeft = 0;
            int prevRight = 0;

            for (int k = 0; k < line->info.segs.sizei(); ++k)
            {
                const LineInfo::Segment &seg = line->info.segs[k];
                const int tab = seg.tabStop;
                const int stopWidth = (tab >= 0? stopMaxWidths[tab] : seg.width);

                if (curLeft + stopWidth >= maxWidth)
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
                    for (Line *wl : wrapped)
                    {
                        lines.insert(i++, wl);
                    }
                    --i;
                    break; // Proceed to next line.
                }

                // Update the coordinate of the previous segment's right edge.
                prevRight = curLeft + seg.width;
                if (!k) prevRight += line->info.indent;

                // Move on to the next segment's left edge.
                curLeft += stopWidth;
            }
        }

        return lineRange.end + extraLinesProduced;
    }

    Image rasterizeSegment(const LineInfo::Segment &segment)
    {
        return font->rasterize(format.subRange(segment.range));
    }
};

FontLineWrapping::FontLineWrapping() : d(new Impl)
{}

void FontLineWrapping::setFont(const Font &font)
{
    DE_GUARD(this);
    d->font = &font;
}

const Font &FontLineWrapping::font() const
{
    DE_GUARD(this);
    DE_ASSERT(hasFont());
    return *d->font;
}

bool FontLineWrapping::hasFont() const
{
    return d->font != nullptr;
}

bool FontLineWrapping::isEmpty() const
{
    DE_GUARD(this);
    return d->lines.isEmpty();
}

void FontLineWrapping::clear()
{
    DE_GUARD(this);
    reset();
    d->text.clear();
}

void FontLineWrapping::reset()
{
    DE_GUARD(this);
    d->clearLines();
    d->indent = 0;
    d->prevIndents.clear();
    d->tabStop = -1;
    d->cancelled = false;
}

void FontLineWrapping::wrapTextToWidth(const String &text, WrapWidth maxWidth)
{
    wrapTextToWidth(text, Font::RichFormat::fromPlainText(text), maxWidth);
}

void FontLineWrapping::wrapTextToWidth(const String &text, const Font::RichFormat &format, WrapWidth maxWidth)
{
    DE_GUARD(this);

    clear();

    if (maxWidth <= 1 || !d->font) return;

    // This is the text that we will be wrapping.
    d->maxWidth = maxWidth;
    d->text     = text;
    d->format   = format;

#if defined (DE_DEBUG)
    if (text)
    {
        // Ensure the format refers to the correct string.
        Font::RichFormat::Iterator iter(format);
        while (iter.hasNext())
        {
            iter.next();
            DE_ASSERT(iter.range().ptr() >= text.data());
            DE_ASSERT(iter.range().ptr() <= text.data() + text.size());
            DE_ASSERT(iter.range().endPtr() >= text.data());
            DE_ASSERT(iter.range().endPtr() <= text.data() + text.size());
        }
    }
#endif

    // When tabs are used, we must first determine the maximum width of each tab stop.
    if (d->containsTabs(text))
    {
        d->indent  = 0;
        d->tabStop = 0;

        // Divide the content into lines by newlines.
        mb_iterator pos = text.begin();
        while (pos != text.end())
        {
            const CString wholeLine = d->untilNextNewline(pos);
            d->lines << d->makeLine(wholeLine);
            pos = wholeLine.end() + 1;
        }

        // Process the content in distinct ranges divided by untabbed content.
        Rangei tabRange = d->findNextTabbedRange(0);
        for (;;)
        {
            int end = d->wrapLinesWithTabs(tabRange);
            if (end == d->lines.sizei())
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
        d->lines = d->wrapRange(text, maxWidth);
    }

    if (d->lines.isEmpty())
    {
        // Make sure at least one blank line exists.
       d->lines << new Impl::Line;
    }

    // Mark the final line.
    d->lines.last()->line.isFinal = true;

#if 0
    debug("Wrapped: %s", d->text.c_str());
    for (const Impl::Line *ln : d->lines)
    {
        debug("  range:[%s](%zu) indent:%i #segments:%i",
              ln->line.range.toString().c_str(),
              ln->line.range.size(),
              ln->info.indent,
              ln->info.segs.size());
        for (const LineInfo::Segment &s : ln->info.segs)
        {
            debug("  - seg [%s](%zu) tab:%i width:%i",
                  s.range.toString().c_str(),
                  s.range.size(),
                  s.tabStop,
                  s.width);
        }
    }
#endif
}

void FontLineWrapping::cancel()
{
    d->cancelled = true;
}

const String &FontLineWrapping::text() const
{
    DE_GUARD(this);

    return d->text;
}

WrappedLine FontLineWrapping::line(int index) const
{
    DE_GUARD(this);

    DE_ASSERT(index >= 0 && index < height());
    return d->lines[index]->line;
}

WrapWidth FontLineWrapping::width() const
{
    DE_GUARD(this);

    WrapWidth w = 0;
    for (dsize i = 0; i < d->lines.size(); ++i)
    {
        w = de::max(w, d->lines.at(i)->line.width);
    }
    return w;
}

int FontLineWrapping::height() const
{
    DE_GUARD(this);

    return d->lines.sizei();
}

WrapWidth FontLineWrapping::rangeWidth(const CString &range) const
{
    DE_GUARD(this);
    return d->rangeAdvanceWidth(range);
}

BytePos FontLineWrapping::indexAtWidth(const CString &range, WrapWidth width) const
{
    DE_GUARD(this);

    WrapWidth prevWidth = 0;

    for (mb_iterator i = range.begin(); i != range.end(); ++i)
    {
        const auto rw = d->rangeAdvanceWidth(CString(range.begin(), i));
        if (rw >= width)
        {
            // Which is closer, this or the previous char?
            if (rw - width <= WrapWidth(abs(int(prevWidth) - int(width))))
            {
                return i.pos(d->text);
            }
            return (i - 1).pos(d->text);
        }
        prevWidth = rw;
    }
    return range.end().pos(d->text);
}

int FontLineWrapping::totalHeightInPixels() const
{
    DE_GUARD(this);

    if (!d->font) return 0;

    const int lines = height();
    int pixels = 0;

    if (lines > 1)
    {
        // Full baseline-to-baseline spacing.
        pixels += (lines - 1) * d->font->lineSpacing().value();
    }
    if (lines > 0)
    {
        // The last (or a single) line is just one 'font height' tall.
        pixels += d->font->height().value();
    }
    return pixels;
}

int FontLineWrapping::maximumWidth() const
{
    DE_GUARD(this);

    return d->maxWidth;
}

Vec2i FontLineWrapping::charTopLeftInPixels(int line, BytePos charIndex)
{
    DE_GUARD(this);

    if (line >= height()) return Vec2i();

    const auto span = d->lines[line]->line;
    //    Rangei const range(span.range.start,
    //                       de::min(span.range.end, span.range.start + charIndex));
    const CString range(span.range.begin(), span.range.ptr(charIndex));

    Vec2i cp;
    cp.x = d->rangeAdvanceWidth(range);
    cp.y = line * d->font->lineSpacing().valuei();

    return cp;
}

const FontLineWrapping::LineInfo &FontLineWrapping::lineInfo(int index) const
{
    DE_GUARD(this);
    DE_ASSERT(index >= 0 && index < d->lines.sizei());
    return d->lines[index]->info;
}

void FontLineWrapping::rasterizeLines(const Rangei &lineRange)
{
    DE_GUARD(this);

    d->rasterized.clear();

    for (int i = 0; i < height(); ++i)
    {
        Impl::RasterizedLine rasterLine;
        if (lineRange.contains(i))
        {
            const LineInfo &line = lineInfo(i);
            for (int k = 0; k < line.segs.sizei(); ++k)
            {
                rasterLine.segmentImages << d->rasterizeSegment(line.segs.at(k));
            }
        }
        d->rasterized << rasterLine;
    }
}

void FontLineWrapping::clearRasterizedLines() const
{
    DE_GUARD(this);
    d->rasterized.clear();
}

Image FontLineWrapping::rasterizedSegment(int line, int segment) const
{
    // Check the cached images.
    {
        DE_GUARD(this);
        DE_ASSERT(line >= 0);
        if (line >= 0 && line < d->rasterized.sizei())
        {
            const auto &rasterLine = d->rasterized.at(line);
            if (!rasterLine.segmentImages.isEmpty())
            {
                DE_ASSERT(segment >= 0 && segment < rasterLine.segmentImages.sizei());
                return rasterLine.segmentImages.at(segment);
            }
        }
    }
    // Rasterize now, since it wasn't previously rasterized.
    return d->rasterizeSegment(lineInfo(line).segs.at(segment));
}

//---------------------------------------------------------------------------------------

int FontLineWrapping::LineInfo::highestTabStop() const
{
    int stop = -1;
    for (const Segment &seg : segs)
    {
        stop = de::max(stop, seg.tabStop);
    }
    return stop;
}

} // namespace de
