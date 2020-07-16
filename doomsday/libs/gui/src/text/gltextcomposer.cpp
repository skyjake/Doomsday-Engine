/** @file gltextcomposer.cpp
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

#include "de/gltextcomposer.h"

namespace de {

using namespace ui;

DE_PIMPL(GLTextComposer)
{
    static const Rangei MAX_VISIBLE_RANGE;

    const Font *            font  = nullptr;
    Atlas *                 atlas = nullptr;
    const FontLineWrapping *wraps = nullptr;
    Font::RichFormat        format;
    bool                    needRedo          = false; ///< Release completely and allocate.
    int                     maxGeneratedWidth = 0;
    Rangei visibleLineRange{MAX_VISIBLE_RANGE}; ///< Only these lines will be updated/drawn.
    bool   visibleLineRangeChanged = false;

    struct Line {
        struct Segment {
            Id     id;
            Vec2i  imageOrigin;
            String text;
            int    x;
            int    width;
            bool   compressed;

            Segment() : id(Id::None), x(0), width(0), compressed(false) {}
            int right() const { return x + width; }
        };
        List<Segment> segs;
    };
    List<Line> lines;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        releaseLines();
    }

    void releaseLines()
    {
        if (atlas)
        {
            for (int i = 0; i < lines.sizei(); ++i)
            {
                releaseLine(i);
            }
        }
        lines.clear();
    }

    void releaseOutsideRange()
    {
        if (!atlas) return;

        for (dsize i = 0; i < lines.size(); ++i)
        {
            if (!isLineVisible(i))
            {
                releaseLine(i, ReleaseButKeepSegs);
            }
        }
    }

    enum ReleaseBehavior { ReleaseFully, ReleaseButKeepSegs };

    void releaseLine(dsize index, ReleaseBehavior behavior = ReleaseFully)
    {
        Line &ln = lines[index];
        for (dsize i = 0; i < ln.segs.size(); ++i)
        {
            if (!ln.segs[i].id.isNone())
            {
                atlas->release(ln.segs[i].id);
                ln.segs[i].id = Id::None;
            }
        }
        if (behavior == ReleaseFully)
        {
            ln.segs.clear();
        }
    }

    bool isLineVisible(dsize line) const
    {
        return visibleLineRange.contains(int(line));
    }

    CString segmentText(int seg, const FontLineWrapping::LineInfo &info) const
    {
        return info.segs[seg].range;
    }

    bool matchingSegments(int lineIndex, const FontLineWrapping::LineInfo &info) const
    {
        if (info.segs.size() != lines[lineIndex].segs.size())
        {
//            debug("line %i number of segs changed", lineIndex);
            return false;
        }
        for (int i = 0; i < info.segs.sizei(); ++i)
        {
            if (lines[lineIndex].segs[i].text != info.segs[i].range)
            {
                // Range has changed.
//                debug("line %i seg %i range change", lineIndex, i);
                return false;
            }
            if (lines[lineIndex].segs[i].text != segmentText(i, info))
            {
                // Text has changed.
//                debug("line %i seg %i text change", lineIndex, i);
                return false;
            }
            if (lines[lineIndex].segs[i].id.isNone() && info.segs[i].range.size() > 0)
            {
                // This segment has previously failed allocation.
//                debug("line %i seg %i not alloced before, len %i [%s]", lineIndex, i,
//                      info.segs[i].range.size(), info.segs[i].range.toString().c_str());
                return false;
            }
        }
        return true;
    }

    bool allocLines()
    {
        DE_GUARD(wraps);

        bool changed = false;

        const int wrapsHeight = wraps->height();
        for (int i = 0; i < wrapsHeight; ++i)
        {
            const auto &info = wraps->lineInfo(i);

            if (i < lines.sizei())
            {
                // Is the rasterized copy up to date?
                if (matchingSegments(i, info))
                {
                    // This line can be kept as is.
                    continue;
                }

                // Needs to be redone.
                releaseLine(i);
            }

            changed = true;

            if (i >= lines.sizei())
            {
                // Need another line.
                lines << Line();
            }

            DE_ASSERT(i < lines.sizei());
            DE_ASSERT(lines[i].segs.isEmpty());

            Line &line = lines[i];
            for (int k = 0; k < info.segs.sizei(); ++k)
            {
                Line::Segment seg;
                seg.text = info.segs[k].range;
//                seg.text = segmentText(k, info);
                if (isLineVisible(i) && seg.text)
                {
                    // The color is white unless a style is defined.
                    Vec4ub fgColor(255, 255, 255, 255);

                    if (format.hasStyle())
                    {
                        fgColor = format.style().richStyleColor(Font::RichFormat::NormalColor);
                    }

                    //qDebug() << "allocating" << seg.text << seg.range.asText();

                    const Image segmentImage = wraps->rasterizedSegment(i, k);
                    seg.id = atlas->alloc(segmentImage.multiplied(fgColor));
                    seg.imageOrigin = segmentImage.origin();
                }
                line.segs << seg;
            }

            DE_ASSERT(line.segs.size() == info.segs.size());
        }

        // Remove the excess lines.
        while (lines.sizei() > wraps->height())
        {
            releaseLine(lines.sizei() - 1);
            lines.removeLast();
            changed = true;
        }

        wraps->clearRasterizedLines();

        DE_ASSERT(wraps->height() == lines.sizei());

        return changed;
    }

    void updateLineLayout(const Rangei &lineRange)
    {
        if (lineRange.isEmpty()) return;

        Rangei current = lineRange;
        for (;;)
        {
            int end = updateLineLayoutUntilUntabbed(current);
            if (end == lineRange.end)
            {
                break; // Whole range done.
            }
            current = Rangei(end, lineRange.end);
        }
    }

    /**
     * Attempts to update lines in the specified range, but stops if an
     * untabbed line is encountered. This ensures that each distinct tabbed
     * content subrange uses its own alignment.
     *
     * @param lineRange  Range of lines to update.
     *
     * @return The actual end of the updated range.
     */
    inline int updateLineLayoutUntilUntabbed(const Rangei &lineRange)
    {
        bool includesTabbedLines = false;
        int rangeEnd = lineRange.end;

        // Find the highest tab in use and initialize seg widths.
        int highestTab = 0;
        for (int i = lineRange.start; i < lineRange.end; ++i)
        {
            int lineStop = wraps->lineInfo(i).highestTabStop();
            if (lineStop >= 0)
            {
                // The range now includes at least one tabbed line.
                includesTabbedLines = true;
            }
            if (lineStop < 0)
            {
                // This is an untabbed line.
                if (!includesTabbedLines)
                {
                    // We can do many untabbed lines in the range as long as
                    // there are no tabbed ones.
                    rangeEnd = i + 1;
                }
                else
                {
                    // An untabbed line will halt the process for now.
                    rangeEnd = de::max(i, lineRange.start + 1);
                    break;
                }
            }
            highestTab = de::max(highestTab, lineStop);

            // Initialize the segments with indentation.
            for (int k = 0; k < lines[i].segs.sizei(); ++k)
            {
                lines[i].segs[k].width = wraps->lineInfo(i).segs[k].width;
            }
        }

        DE_ASSERT(rangeEnd > lineRange.start);

        // Set segment X coordinates by stacking them left-to-right on each line.
        for (int i = lineRange.start; i < rangeEnd; ++i)
        {
            if (lines[i].segs.isEmpty() || i >= visibleLineRange.end)
                continue;

            lines[i].segs[0].x = wraps->lineInfo(i).indent;

            for (int k = 1; k < lines[i].segs.sizei(); ++k)
            {
                Impl::Line::Segment &seg = lines[i].segs[k];
                seg.x = lines[i].segs[k - 1].right();
            }
        }

        // Align each tab stop with other matching stops on the other lines.
        for (int tab = 1; tab <= highestTab; ++tab)
        {
            int maxRight = 0;

            // Find the maximum right edge for this spot.
            for (int i = lineRange.start; i < rangeEnd; ++i)
            {
                if (i >= visibleLineRange.end) break;

                const FontLineWrapping::LineInfo &info = wraps->lineInfo(i);

                DE_ASSERT(info.segs.size() == lines[i].segs.size());
                for (int k = 0; k < info.segs.sizei(); ++k)
                {
                    Impl::Line::Segment &seg = lines[i].segs[k];
                    if (info.segs[k].tabStop >= 0 && info.segs[k].tabStop < tab)
                    {
                        maxRight = de::max(maxRight, seg.right());
                    }
                }
            }

            // Move the segments to this position.
            for (int i = lineRange.start; i < rangeEnd; ++i)
            {
                if (i >= visibleLineRange.end) break;

                int localRight = maxRight;

                const FontLineWrapping::LineInfo &info = wraps->lineInfo(i);
                for (int k = 0; k < info.segs.sizei(); ++k)
                {
                    if (info.segs[k].tabStop == tab)
                    {
                        lines[i].segs[k].x = localRight;
                        localRight += info.segs[k].width;
                    }
                }
            }
        }

        return rangeEnd;
    }
};

