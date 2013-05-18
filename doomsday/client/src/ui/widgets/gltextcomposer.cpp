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
    bool needRaster;

    typedef QList<Id> Lines;
    Lines lines;

    Instance(Public *i) : Base(i), font(0), atlas(0), wraps(0), needRaster(false)
    {}

    ~Instance()
    {
        releaseLines();
    }

    void releaseLines()
    {
        DENG2_ASSERT(atlas != 0);

        foreach(Id const &id, lines)
        {
            atlas->release(id);
        }
        lines.clear();
    }

    void allocLines()
    {
        /// @todo Could check which lines have actually changed.

        // Release the old lines.
        releaseLines();

        qDebug() << "allocLines for" << text << wraps->height();

        for(int i = 0; i < wraps->height(); ++i)
        {
            shell::WrappedLine const span = wraps->line(i);
            String part = text.substr(span.range.start, span.range.size());
            lines.append(atlas->alloc(font->rasterize(part)));

            qDebug() << lines.back().asText() << part;
        }
    }
};

GLTextComposer::GLTextComposer() : d(new Instance(this))
{}

void GLTextComposer::release()
{
    d->releaseLines();
}

void GLTextComposer::setAtlas(Atlas &atlas)
{
    d->atlas = &atlas;
}

void GLTextComposer::setText(String const &text, FontLineWrapping const &wrappedLines)
{
    d->text = text;
    d->wraps = &wrappedLines;
    d->font = &d->wraps->font();
    d->needRaster = true;
}

void GLTextComposer::update()
{
    if(d->needRaster)
    {
        d->needRaster = false;
        d->allocLines();
    }

    DENG2_ASSERT(d->wraps->height() == d->lines.size());
}

void GLTextComposer::makeVertices(Vertices &triStrip,
                                  Rectanglei const &rect,
                                  Alignment const &alignInRect,
                                  Alignment const &lineAlign)
{
    // Top left corner.
    Vector2f p = rect.topLeft;

    Vector2i const contentSize(d->wraps->width(), d->wraps->totalHeightInPixels());

    // Apply alignment within the provided rectangle.
    if(alignInRect.testFlag(AlignRight))
    {
        p.x += rect.width() - contentSize.x;
    }
    else if(!alignInRect.testFlag(AlignLeft))
    {
        p.x += (rect.width() - contentSize.x) / 2;
    }
    if(alignInRect.testFlag(AlignBottom))
    {
        p.y += rect.height() - contentSize.y;
    }
    else
    {
        p.y += (rect.height() - contentSize.y) / 2;
    }

    DENG2_ASSERT(d->wraps->height() == d->lines.size());

    // Generate vertices for each line.
    for(int i = 0; i < d->wraps->height(); ++i)
    {
        Vector2ui const size = d->atlas->imageRect(d->lines[i]).size();
        Rectanglef const uv  = d->atlas->imageRectf(d->lines[i]);

        Vertex v;
        Vertices quad;

        v.rgba = Vector4f(1, 1, 1, 1); // should be a param

        Vector2f linePos = p;

        // Align the line.
        if(lineAlign.testFlag(AlignRight))
        {
            linePos.x += rect.width() - size.x;
        }
        else if(!lineAlign.testFlag(AlignLeft))
        {
            linePos.x += (rect.width() - size.x) / 2;
        }

        v.pos = linePos;                            v.texCoord = uv.topLeft;      quad << v;
        v.pos = linePos + Vector2f(size.x, 0);      v.texCoord = uv.topRight();   quad << v;
        v.pos = linePos + Vector2f(0, size.y);      v.texCoord = uv.bottomLeft(); quad << v;
        v.pos = linePos + Vector2f(size.x, size.y); v.texCoord = uv.bottomRight;  quad << v;

        VertexBuf::concatenate(quad, triStrip);

        p.y += d->font->lineSpacing().value();
    }
}
