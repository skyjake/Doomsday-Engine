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
#include <QFontDatabase>
#include <QImage>
#include <QPainter>

#if defined(WIN32) || defined(MACOSX)
#  define LIBGUI_ACCURATE_TEXT_BOUNDS
#endif

namespace de {

DENG2_PIMPL(Font)
{
    QFont font;
    QScopedPointer<QFontMetrics> metrics;
    ConstantRule *heightRule;
    ConstantRule *ascentRule;
    ConstantRule *descentRule;
    ConstantRule *lineSpacingRule;
    int ascent;

    enum AltFamilyId { AltFamilyLight, AltFamilyBold, NUM_ALTS };
    String altFamily[NUM_ALTS];

    Instance(Public *i) : Base(i), ascent(0)
    {
        createRules();
    }

    Instance(Public *i, QFont const &qfont) : Base(i), font(qfont)
    {
#if 0
        // Development aid: list all available fonts and styles.
        QFontDatabase db;
        foreach(QString fam, db.families())
        {
            qDebug() << "FONT FAMILY:" << fam;
            qDebug() << "\tStyles:" << db.styles(fam);
        }
#endif

        createRules();
        updateMetrics();
    }

    void setAltFamily(RichFormat::Weight weight, String const &family)
    {
        switch(weight)
        {
        case RichFormat::Light:
            altFamily[AltFamilyLight] = family;
            break;

        case RichFormat::Bold:
            altFamily[AltFamilyBold] = family;
            break;

        default:
            break;
        }
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

        ascent = metrics->ascent();
        if(font.weight() != QFont::Normal)
        {
            // Use the ascent of the normal weight for non-normal weights;
            // we need to align content to baseline regardless of weight.
            ascent = QFontMetrics(QFont(font.family(), font.pointSize())).ascent();
        }

        ascentRule->set(ascent);
        descentRule->set(metrics->descent());
        heightRule->set(metrics->height());
        lineSpacingRule->set(metrics->lineSpacing());
    }

    /**
     * Produces a font based on this one but with the attribute modifications applied
     * from a rich format range.
     *
     * @param rich  Rich formatting.
     *
     * @return  Font with applied formatting.
     */
    QFont alteredFont(RichFormat::Iterator const &rich) const
    {
        if(!rich.isDefault())
        {
            QFont mod = font;

            // Size change.
            if(!fequal(rich.sizeFactor(), 1.f))
            {
                mod.setPointSizeF(mod.pointSizeF() * rich.sizeFactor());
            }

            // Style change (including monospace).
            switch(rich.style())
            {
            case RichFormat::OriginalStyle:
                break;

            case RichFormat::Regular:
                mod.setFamily(font.family());
                mod.setItalic(false);
                break;

            case RichFormat::Italic:
                mod.setFamily(font.family());
                mod.setItalic(true);
                break;

            case RichFormat::Monospace:
                if(rich.format.format().hasStyle())
                {
                    if(Font const *altFont = rich.format.format().style().richStyleFont(rich.style()))
                    {
                        mod.setFamily(altFont->d->font.family());
                        mod.setItalic(altFont->d->font.italic());
                        mod.setWeight(altFont->d->font.weight());
                        mod.setPointSizeF(altFont->d->font.pointSizeF());
                    }
                }
                break;
            }

            // Weight change.
            if(rich.weight() != RichFormat::OriginalWeight)
            {
                mod.setWeight(rich.weight() == RichFormat::Normal? QFont::Normal :
                              rich.weight() == RichFormat::Bold?   QFont::Bold   :
                                                                   QFont::Light);

                // Some weights may require an alternate font family.
                if(rich.weight() == RichFormat::Light && !altFamily[AltFamilyLight].isEmpty())
                {
                    mod.setFamily(altFamily[AltFamilyLight]);
                }
                else if(rich.weight() == RichFormat::Bold && !altFamily[AltFamilyBold].isEmpty())
                {
                    mod.setFamily(altFamily[AltFamilyBold]);
                }
            }
            return mod;
        }
        return font;
    }

    QFontMetrics alteredMetrics(RichFormat::Iterator const &rich) const
    {
        if(!rich.isDefault())
        {
            return QFontMetrics(alteredFont(rich));
        }
        return *metrics;
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

void Font::setAltFamily(RichFormat::Weight weight, String const &familyName)
{
    d->setAltFamily(weight, familyName);
}

Rectanglei Font::measure(String const &textLine) const
{
    return measure(textLine, RichFormat::fromPlainText(textLine));
}

Rectanglei Font::measure(String const &textLine, RichFormatRef const &format) const
{
    Rectanglei bounds;
    int advance = 0;

    RichFormat::Iterator iter(format);
    while(iter.hasNext())
    {
        iter.next();
        if(iter.range().isEmpty()) continue;

        QFontMetrics const metrics = d->alteredMetrics(iter);

        String const part = textLine.substr(iter.range());
        Rectanglei rect = Rectanglei::fromQRect(metrics.boundingRect(part));

        if(rect.height() == 0)
        {
            // It seems measuring the bounds of a Tab character produces
            // strange results (position 100000?).
            rect = Rectanglei(0, 0, rect.width(), 0);
        }

        // Combine to the total bounds.
        rect.moveTopLeft(Vector2i(advance, rect.top()));
        bounds |= rect;

        advance += metrics.width(part);
    }

    return bounds;
}

int Font::advanceWidth(String const &textLine) const
{
    return advanceWidth(textLine, RichFormat::fromPlainText(textLine));
}

int Font::advanceWidth(String const &textLine, RichFormatRef const &format) const
{
    int advance = 0;
    RichFormat::Iterator iter(format);
    while(iter.hasNext())
    {
        iter.next();
        if(iter.range().isEmpty()) continue;

        QFontMetrics const metrics = d->alteredMetrics(iter);
        advance += metrics.width(textLine.substr(iter.range()));
    }
    return advance;
}

QImage Font::rasterize(String const &textLine,
                       Vector4ub const &foreground,
                       Vector4ub const &background) const
{
    return rasterize(textLine, RichFormat::fromPlainText(textLine), foreground, background);
}

QImage Font::rasterize(String const &textLine,
                       RichFormatRef const &format,
                       Vector4ub const &foreground,
                       Vector4ub const &background) const
{
    if(textLine.isEmpty())
    {
        return QImage();
    }

#ifdef LIBGUI_ACCURATE_TEXT_BOUNDS
    Rectanglei const bounds = measure(textLine, format);
#else
    Rectanglei const bounds(0, 0,
                            advanceWidth(textLine, format),
                            d->metrics->height());
#endif

    QColor fgColor(foreground.x, foreground.y, foreground.z, foreground.w);
    QColor bgColor(background.x, background.y, background.z, background.w);

    QImage img(QSize(bounds.width() + 1,
                     de::max(duint(d->metrics->height()), bounds.height()) + 1),
               QImage::Format_ARGB32);
    img.fill(bgColor.rgba());

    QPainter painter(&img);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    int advance = 0;
    RichFormat::Iterator iter(format);
    while(iter.hasNext())
    {
        iter.next();
        if(iter.range().isEmpty()) continue;

        QFont font = d->font;

        if(iter.isDefault())
        {
            painter.setPen(fgColor);
            painter.setBrush(bgColor);
        }
        else
        {
            font = d->alteredFont(iter);

            if(iter.colorIndex() != RichFormat::OriginalColor)
            {
                RichFormat::IStyle::Color styleColor = iter.color();
                QColor const fg(styleColor.x, styleColor.y, styleColor.z, styleColor.w);
                QColor const bg(styleColor.x, styleColor.y, styleColor.z, 0);
                painter.setPen(fg);
                painter.setBrush(bg);
            }
            else
            {
                painter.setPen(fgColor);
                painter.setBrush(bgColor);
            }
        }
        painter.setFont(font);

        String const part = textLine.substr(iter.range());
        painter.drawText(advance, d->ascent, part);
        advance += QFontMetrics(font).width(part);
    }
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

int Font::xHeight() const
{
    return d->metrics->xHeight();
}

} // namespace de
