/** @file fontlinewrapping.h  Font line wrapping.
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

#ifndef LIBAPPFW_FONTLINEWRAPPING_H
#define LIBAPPFW_FONTLINEWRAPPING_H

#include "libappfw.h"

#include <de/String>
#include <de/Font>
#include <de/Lockable>
#include <de/shell/ILineWrapping>

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
 */
class LIBAPPFW_PUBLIC FontLineWrapping : public Lockable, public shell::ILineWrapping
{
public:
    FontLineWrapping();

    void setFont(Font const &font);
    Font const &font() const;
    bool hasFont() const;

    /**
     * Clears the wrapping completely. The text is also cleared.
     */
    void clear();

    /**
     * Resets the existing wrapping (isEmpty() will return @c true) but does not
     * clear the text.
     */
    void reset();

    void wrapTextToWidth(String const &text, int maxWidth);
    void wrapTextToWidth(String const &text, Font::RichFormat const &format, int maxWidth);

    bool isEmpty() const;
    String const &text() const;
    shell::WrappedLine line(int index) const;
    int width() const;
    int height() const;
    int rangeWidth(Rangei const &range) const;
    int indexAtWidth(Rangei const &range, int width) const;

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
    Vector2i charTopLeftInPixels(int line, int charIndex);

    struct LineInfo
    {
        struct Segment {
            Rangei range;
            int tabStop;
            int width;

            Segment(Rangei const &r, int tab = 0) : range(r), tabStop(tab), width(0)
            {}
        };
        typedef QList<Segment> Segments;
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
    LineInfo const &lineInfo(int index) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_FONTLINEWRAPPING_H
