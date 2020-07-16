/** @file fontlinewrapping.h  Font line wrapping.
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

#ifndef LIBAPPFW_FONTLINEWRAPPING_H
#define LIBAPPFW_FONTLINEWRAPPING_H

#include "libgui.h"

#include <de/font.h>
#include <de/ilinewrapping.h>
#include <de/image.h>
#include <de/lockable.h>
#include <de/string.h>

namespace de {

/**
 * Line wrapper that uses a particular font and calculates widths in pixels.
 * Height is still in lines, though. The wrapper cannot be used before the
 * font has been defined.
 *
 * Supports indentation of lines, as marked in the RichFormat.
 *
 * @par Thread-safety
 *
 * FontLineWrapping locks itself automatically when any of its methods are
 * being executed. Instances can be used from multiple threads.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC FontLineWrapping : public Lockable, public ILineWrapping
{
public:
    FontLineWrapping();

    void        setFont(const Font &font);
    const Font &font() const;
    bool        hasFont() const;

    /**
     * Clears the wrapping completely. The text is also cleared.
     */
    void clear() override;

    /**
     * Resets the existing wrapping (isEmpty() will return @c true) but does not
     * clear the text.
     */
    void reset();

    void wrapTextToWidth(const String &text, WrapWidth maxWidth) override;
    void wrapTextToWidth(const String &text, const Font::RichFormat &format, WrapWidth maxWidth);

    void rasterizeLines(const Rangei &lineRange);
    void clearRasterizedLines() const;

    /**
     * Cancels the ongoing wrapping operation. This is useful when doing long wrapping
     * operations in the background. An exception is thrown from the ongoing
     * wrapTextToWidth() call.
     */
    void cancel();

    bool          isEmpty() const override;
    const String &text() const;
    WrappedLine   line(int index) const override;
    WrapWidth     width() const override;
    int           height() const override;
    WrapWidth     rangeWidth(const CString &range) const override;
    BytePos       indexAtWidth(const CString &range, WrapWidth width) const override;

    /**
     * Calculates the total height of the wrapped lined in pixels. If there are
     * multiple lines, takes into consideration the appropriate leading between
     * lines.
     */
    int totalHeightInPixels() const;

    /**
     * Returns the maximum width given to wrapTextToWidth().
     */
    int maximumWidth() const;

    /**
     * Determines the coordinates of a character's top left corner in pixels.
     *
     * @param line       Wrapped line number.
     * @param charIndex  Index of the character on the wrapped line. Each
     *                   wrapped line is indexed starting from zero.
     *
     * @return XY coordinates of the character.
     */
    Vec2i charTopLeftInPixels(int line, BytePos charIndex);

    struct LineInfo
    {
        struct Segment {
            CString   range;
            int       tabStop;
            WrapWidth width;

            Segment(const CString &r = CString(), int tab = -1)
                : range(r)
                , tabStop(tab)
                , width(0)
            {}
        };
        typedef List<Segment> Segments;
        Segments segs;

        int indent;         ///< Left indentation to apply to the entire line.

    public:
        int highestTabStop() const;
    };

    /**
     * Returns the left indentation for a wrapped line.
     *
     * @param index  Wrapped line number.
     */
    const LineInfo &lineInfo(int index) const;

    /**
     * Returns a rasterized version of a segment.
     * Before calling this, segments must first be rasterized by calling
     * rasterizeAllSegments().
     *
     * @param line     Line index.
     * @param segment  Segment on the line.
     */
    Image rasterizedSegment(int line, int segment) const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_FONTLINEWRAPPING_H
