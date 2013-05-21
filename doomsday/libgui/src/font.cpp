/** @file font.cpp  Font with metrics.
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

#include "de/Font"

#include <de/ConstantRule>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>

namespace de {

DENG2_PIMPL(Font)
{
    QFont font;
    QScopedPointer<QFontMetrics> metrics;
    ConstantRule *heightRule;
    ConstantRule *ascentRule;
    ConstantRule *descentRule;
    ConstantRule *lineSpacingRule;

    Instance(Public *i) : Base(i)
    {
        createRules();
    }

    Instance(Public *i, QFont const &qfont) : Base(i), font(qfont)
    {
        createRules();
        updateMetrics();
    }

    void createRules()
    {
        heightRule      = new ConstantRule(0);
        ascentRule      = new ConstantRule(0);
        descentRule     = new ConstantRule(0);
        lineSpacingRule = new ConstantRule(0);
    }

    void updateMetrics()
    {
        metrics.reset(new QFontMetrics(font));

        heightRule->set(metrics->height());
        ascentRule->set(metrics->ascent());
        descentRule->set(metrics->descent());
        lineSpacingRule->set(metrics->lineSpacing());
    }
};

Font::Font() : d(new Instance(this))
{}

Font::Font(Font const &other) : d(new Instance(this, other.d->font))
{}

Font::Font(QFont const &font) : d(new Instance(this, font))
{}

QFont Font::toQFont() const
{
    return d->font;
}

Rectanglei Font::measure(String const &textLine) const
{
    return Rectanglei::fromQRect(d->metrics->boundingRect(textLine));
}

int Font::advanceWidth(String const &textLine) const
{
    return d->metrics->width(textLine);
}

QImage Font::rasterize(String const &textLine,
                       Vector4ub const &foreground,
                       Vector4ub const &background) const
{
    Rectanglei bounds = measure(textLine);

    QImage img(QSize(bounds.width(), bounds.height()), QImage::Format_ARGB32);

    QColor bgColor(background.x, background.y, background.z, background.w);
    img.fill(bgColor.rgba());

    QPainter painter(&img);
    painter.setFont(d->font);
    painter.setPen(QColor(foreground.x, foreground.y, foreground.z, foreground.w));
    painter.setBrush(bgColor);
    painter.drawText(0, d->ascentRule->valuei(), textLine);
    return img;
}

Rule const &Font::height() const
{
    return *d->heightRule;
}

Rule const &Font::ascent() const
{
    return *d->ascentRule;
}

Rule const &Font::descent() const
{
    return *d->descentRule;
}

Rule const &Font::lineSpacing() const
{
    return *d->lineSpacingRule;
}

} // namespace de
