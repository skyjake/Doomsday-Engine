/** @file gltextcomposer.h
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

#ifndef LIBAPPFW_GLTEXTCOMPOSER_H
#define LIBAPPFW_GLTEXTCOMPOSER_H

#include <de/font.h>
#include <de/atlas.h>
#include <de/glbuffer.h>
#include <de/painter.h>

#include "ui/defs.h"
#include "de/fontlinewrapping.h"

namespace de {

/**
 * Allocates and releases lines of text on an atlas and produces geometry for
 * drawing the text.
 *
 * Relies on a pre-existing FontLineWrapping where the text content has been
 * wrapped onto multiple lines and laid out appropriately.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC GLTextComposer : public Asset
{
public:
    GLTextComposer();

    void setAtlas(Atlas &atlas);
    void setWrapping(const FontLineWrapping &wrappedLines);

    void setText(const String &text);
    void setStyledText(const String &styledText);
    void setText(/*const String &text, */const Font::RichFormat &format);

    /**
     * Sets the range of visible lines.
     *
     * @param visibleLineRange  Visible range of lines.
     */
    void setRange(const Rangei &visibleLineRange);

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

    void makeVertices(GuiVertexBuilder &triStrip,
                      const Vec2i &topLeft,
                      const ui::Alignment &lineAlign,
                      const Vec4f &color = Vec4f(1));

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
    void makeVertices(GuiVertexBuilder &triStrip,
                      const Rectanglei &rect,
                      const ui::Alignment &alignInRect,
                      const ui::Alignment &lineAlign,
                      const Vec4f &color = Vec4f(1));

    /**
     * Returns the maximum width of the generated vertices. This is only valid after
     * makeVertices() has been called.
     *
     * This may be larger than the maximum width as determined by FontLineWrapping if
     * tabbed lines are used in the text. This is because text segments are aligned
     * with tab stops only during makeVertices().
     *
     * @todo Ideally tab stop alignment should be done in FontLineWrapping, so that the
     * maximum width would be known prior to generating the vertices.
     *
     * @return Maximum width of the generated vertices.
     */
    int verticesMaxWidth() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_GLTEXTCOMPOSER_H