const Rangei GLTextComposer::Impl::MAX_VISIBLE_RANGE{0, 0x7fffffff};

GLTextComposer::GLTextComposer() : d(new Impl(this))
{}

void GLTextComposer::release()
{
    d->releaseLines();
    setRange(Impl::MAX_VISIBLE_RANGE);
    setState(false);
}

void GLTextComposer::releaseLinesOutsideRange()
{
    d->releaseOutsideRange();
}

void GLTextComposer::setAtlas(Atlas &atlas)
{
    d->atlas = &atlas;
}

void GLTextComposer::setWrapping(const FontLineWrapping &wrappedLines)
{
    if (d->wraps != &wrappedLines)
    {
        d->wraps = &wrappedLines;
        d->needRedo = true;
        setState(false);
    }
}

void GLTextComposer::setText(const String &text)
{
    setText(Font::RichFormat::fromPlainText(text));
}

void GLTextComposer::setStyledText(const String &styledText)
{
    d->format.clear();
    d->format.initFromStyledText(styledText);
    d->needRedo = true;
    setState(false);
}

void GLTextComposer::setText(/*const String &text, */const Font::RichFormat &format)
{
//    d->text = text;
    d->format = format;
    d->needRedo = true;
    setState(false);
}

void GLTextComposer::setRange(const Rangei &visibleLineRange)
{
    if (d->visibleLineRange != visibleLineRange)
    {
        d->visibleLineRange = visibleLineRange;
        d->visibleLineRangeChanged = true;
    }
}

