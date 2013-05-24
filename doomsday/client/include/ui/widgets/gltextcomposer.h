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

#include "fontlinewrapping.h"

/**
 * Manages lines of text on an atlas and produces geometry for drawing the
 * text.
 */
class GLTextComposer
{
public:
    typedef de::Vertex2TexRgba    Vertex;
    typedef de::GLBufferT<Vertex> VertexBuf;
    typedef VertexBuf::Builder    Vertices;

    /**
     * Flags for specifying alignment.
     */
    enum AlignmentFlag
    {
        AlignTop    = 0x1,
        AlignBottom = 0x2,
        AlignLeft   = 0x4,
        AlignRight  = 0x8,

        AlignCenter = 0,

        DefaultAlignment = AlignCenter
    };
    Q_DECLARE_FLAGS(Alignment, AlignmentFlag)

public:
    GLTextComposer();

    void setAtlas(de::Atlas &atlas);
    void setWrapping(FontLineWrapping const &wrappedLines);

    void setText(de::String const &text);
    void setText(de::String const &text, de::Font::RichFormat const &format);

    /**
     * Makes sure all the lines are allocated on the atlas. After this all the
     * allocated lines match the ones in the wrapping.
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
                      Alignment const &lineAlign);

    /**
     * Generates vertices for all the text lines and concatenates them
     * onto the existing triangle strip in @a triStrip.
     *
     * @param triStrip  Vertices for a triangle strip.
     */
    void makeVertices(Vertices &triStrip,
                      de::Rectanglei const &rect,
                      Alignment const &alignInRect,
                      Alignment const &lineAlign);

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GLTextComposer::Alignment)

#endif // DENG_CLIENT_GLTEXTCOMPOSER_H
