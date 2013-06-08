/** @file gltextcomposer.cpp
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

#include "ui/widgets/gltextcomposer.h"

#include <QList>

using namespace de;

DENG2_PIMPL(GLTextComposer)
{
    Font const *font;
    Atlas *atlas;
    String text;
    FontLineWrapping const *wraps;
    Font::RichFormat format;
    bool needRaster;

    struct Line {
        struct Segment {
            Id id;
            Rangei range;
            int x;
            int width;
            bool compressed;

            Segment() : id(Id::None), x(0), width(0), compressed(false) {}
            int right() const { return x + width; }
        };
        QList<Segment> segs;
    };
    typedef QList<Line> Lines;
    Lines lines;

    Instance(Public *i) : Base(i), font(0), atlas(0), wraps(0), needRaster(false)
    {}

    ~Instance()
    {
        releaseLines();
    }

    void releaseLines()
    {
        if(atlas)
        {
            for(int i = 0; i < lines.size(); ++i)
            {
                releaseLine(i);
            }
        }
        lines.clear();
    }

    void releaseLine(int index)
    {
        Line &ln = lines[index];
        for(int i = 0; i < ln.segs.size(); ++i)
        {
            atlas->release(ln.segs[i].id);
        }
        ln.segs.clear();
    }

    bool matchingSegments(int lineIndex, FontLineWrapping::LineInfo const &info) const
    {
        if(info.segs.size() != lines[lineIndex].segs.size())
        {
            return false;
        }
        for(int i = 0; i < info.segs.size(); ++i)
        {
            if(info.segs[i].range != lines[lineIndex].segs[i].range)
            {
                return false;
            }
        }
        return true;
    }

    bool allocLines()
    {
        bool changed = false;

        for(int i = 0; i < wraps->height(); ++i)
        {
            FontLineWrapping::LineInfo const &info = wraps->lineInfo(i);

            if(i < lines.size())
            {
                // Is the rasterized copy up to date?
                if(matchingSegments(i, info))
                {
                    // This line can be kept as is.
                    continue;
                }

                // Needs to be redone.
                releaseLine(i);
            }

            changed = true;

            if(i >= lines.size())
            {
                // Need another line.
                lines << Line();
            }

            DENG2_ASSERT(lines[i].segs.isEmpty());

            Line &line = lines[i];
            for(int k = 0; k < info.segs.size(); ++k)
            {
                Line::Segment seg;
                seg.range = info.segs[k].range;

                String const part = text.substr(seg.range);

                // The color is white unless a style is defined.
                Vector4ub fgColor(255, 255, 255, 255);

                if(format.haveStyle())
                {
                    fgColor = format.style().richStyleColor(Font::RichFormat::NormalColor);
                }

                // Set up the background color to be transparent with no
                // change of color in the alphablended smooth edges.
                Vector4ub bgColor = fgColor;
                bgColor.w = 0;

                seg.id = atlas->alloc(font->rasterize(part, format.subRange(seg.range),
                                                      fgColor, bgColor));

                line.segs << seg;
            }
        }

        // Remove the excess lines.
        while(lines.size() > wraps->height())
        {
            releaseLine(lines.size() - 1);
            lines.removeLast();
            changed = true;
        }

        DENG2_ASSERT(wraps->height() == lines.size());

        return changed;
    }
};

GLTextComposer::GLTextComposer() : d(new Instance(this))
{}

void GLTextComposer::release()
{
    d->releaseLines();
    setState(false);
}

void GLTextComposer::setAtlas(Atlas &atlas)
{
    d->atlas = &atlas;
}

void GLTextComposer::setWrapping(FontLineWrapping const &wrappedLines)
{
    d->wraps = &wrappedLines;
}

void GLTextComposer::setText(String const &text)
{
    setText(text, Font::RichFormat::fromPlainText(text));
}

void GLTextComposer::setStyledText(const String &styledText)
{
    d->format.clear();
    d->text = d->format.initFromStyledText(styledText);
    d->needRaster = true;
    setState(false);
}

void GLTextComposer::setText(String const &text, Font::RichFormat const &format)
{
    d->text = text;
    d->format = format;
    d->needRaster = true; // Force a redo of everything.
    setState(false);
}

bool GLTextComposer::update()
{
    DENG2_ASSERT(d->wraps != 0);
    if(d->font != &d->wraps->font())
    {
        d->font = &d->wraps->font();
        d->needRaster = true;
    }

    if(d->needRaster)
    {
        d->releaseLines();
        d->needRaster = false;
    }

    setState(true);

    return d->allocLines();
}

void GLTextComposer::makeVertices(Vertices &triStrip,
                                  Vector2i const &topLeft,
                                  Alignment const &lineAlign,
                                  Vector4f const &color)
{
    makeVertices(triStrip, Rectanglei(topLeft, topLeft), AlignTopLeft, lineAlign, color);
}

void GLTextComposer::makeVertices(Vertices &triStrip,
                                  Rectanglei const &rect,
                                  Alignment const &alignInRect,
                                  Alignment const &lineAlign,
                                  Vector4f const &color)
{
    if(!isReady()) return;

    DENG2_ASSERT(d->wraps != 0);
    DENG2_ASSERT(d->font != 0);

    Vector2i const contentSize(d->wraps->width(), d->wraps->totalHeightInPixels());

    // Apply alignment within the provided rectangle.
    Vector2f p = applyAlignment(alignInRect, contentSize, rect);

    DENG2_ASSERT(d->wraps->height() == d->lines.size());

    // Align segments based on tab stops.
    int highestTab = 0;
    for(int i = 0; i < d->lines.size(); ++i)
    {
        highestTab = de::max(highestTab, d->wraps->lineInfo(i).highestTabStop());

        // Initialize the segments with indentation.
        for(int k = 0; k < d->lines[i].segs.size(); ++k)
        {
            // Determine the width of this segment.
            d->lines[i].segs[k].width = d->wraps->lineInfo(i).segs[k].width;
        }
    }

    for(int i = 0; i < d->lines.size(); ++i)
    {
        d->lines[i].segs[0].x = d->wraps->lineInfo(i).indent;

        for(int k = 1; k < d->lines[i].segs.size(); ++k)
        {
            Instance::Line::Segment &seg = d->lines[i].segs[k];
            seg.x = d->lines[i].segs[k - 1].right();
        }
    }

    // Align each tab stop with other the matching stops on the other lines.
    for(int tab = 1; tab <= highestTab; ++tab)
    {
        int maxRight = 0;

        // Find the maximum right edge for this spot.
        for(int i = 0; i < d->lines.size(); ++i)
        {
            FontLineWrapping::LineInfo const &info = d->wraps->lineInfo(i);
            for(int k = 0; k < info.segs.size(); ++k)
            {
                Instance::Line::Segment &seg = d->lines[i].segs[k];
                if(info.segs[k].tabStop < tab)
                {
                    maxRight = de::max(maxRight, seg.right());
                }
            }
        }

        // Move the segments to this position.
        for(int i = 0; i < d->lines.size(); ++i)
        {
            int localRight = maxRight;

            FontLineWrapping::LineInfo const &info = d->wraps->lineInfo(i);
            for(int k = 0; k < info.segs.size(); ++k)
            {
                if(info.segs[k].tabStop == tab)
                {
                    d->lines[i].segs[k].x = localRight;
                    localRight += info.segs[k].width;
                }
            }
        }
    }

    // Compress lines to fit into the maximum allowed width.
    for(int i = 0; i < d->lines.size(); ++i)
    {
        Instance::Line &line = d->lines[i];

        if(!d->wraps->lineInfo(i).segs.last().tabStop)
            continue;

        Instance::Line::Segment &seg = line.segs.last();
        if(seg.right() > d->wraps->maximumWidth() + 1)
        {
            // Needs compressing (up to 15%).
            seg.compressed = true;
            seg.width = de::max(int(seg.width * .85f), d->wraps->maximumWidth() - seg.x);
        }
    }

    // Generate vertices for each line.
    for(int i = 0; i < d->wraps->height(); ++i)
    {
        Instance::Line const &line = d->lines[i];
        FontLineWrapping::LineInfo const &info = d->wraps->lineInfo(i);

        Vector2f linePos = p;

        for(int k = 0; k < info.segs.size(); ++k)
        {
            Instance::Line::Segment const &seg = line.segs[k];

            // Empty lines are skipped.
            if(seg.id.isNone()) continue;

            Vector2ui size = d->atlas->imageRect(seg.id).size();
            if(seg.compressed)
            {
                size.x = seg.width;
            }

            // Line alignment.
            /// @todo How to center/right-align text that uses tab stops?
            if(line.segs.size() == 1 && !d->wraps->lineInfo(0).segs[0].tabStop)
            {
                if(lineAlign.testFlag(AlignRight))
                {
                    linePos.x += int(rect.width()) - int(size.x);
                }
                else if(!lineAlign.testFlag(AlignLeft))
                {
                    linePos.x += (int(rect.width()) - int(size.x)) / 2;
                }
            }

            Rectanglef const uv = d->atlas->imageRectf(seg.id);

            triStrip.makeQuad(Rectanglef::fromSize(linePos + Vector2f(seg.x, 0), size),
                              color, uv);
        }

        p.y += d->font->lineSpacing().value();
    }
}
