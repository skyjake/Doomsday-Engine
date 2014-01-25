/** @file qtnativefont.h  Qt-based native font.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_QTNATIVEFONT_H
#define LIBGUI_QTNATIVEFONT_H

#include "nativefont.h"

namespace de {

class QtNativeFont : public NativeFont
{
public:
    QtNativeFont(String const &family = "");
    QtNativeFont(QtNativeFont const &other);
    QtNativeFont(QFont const &font);

    QtNativeFont &operator = (QtNativeFont const &other);

protected:
    void commit() const;

    int nativeFontAscent() const;
    int nativeFontDescent() const;
    int nativeFontHeight() const;
    int nativeFontLineSpacing() const;

    Rectanglei nativeFontMeasure(String const &text) const;
    int nativeFontWidth(String const &text) const;
    QImage nativeFontRasterize(String const &text,
                               Vector4ub const &foreground,
                               Vector4ub const &background) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_QTNATIVEFONT_H
