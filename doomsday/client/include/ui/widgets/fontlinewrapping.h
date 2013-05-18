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

#ifndef DENG_CLIENT_FONTLINEWRAPPING_H
#define DENG_CLIENT_FONTLINEWRAPPING_H

#include <de/String>
#include <de/Font>
#include <de/shell/ILineWrapping>

/**
 * Line wrapper that uses a particular font and calculates widths in pixels.
 * Height is still in lines, though. The wrapper cannot be used before the
 * font has been defined.
 */
class FontLineWrapping : public de::shell::ILineWrapping
{
public:
    FontLineWrapping();

    void setFont(de::Font const &font);
    de::Font const &font() const;

    bool isEmpty() const;
    void clear();
    void wrapTextToWidth(de::String const &text, int maxWidth);
    de::shell::WrappedLine line(int index) const;
    int width() const;
    int height() const;

    /**
     * Calculates the total height of the wrapped lined in pixels, taking into
     * consideration the appropriate amount of line spacing.
     */
    int totalHeightInPixels() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_FONTLINEWRAPPING_H
