/** @file stbttnativefont.h  Text rendering with stb_truetype.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_STBTTNATIVEFONT_H
#define LIBGUI_STBTTNATIVEFONT_H

#include "de/nativefont.h"
#include "de/image.h"

namespace de {

/**
 * Native font implementation that uses stb_truetype.
 */
class StbTtNativeFont : public NativeFont
{
public:
    StbTtNativeFont(const String &family = "");
    StbTtNativeFont(const StbTtNativeFont &other);

    StbTtNativeFont &operator=(const StbTtNativeFont &other);

    static bool load(const String &fontName, const Block &fontData);

protected:
    void commit() const override;

    int nativeFontAscent() const override;
    int nativeFontDescent() const override;
    int nativeFontHeight() const override;
    int nativeFontLineSpacing() const override;

    int        nativeFontAdvanceWidth(const String &text) const override;
    Rectanglei nativeFontMeasure(const String &text) const override;
    Image      nativeFontRasterize(const String &      text,
                                   const Image::Color &foreground,
                                   const Image::Color &background) const override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_STBTTNATIVEFONT_H