Rangei GLTextComposer::range() const
{
    return d->visibleLineRange;
}

bool GLTextComposer::update()
{
    DE_ASSERT(d->wraps);

    bool changed = false;

    // If a font hasn't been defined, there isn't much to do.
    if (!d->wraps->hasFont())
    {
        return false;
    }
    if (d->font != &d->wraps->font())
    {
        d->font = &d->wraps->font();
        d->needRedo = true;
    }
    if (d->needRedo)
    {
        d->releaseLines();
        d->needRedo = false;
        changed = d->allocLines();
    }
    else if (d->visibleLineRangeChanged)
    {
        changed = d->allocLines();
        d->visibleLineRangeChanged = false;
    }
    setState(true);
    return changed;
}

void GLTextComposer::forceUpdate()
{
    d->needRedo = true;
}

void GLTextComposer::makeVertices(GuiVertexBuilder &triStrip,
                                  const Vec2i &     topLeft,
                                  const Alignment & lineAlign,
                                  const Vec4f &     color)
{
    makeVertices(triStrip, Rectanglei(topLeft, topLeft), AlignTopLeft, lineAlign, color);
}

void GLTextComposer::makeVertices(GuiVertexBuilder &triStrip,
                                  const Rectanglei &rect,
                                  const Alignment & alignInRect,
                                  const Alignment & lineAlign,
                                  const Vec4f &     color)
{
    if (!isReady()) return;

    DE_ASSERT(d->wraps != nullptr);
    DE_ASSERT(d->font != nullptr);

    Vec2i const contentSize(d->wraps->width(), d->wraps->totalHeightInPixels());

    // Apply alignment within the provided rectangle.
    Vec2f p = applyAlignment(alignInRect, contentSize, rect);

    if (d->wraps->height() != d->lines.sizei())
    {
        debug("[GLTextComposer] lines out of sync! -- allocating now");
        d->allocLines();
    }

    // Align segments based on tab stops.
    d->updateLineLayout(Rangei(0, d->lines.sizei()));

    // Compress lines to fit into the maximum allowed width.
    for (int i = 0; i < d->lines.sizei(); ++i)
    {
        Impl::Line &line = d->lines[i];
        if (!d->isLineVisible(i) || line.segs.isEmpty()) continue;

        /*
        if (!d->wraps->lineInfo(i).segs.last().tabStop)
            continue;
            */

        Impl::Line::Segment &seg = line.segs.last();
        const int leeway = 3;
        if (seg.right() > d->wraps->maximumWidth() + leeway)
        {
            // Needs compressing (up to 10%).
            seg.compressed = true;
            seg.width = de::max(int(seg.width * .9f), d->wraps->maximumWidth() + leeway - seg.x);
        }
    }

    d->maxGeneratedWidth = 0;

    // Generate vertices for each line.
    for (int i = 0; i < d->wraps->height(); ++i)
    {
        const Impl::Line &line = d->lines[i];

        if (d->isLineVisible(i))
        {
            const FontLineWrapping::LineInfo &info = d->wraps->lineInfo(i);
            Vec2f linePos = p;

            for (int k = 0; k < info.segs.sizei(); ++k)
            {
                const Impl::Line::Segment &seg = line.segs[k];

                // Empty lines are skipped.
                if (seg.id.isNone()) continue;

                Vec2ui size = d->atlas->imageRect(seg.id).size();
                if (seg.compressed)
                {
                    size.x = seg.width;
                }

                // Line alignment.
                /// @todo How to center/right-align text that uses tab stops?
                if (line.segs.size() == 1 && d->wraps->lineInfo(0).segs[0].tabStop < 0)
                {
                    if (lineAlign.testFlag(AlignRight))
                    {
                        linePos.x += int(contentSize.x) - int(size.x);
                    }
                    else if (!lineAlign.testFlag(AlignLeft))
                    {
                        linePos.x += (int(contentSize.x) - int(size.x)) / 2;
                    }
                }

                const Rectanglef uv = d->atlas->imageRectf(seg.id);

                const auto segRect =
                    Rectanglef::fromSize(linePos + Vec2f(seg.x, 0) + seg.imageOrigin, size);
                triStrip.makeQuad(segRect, color, uv);

                // Keep track of how wide the geometry really is.
                d->maxGeneratedWidth = de::max(d->maxGeneratedWidth, int(segRect.right() - p.x));
            }
        }

        p.y += d->font->lineSpacing().value();
    }
}

int GLTextComposer::verticesMaxWidth() const
{
    return d->maxGeneratedWidth;
}

} // namespace de
