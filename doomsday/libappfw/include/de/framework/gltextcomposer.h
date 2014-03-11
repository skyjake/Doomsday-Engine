/** @file gltextcomposer.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_GLTEXTCOMPOSER_H
#define LIBAPPFW_GLTEXTCOMPOSER_H

#include <QVector>
#include <de/Font>
#include <de/Atlas>
#include <de/GLBuffer>

#include "../ui/defs.h"
#include "../FontLineWrapping"

namespace de {

/**
 * Allocates and releases lines of text on an atlas and produces geometry for
 * drawing the text.
 *
 * Relies on a pre-existing FontLineWrapping where the text content has been
 * wrapped onto multiple lines and laid out appropriately.
 */
class LIBAPPFW_PUBLIC GLTextComposer : public Asset
{
public:
    typedef Vertex2TexRgba      Vertex;
    typedef GLBufferT<Vertex>   VertexBuf;
    typedef VertexBuf::Builder  Vertices;

public:
    GLTextComposer();

    void setAtlas(Atlas &atlas);
    void setWrapping(FontLineWrapping const &wrappedLines);

    void setText(String const &text);
    void setStyledText(String const &styledText);
    void setText(String const &text, Font::RichFormat const &format);

    /**
     * Sets the range of visible lines.
     *
     * @param visibleLineRange  Visible range of lines.
     */
    void setRange(Rangei const &visibleLineRange);

    Rangei range() const;

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
     * Forces all the text to be rerasterized.
     */
    void forceUpdate();

    /**
     * Releases all the allocations from the atlas.
     */
    void release();

    /**
     * Releases the allocated lines that are outside the current range.
     */
    void releaseLinesOutsideRange();

    void makeVertices(Vertices &triStrip,
                      Vector2i const &topLeft,
                      ui::Alignment const &lineAlign,
                      Vector4f const &color = Vector4f(1, 1, 1, 1));

    /**
     * Generates vertices for all the text lines and concatenates them onto the existing
     * triangle strip in @a triStrip.
     *
     * @param triStrip     Vertices for a triangle strip.
     * @param rect         Rectangle inside which the text will be placed.
     * @param alignInRect  Alignment within @a rect.
     * @param lineAlign    Horizontal alignment for each line within the paragraph.
     * @param color        Vertex color for the generated vertices.
     */
    void makeVertices(Vertices &triStrip,
                      Rectanglei const &rect,
                      ui::Alignment const &alignInRect,
                      ui::Alignment const &lineAlign,
                      Vector4f const &color = Vector4f(1, 1, 1, 1));

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_GLTEXTCOMPOSER_H
