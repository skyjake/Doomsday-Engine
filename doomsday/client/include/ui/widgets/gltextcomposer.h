/** @file gltextcomposer.h
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

#ifndef DENG_CLIENT_GLTEXTCOMPOSER_H
#define DENG_CLIENT_GLTEXTCOMPOSER_H

#include <QVector>
#include <de/Font>
#include <de/Atlas>
#include <de/GLBuffer>

#include "alignment.h"
#include "fontlinewrapping.h"

/**
 * Allocates and releases lines of text on an atlas and produces geometry for
 * drawing the text.
 *
 * Relies on a pre-existing FontLineWrapping where the text content has been
 * wrapped onto multiple lines and laid out appropriately.
 */
class GLTextComposer : public de::Asset
{
public:
    typedef de::Vertex2TexRgba    Vertex;
    typedef de::GLBufferT<Vertex> VertexBuf;
    typedef VertexBuf::Builder    Vertices;

public:
    GLTextComposer();

    void setAtlas(de::Atlas &atlas);
    void setWrapping(FontLineWrapping const &wrappedLines);

    void setText(de::String const &text);
    void setStyledText(de::String const &styledText);
    void setText(de::String const &text, de::Font::RichFormat const &format);

    /**
     * Makes sure all the lines are allocated on the atlas. After this all the
     * allocated lines match the ones in the wrapping. This must be called
     * before makeVertices().
     *
     * @return @c true, if any allocations were changed and makeVertices()
     * should be called again.
     */
    bool update();

    /**
     * Releases all the allocations from the atlas.
     */
    void release();

    void makeVertices(Vertices &triStrip,
                      de::Vector2i const &topLeft,
                      ui::Alignment const &lineAlign,
                      de::Vector4f const &color = de::Vector4f(1, 1, 1, 1));

    /**
     * Generates vertices for all the text lines and concatenates them
     * onto the existing triangle strip in @a triStrip.
     *
     * @param triStrip  Vertices for a triangle strip.
     */
    void makeVertices(Vertices &triStrip,
                      de::Rectanglei const &rect,
                      ui::Alignment const &alignInRect,
                      ui::Alignment const &lineAlign,
                      de::Vector4f const &color = de::Vector4f(1, 1, 1, 1));

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_GLTEXTCOMPOSER_H
