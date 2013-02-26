/** @file qttextcanvas.h  Text-based drawing surface for Qt.
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

#ifndef QTTEXTCANVAS_H
#define QTTEXTCANVAS_H

#include <de/shell/TextCanvas>
#include <QImage>
#include <QFont>
#include <QColor>

class QtTextCanvas : public de::shell::TextCanvas
{
public:
    QtTextCanvas(Size const &size);

    ~QtTextCanvas();

    QImage const &image() const;

    void resize(Size const &newSize);

    /**
     * Sets the size of a character in pixels. All characters are the same size.
     *
     * @param pixelSize  Pixel size of characters.
     */
    void setCharSize(de::Vector2i const &pixelSize);

    /**
     * Sets the font used for drawing.
     *
     * @param font  Font.
     */
    void setFont(QFont const &font);

    void setForegroundColor(QColor const &fg);

    QColor foregroundColor() const;

    void setBackgroundColor(QColor const &bg);

    QColor backgroundColor() const;

    void setCursorPosition(Coord const &pos);

    Coord cursorPosition() const;

    void setBlinkVisible(bool visible);

    /**
     * Draws the changed portions of the text canvas into the destination
     * buffer.
     */
    void show();

private:
    DENG2_PRIVATE(d)
};

#endif // QTTEXTCANVAS_H
