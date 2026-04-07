/** @file textcanvas.h Text-based drawing surface.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_TEXTCANVAS_H
#define LIBSHELL_TEXTCANVAS_H

#include "../ilinewrapping.h"
#include "../vector.h"
#include "../rectangle.h"

namespace de { namespace term {

/**
 * Flags for specifying alignment.
 * @ingroup textUi
 */
enum AlignmentFlag { AlignTop = 0x1, AlignBottom = 0x2, AlignLeft = 0x4, AlignRight = 0x8 };
using Alignment = Flags;

/**
 * Text-based, device-independent drawing surface.
 *
 * When characters are written to the canvas (or their properties change), they
 * get marked dirty. When a surface is drawn on screen, only the dirty
 * characters need to be drawn, as they are the only ones that have changed
 * relative to the previous state.
 *
 * @ingroup textUi
 */
class DE_PUBLIC TextCanvas
{
public:
    typedef Vec2ui Size;
    typedef Vec2i  Coord;

    struct AttribChar {
        enum Attrib {
            Bold      = 0x1,
            Underline = 0x2,
            Reverse   = 0x4,
            Blink     = 0x8,

            Dirty = 0x80000000,

            DefaultAttributes = 0,
            VisualAttributes  = Bold | Underline | Reverse | Blink
        };
        using Attribs = Flags;

        Char    ch;
        Attribs attribs;

    public:
        AttribChar(const Char c = ' ', Attribs at = DefaultAttributes)
            : ch(c)
            , attribs(at)
        {
            attribs |= Dirty;
        }

        AttribChar(const AttribChar &) = default;

        bool operator<(const AttribChar &other) const
        {
            if (ch == other.ch)
            {
                return (attribs & VisualAttributes) < (other.attribs & VisualAttributes);
            }
            return ch < other.ch;
        }

        bool operator==(const AttribChar &other) const
        {
            return (ch == other.ch &&
                    (attribs & VisualAttributes) == (other.attribs & VisualAttributes));
        }

        AttribChar &operator=(const AttribChar &other)
        {
            bool changed = false;

            if (ch != other.ch)
            {
                ch      = other.ch;
                changed = true;
            }
            if ((attribs & VisualAttributes) != (other.attribs & VisualAttributes))
            {
                attribs &= ~VisualAttributes;
                attribs |= (other.attribs & VisualAttributes);
                changed = true;
            }

            if (changed)
            {
                attribs |= Dirty;
            }
            return *this;
        }

        bool isDirty() const { return attribs.testFlag(Dirty); }
    };

public:
    TextCanvas(const Size &size = Size(1, 1));
    virtual ~TextCanvas();

    Size       size() const;
    int        width() const;
    int        height() const;
    Rectanglei rect() const;

    virtual void resize(const Size &newSize);

    /**
     * Returns a modifiable reference to a character. The character is
     * not marked dirty automatically.
     *
     * @param pos  Position of the character.
     *
     * @return Character.
     */
    AttribChar &at(const Coord &pos);

    const AttribChar &at(const Coord &pos) const;

    /**
     * Determines if a coordinate is valid.
     * @param pos  Coordinate.
     * @return @c true, if the position can be accessed with at().
     */
    bool isValid(const Coord &pos) const;

    /**
     * Marks the entire canvas dirty.
     */
    void markDirty();

    void clear(const AttribChar &ch = AttribChar());

    void fill(const Rectanglei &rect, const AttribChar &ch);

    void put(const Coord &pos, const AttribChar &ch);

    void drawText(const Coord &              pos,
                  const String &             text,
                  const AttribChar::Attribs &attribs    = AttribChar::DefaultAttributes,
                  BytePos                    richOffset = BytePos(0));

    /**
     * Draws line wrapped text. Use de::shell::wordWrapText() to determine
     * appropriate wrapped lines.
     *
     * @param pos            Top left / starting point for the text.
     * @param text           The entire text to be drawn.
     * @param wraps          Line wrapping.
     * @param attribs        Character attributes.
     * @param lineAlignment  Alignment for lines.
     */
    void drawWrappedText(const Coord &              pos,
                         const String &             text,
                         const ILineWrapping &      wraps,
                         const AttribChar::Attribs &attribs       = AttribChar::DefaultAttributes,
                         const Alignment &          lineAlignment = AlignLeft);

    void clearRichFormat();

    void setRichFormatRange(const AttribChar::Attribs &attribs, const String::ByteRange &range);

    void drawLineRect(const Rectanglei &         rect,
                      const AttribChar::Attribs &attribs = AttribChar::DefaultAttributes);

    /**
     * Draws the contents of a canvas onto this canvas.
     *
     * @param canvas   Source canvas.
     * @param topLeft  Top left coordinate of the destination area.
     */
    void draw(const TextCanvas &canvas, const Coord &topLeft);

    /**
     * Draws all characters marked dirty onto the screen so that they become
     * visible. This base class implementation just marks all characters not
     * dirty -- call this as the last step in derived class' show() method.
     */
    virtual void show();

    /**
     * Sets the position of the cursor on the canvas. Derived classes may
     * choose to visualize the cursor in some fashion (especially if this isn't
     * taken care of by the display device).
     *
     * @param pos  Position.
     */
    virtual void setCursorPosition(const Coord &pos);

private:
    DE_PRIVATE(d)
};

}} // namespace de::term

#endif // LIBSHELL_TEXTCANVAS_H
