/** @file textcanvas.h Text-based drawing surface.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_TEXTCANVAS_H
#define LIBSHELL_TEXTCANVAS_H

#include <QChar>
#include <QFlags>
#include <de/Vector>
#include <de/Rectangle>

namespace de {
namespace shell {

/**
 * Text-based, device-independent drawing surface.
 *
 * When characters are written to the canvas (or their properties change), they
 * get marked dirty. When a surface is drawn on screen, only the dirty
 * characters need to be drawn, as they are the only ones that have changed
 * relative to the previous state.
 */
class TextCanvas
{
public:
    typedef Vector2i Size;
    typedef Vector2i Coord;

    struct Char
    {
        enum Attrib
        {
            Bold      = 0x1,
            Underline = 0x2,
            Reverse   = 0x4,

            Dirty     = 0x80000000,

            DefaultAttributes = 0,
            VisualAttributes = Bold | Underline | Reverse
        };
        Q_DECLARE_FLAGS(Attribs, Attrib)

        QChar ch;
        Attribs attribs;

    public:
        Char(QChar const &c = QChar(' '), Attribs const &at = DefaultAttributes)
            : ch(c), attribs(at)
        {
            attribs |= Dirty;
        }

        Char &operator = (Char const &other)
        {
            bool changed = false;

            if(ch != other.ch)
            {
                ch = other.ch;
                changed = true;
            }
            if((attribs & VisualAttributes) != (other.attribs & VisualAttributes))
            {
                attribs &= ~VisualAttributes;
                attribs |= (other.attribs & VisualAttributes);
                changed = true;
            }

            if(changed)
            {
                attribs |= Dirty;
            }
            return *this;
        }

        bool isDirty() const
        {
            return attribs.testFlag(Dirty);
        }
    };

public:
    TextCanvas(Size const &size = Size(1, 1));

    virtual ~TextCanvas();

    Size size() const;

    void resize(Size const &newSize);

    /**
     * Returns a modifiable reference to a character. The character is
     * not marked dirty automatically.
     *
     * @param pos  Position of the character.
     *
     * @return Character.
     */
    Char &at(Coord const &pos);

    Char const &at(Coord const &pos) const;

    /**
     * Determines if a coordinate is valid.
     * @param pos  Coordinate.
     * @return @c true, if the position can be accessed with at().
     */
    bool isValid(Coord const &pos) const;

    /**
     * Marks the entire canvas dirty.
     */
    void markDirty();

    void clear(Char const &ch = Char());

    void fill(Rectanglei const &rect, Char const &ch);

    void put(Vector2i const &pos, Char const &ch);

    void drawText(Vector2i const &pos, String const &text, Char::Attribs const &attribs);

    /**
     * Copies the contents of this canvas onto another canvas.
     *
     * @param dest     Destination canvas.
     * @param topLeft  Top left coordinate of the destination area.
     */
    void blit(TextCanvas &dest, Coord const &topLeft) const;

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
    virtual void setCursorPosition(Vector2i const &pos);

private:
    struct Instance;
    Instance *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TextCanvas::Char::Attribs)

} // namespace shell
} // namespace de

#endif // LIBSHELL_TEXTCANVAS_H
