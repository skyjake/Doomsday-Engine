/** @file qtnativefont.cpp  Qt-based native font.
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

#include "../src/text/qtnativefont.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>

namespace de
{
    DE_PIMPL_NOREF(QtNativeFont)
    {
        QFont font;
        QScopedPointer<QFontMetrics> metrics;
    };
}

namespace de {

QtNativeFont::QtNativeFont(String const &family)
    : NativeFont(family), d(new Impl)
{}

QtNativeFont::QtNativeFont(QFont const &font)
    : NativeFont(font.family()), d(new Impl)
{
    d->font = font;
    setSize(font.pointSizeF());
    setWeight(font.weight());
    setStyle(font.italic()? Italic : Regular);
    setTransform(font.capitalization() == QFont::AllUppercase? Uppercase :
                 font.capitalization() == QFont::AllLowercase? Lowercase : NoTransform);
}

QtNativeFont::QtNativeFont(QtNativeFont const &other)
    : NativeFont(other), d(new Impl)
{
    d->font = other.d->font;
}

QtNativeFont &QtNativeFont::operator = (QtNativeFont const &other)
{
    NativeFont::operator = (other);
    d->font = other.d->font;
    return *this;
}

void QtNativeFont::commit() const
{
    d->font.setFamily(family());
    d->font.setPointSizeF(size());
    d->font.setStyle(style() == Italic? QFont::StyleItalic : QFont::StyleNormal);
    d->font.setWeight(weight());
    d->font.setCapitalization(transform() == Uppercase? QFont::AllUppercase :
                              transform() == Lowercase? QFont::AllLowercase : QFont::MixedCase);

    d->metrics.reset(new QFontMetrics(d->font));
}

int QtNativeFont::nativeFontAscent() const
{
    return d->metrics->ascent();
}

int QtNativeFont::nativeFontDescent() const
{
    return d->metrics->descent();
}

int QtNativeFont::nativeFontHeight() const
{
    return d->metrics->height();
}

int QtNativeFont::nativeFontLineSpacing() const
{
    return d->metrics->lineSpacing();
}

Rectanglei QtNativeFont::nativeFontMeasure(String const &text) const
{
#ifdef LIBGUI_ACCURATE_TEXT_BOUNDS
    Rectanglei rect = Rectanglei::fromQRect(d->metrics->boundingRect(text));
#else
    Rectanglei rect(Vec2i(0, -d->metrics->ascent()),
                    Vec2i(d->metrics->width(text),
                             d->metrics->descent()));
#endif

    if (rect.height() == 0)
    {
        // It seems measuring the bounds of a Tab character produces
        // strange results (position 100000?).
        rect = Rectanglei(0, 0, rect.width(), 0);
    }

    return rect;
}

int QtNativeFont::nativeFontWidth(String const &text) const
{
    return d->metrics->width(text);
}

QImage QtNativeFont::nativeFontRasterize(String const &text,
                                         Vec4ub const &foreground,
                                         Vec4ub const &background) const
{
    Rectanglei const bounds = measure(text);

    QColor const fgColor(foreground.x, foreground.y, foreground.z, foreground.w);
    QColor const bgColor(background.x, background.y, background.z, background.w);

    QImage img(QSize(bounds.width() + 1, bounds.height() + 1),
               QImage::Format_ARGB32);

    img.fill(bgColor.rgba());

    QPainter painter(&img);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    painter.setFont(d->font);
    painter.setPen(fgColor);
    painter.setBrush(bgColor);
    painter.drawText(-bounds.left(), -bounds.top(), text);

    return img;
}

} // namespace de
