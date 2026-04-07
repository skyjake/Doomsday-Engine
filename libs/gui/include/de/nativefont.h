/** @file nativefont.h  Abstraction of a native font.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/string.h>
#include <de/asset.h>
#include <de/rectangle.h>
#include <de/keymap.h>

#include "de/image.h"

namespace de {

/**
 * Provides the means to access the platform's native fonts, measure the bounds of a
 * string of text, and draw the text onto an Image. This is an abstract base class for
 * concrete implementations of native fonts.
 *
 * Each NativeFont is specific to a single thread. This allows text rendering to be
 * done in multiple background threads without synchronization.
 *
 * @ingroup gui
 */
class LIBGUI_PUBLIC NativeFont : public Asset
{
public:
    enum Style { Regular, Italic };
    enum Weight
    {
        UltraLight = 0,
        Light      = 25,
        Normal     = 50,
        Bold       = 75,
        Black      = 100
    };
    enum Transform { NoTransform, Uppercase, Lowercase };

    struct Spec
    {
        Style     style;
        int       weight;
        Transform transform;

        Spec(Style s = Regular, int w = Normal, Transform xform = NoTransform)
            : style(s), weight(w), transform(xform) {}

        bool operator == (const Spec &other) const {
            return style == other.style && weight == other.weight && transform == other.transform;
        }
        bool operator < (const Spec &other) const { // Map key order
            if (weight < other.weight) return true;
            if (weight > other.weight) return false;
            if (style == other.style) {
                return transform < other.transform;
            }
            return (style < other.style);
        }
    };

    typedef KeyMap<Spec, String> StyleMapping; ///< Spec => native font name

    /**
     * Defines a mapping from font family name plus style/weight to an actual platform
     * font.
     *
     * @param family   Native font family name.
     * @param mapping  Mapping of styles to native font names.
     */
    static void defineMapping(const String &family, const StyleMapping &mapping);

public:
    NativeFont(const String &family = "");
    NativeFont(const NativeFont &other);

    void setFamily(const String &family);
    void setPointSize(float pointSize);
    void setStyle(Style style);
    void setWeight(int weight);
    void setTransform(Transform transform);

    String    family() const;
    float     pointSize() const;
    Style     style() const;
    int       weight() const;
    Transform transform() const;

    /**
     * Determines the native font name based on style mappings.
     */
    String nativeFontName() const;

    // Metrics are returned as pixels:
    int ascent() const;
    int descent() const;
    int height() const;
    int lineSpacing() const;

    /**
     * Measures the extents of a line of text as pixels.
     *
     * @param text  Text line.
     *
     * @return Boundaries as pixels. The coordinat eorigin (0,0) is on the baseline.
     */
    Rectanglei measure(const String &text) const;

    /**
     * Advance width of a text string as pixels.
     *
     * @param text  Text string.
     *
     * @return Pixel width.
     */
    int advanceWidth(const String &text) const;

    /**
     * Draws a line of text using the font into an image.
     *
     * @param text        Text line.
     * @param foreground  Foreground/text color.
     * @param background  Background color.
     *
     * @return Image of the text, with the same dimensions as returned by measure().
     * The image origin is set to appropriately offset its top left corner so that
     * the (0,0) coordinates are at the baseline.
     */
    Image rasterize(const String &      text,
                    const Image::Color &foreground,
                    const Image::Color &background) const;

    /**
     * Sets the pixels-per-point ratio for measuring and rasterizing text.
     *
     * @param pixelRatio  Number of pixels per point.
     */
    static void setPixelRatio(float pixelRatio);

    static float pixelRatio();

protected:
    NativeFont &operator=(const NativeFont &other);

    /**
     * Called when the font is needed to be used but it isn't marked Ready.
     */
    virtual void commit() const = 0;

    virtual int nativeFontAscent() const      = 0;
    virtual int nativeFontDescent() const     = 0;
    virtual int nativeFontHeight() const      = 0;
    virtual int nativeFontLineSpacing() const = 0;

    virtual int        nativeFontAdvanceWidth(const String &text) const          = 0;
    virtual Rectanglei nativeFontMeasure(const String &text) const               = 0;
    virtual Image      nativeFontRasterize(const String &      text,
                                           const Image::Color &foreground,
                                           const Image::Color &background) const = 0;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_NATIVEFONT_H
