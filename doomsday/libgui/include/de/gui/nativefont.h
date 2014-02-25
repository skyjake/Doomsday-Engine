/** @file nativefont.h  Abstraction of a native font.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_NATIVEFONT_H
#define LIBGUI_NATIVEFONT_H

#include "libgui.h"

#include <de/String>
#include <de/Asset>
#include <de/Rectangle>

#include <QImage>
#include <QMap>

namespace de {

/**
 * Provides the means to access the platform's native fonts, measure the bounds of a
 * string of text, and draw the text onto an Image. This is an abstract base class for
 * concrete implementations of native fonts.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC NativeFont : public Asset
{
public:
    enum Style
    {
        Regular,
        Italic
    };

    enum Weight
    {
        UltraLight = 0,
        Light      = 25,
        Normal     = 50,
        Bold       = 75,
        Black      = 100
    };

    struct Spec
    {
        Style style;
        dint weight;

        Spec(Style s = Regular, dint w = Normal) : style(s), weight(w) {}

        bool operator < (Spec const &other) const { // QMap key order
            if(weight < other.weight) return true;
            if(weight > other.weight) return false;
            return (style < other.style);
        }
    };

    typedef QMap<Spec, String> StyleMapping; ///< Spec => native font name

    /**
     * Defines a mapping from font family name plus style/weight to an actual platform
     * font.
     *
     * @param mapping  Mapping of styles to native font names.
     */
    static void defineMapping(String const &family, StyleMapping const &mapping);

public:
    NativeFont(String const &family = "");
    NativeFont(NativeFont const &other);

    void setFamily(String const &family);
    void setSize(dfloat size);
    void setStyle(Style style);
    void setWeight(dint weight);

    String family() const;
    dfloat size() const;
    Style style() const;
    dint weight() const;

    /**
     * Determines the native font name based on style mappings.
     */
    String nativeFontName() const;

    int ascent() const;
    int descent() const;
    int height() const;
    int lineSpacing() const;

    /**
     * Measures the extents of a line of text.
     *
     * @param text  Text line.
     *
     * @return Boundaries.
     */
    Rectanglei measure(String const &text) const;

    int width(String const &text) const;

    /**
     * Draws a line of text using the font into an image.
     *
     * @param text  Text line.
     * @param foreground  Foreground/text color.
     * @param background  Background color.
     *
     * @return Image of the text, with the same dimensions as returned by measure().
     */
    QImage rasterize(String const &text,
                     Vector4ub const &foreground,
                     Vector4ub const &background) const;

protected:
    NativeFont &operator = (NativeFont const &other);

    /**
     * Called when the font is needed to be used but it isn't marked Ready.
     */
    virtual void commit() const = 0;

    virtual int nativeFontAscent() const = 0;
    virtual int nativeFontDescent() const = 0;
    virtual int nativeFontHeight() const = 0;
    virtual int nativeFontLineSpacing() const = 0;

    virtual int nativeFontWidth(String const &text) const = 0;
    virtual Rectanglei nativeFontMeasure(String const &text) const = 0;
    virtual QImage nativeFontRasterize(String const &text,
                                       Vector4ub const &foreground,
                                       Vector4ub const &background) const = 0;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_NATIVEFONT_H
