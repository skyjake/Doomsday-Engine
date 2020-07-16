/** @file textdrawable.h  High-level GL text drawing utility.
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

#ifndef LIBAPPFW_TEXTDRAWABLE_H
#define LIBAPPFW_TEXTDRAWABLE_H

#include "gltextcomposer.h"
#include "fontlinewrapping.h"

namespace de {

/**
 * High-level GL text drawing utility.
 *
 * The task of drawing text is rather complicated. There are several components
 * involved:
 *
 * - EscapeParser parses style escape codes.
 * - Font understands rich text formatting and performs the actual
 *   rasterization of text onto bitmap images.
 * - FontLineWrapping takes rich-formatted ("styled") text and wraps it onto
 *   multiple lines, taking into account tab stops and indentation.
 * - GLTextComposer allocates the text lines from an Atlas and generates the
 *   triangle strips needed for actually drawing the text on the screen.
 *
 * TextDrawable is a high-level utility for controlling this entire process as
 * easily as possible. If fine-grained control of the text is required, one can
 * still use the lower level components directly.
 *
 * TextDrawable handles wrapping of text using a background thread, so content
 * length is unlimited and the calling thread will not block. TextDrawable is
 * an Asset, and will not be marked ready until the background wrapping is
 * done. One is still required to call TextDrawable::update() before drawing,
 * though.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC TextDrawable : public GLTextComposer
{
public:
    TextDrawable();

    void init(Atlas &atlas, const Font &font,
              const Font::RichFormat::IStyle *style = 0);

    void deinit();

    void clear();

    /**
     * Sets the maximum width for text lines.
     *
     * @param maxLineWidth  Maximum line width.
     */
    void setLineWrapWidth(int maxLineWidth);

    void setText(const String &styledText);

    void setFont(const Font &font);

    /**
     * Sets the range of visible lines and releases all allocations outside
     * the range.
     *
     * @param lineRange  Range of visible lines.
     */
    void setRange(const Rangei &lineRange);

    /**
     * Updates the status of the composer. This includes first checking if a
     * background wrapping task has been completed, and if so, the results of
     * background wrapping become the visible data to be drawn.
     *
     * @return @c true, if the lines have changed and it is necessary to remake
     * the geometry. @c false, if nothing further needs to be done or text is
     * still being preprocessed for drawing.
     */
    bool update();

    const FontLineWrapping &wraps() const;

    Vec2ui wrappedSize() const;
    String text() const;
    bool isBeingWrapped() const;
    const Font &font() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_TEXTDRAWABLE_H
