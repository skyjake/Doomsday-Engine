/** @file coretextnativefont_macx.h
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

#ifndef LIBGUI_CORETEXTNATIVEFONT_H
#define LIBGUI_CORETEXTNATIVEFONT_H

#include "nativefont.h"

namespace de {

/**
 * OS X specific native font implementation that uses Core Text.
 */
class CoreTextNativeFont : public NativeFont
{
public:
    CoreTextNativeFont(String const &family = "");
    CoreTextNativeFont(CoreTextNativeFont const &other);
    CoreTextNativeFont(QFont const &font);

    CoreTextNativeFont &operator = (CoreTextNativeFont const &other);

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

#endif // LIBGUI_CORETEXTNATIVEFONT_H
