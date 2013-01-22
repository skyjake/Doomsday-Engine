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

#ifndef TEXTCANVAS_H
#define TEXTCANVAS_H

#include <QChar>
#include <de/Vector>
#include <de/Rectangle>

/**
 * Text-based, device-independent drawing surface.
 */
class TextCanvas
{
public:
    typedef de::Vector2i Size;
    typedef de::Vector2i Coord;

    struct Char
    {
        QChar ch;
        struct {
            unsigned char bold      : 1;
            unsigned char underline : 1;
            unsigned char reverse   : 1;
            unsigned char dirty     : 1;
        } attrib;

        Char(QChar const &c = QChar(' ')) : ch(c)
        {
            attrib.bold      = false;
            attrib.underline = false;
            attrib.reverse   = false;
            attrib.dirty     = true;
        }

        Char &operator = (Char const &other)
        {
            bool changed = false;

#define CH_COPY(prop) if(prop != other.prop) { prop = other.prop; changed = true; }
            CH_COPY(ch);
            CH_COPY(attrib.bold);
            CH_COPY(attrib.underline);
            CH_COPY(attrib.reverse);
#undef CH_COPY

            attrib.dirty = changed;
            return *this;
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

    void fill(de::Rectanglei const &rect, Char const &ch);

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

private:
    struct Instance;
    Instance *d;
};

#endif // TEXTCANVAS_H
