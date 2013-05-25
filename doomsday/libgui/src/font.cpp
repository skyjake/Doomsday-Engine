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

Font::RichFormat::RichFormat() : _style(0)
{}

Font::RichFormat::RichFormat(IStyle const &style) : _style(&style)
{}

Font::RichFormat::RichFormat(RichFormat const &other)
    : _style(other._style), _ranges(other._ranges)
{}

void Font::RichFormat::setStyle(IStyle const &style)
{
    _style = &style;
}

bool Font::RichFormat::haveStyle() const
{
    return _style != 0;
}

Font::RichFormat::IStyle const &Font::RichFormat::style() const
{
    return *_style;
}

Font::RichFormat Font::RichFormat::fromPlainText(String const &plainText)
{
    FormatRange all;
    all.range = Rangei(0, plainText.size());
    RichFormat form;
    form._ranges << all;
    return form;
}

String Font::RichFormat::initFromStyledText(String const &styledText)
{
    String plain;
    FormatRange *format = 0;
    Rangei range; // within styledText
    int offset = 0; // from styled to plain

    QList<FormatRange> stack;

    // Insert the first range.
    _ranges << FormatRange();
    format = &_ranges.back();

    forever
    {
        range.end = styledText.indexOf(QChar('\x1b'), range.start);
        if(range.end >= 0)
        {
            // Empty ranges do not cause insertion of new formats.
            if(range.size() > 0)
            {
                // Update current range's end.
                format->range.end = range.end;

                // Update plaintext.
                plain += styledText.substr(range);
                format->range.end -= offset; // within plain

                // Put it on the stack so it can be returned to.
                stack << *format;

                // Start a new range (copying the current one).
                _ranges << FormatRange(*format);
                format = &_ranges.back();
                format->range = Rangei(range.end - offset, range.end - offset);
            }

            // Check the escape sequence.
            char ch = styledText[range.end + 1].toLatin1();
            switch(ch)
            {
            case '.': // pop a format off the stack
                if(!stack.isEmpty())
                {
                    FormatRange prev = stack.takeLast();
                    format->sizeFactor = prev.sizeFactor;
                    format->colorIndex = prev.colorIndex;
                    format->weight = prev.weight;
                    format->style = prev.style;
                }
                else
                {
                    format->sizeFactor = 1.f;
                    format->colorIndex = OriginalColor;
                    format->weight = OriginalWeight;
                    format->style = OriginalStyle;
                }
                break;

            case '>':
                format->markIndent = true;
                break;

            case 'b':
                format->weight = Bold;
                break;

            case 'l':
                format->weight = Light;
                break;

            case 'w':
                format->weight = Normal;
                break;

            case 'r':
                format->style = Regular;
                break;

            case 'i':
                format->style = Italic;
                break;

            case 's':
                format->sizeFactor = .8f;
                break;

            case 't':
                format->sizeFactor = .75f;
                break;

            case 'n':
                format->sizeFactor = .6f;
                break;

            case 'A': // Normal color
            case 'B': // Highlight color
            case 'C': // Dimmed color
            case 'D': // Accent color
                format->colorIndex = ch - 'A';
                break;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
                if(_style)
                {
                    _style->richStyleFormat(ch - '0', format->sizeFactor, format->weight,
                                            format->style, format->colorIndex);
                }
                break;
            }

            // Advance the scanner.
            range.start = range.end + 2;
            offset += 2; // skipped chars
        }
        else
        {
            // No more styles.
            range.end = styledText.size();
            format->range.end = range.end;
            plain += styledText.substr(range);
            format->range.end -= offset;
            if(!format->range.size())
            {
                // Don't keep an empty range at the end.
                _ranges.takeLast();
            }
            break;
        }
    }

    /*
    qDebug() << "Styled text:" << styledText;
    qDebug() << "plain:" << plain;
    foreach(FormatRange const &r, _ranges)
    {
        qDebug() << r.range.asText()
                 << plain.substr(r.range)
                 << "size:" << r.sizeFactor
                 << "weight:" << r.weight
                 << "style:" << r.style
                 << "color:" << r.colorIndex;
    }*/

    return plain;
}

Font::RichFormat Font::RichFormat::subRange(Rangei const &range) const
{
    RichFormat sub(*this);

    for(int i = 0; i < sub._ranges.size(); ++i)
    {
        Rangei &sr = sub._ranges[i].range;

        sr -= range.start;
        if(sr.end < 0 || sr.start >= range.size())
        {
            // This range is outside the subrange.
            sub._ranges.removeAt(i--);
            continue;
        }
        sr.start = de::max(sr.start, 0);
        sr.end   = de::min(sr.end, range.size());
        if(!sr.size())
        {
            sub._ranges.removeAt(i--);
            continue;
        }
    }

    return sub;
}

Font::RichFormat::Iterator::Iterator(RichFormat const &f) : format(f), index(-1) {}

bool Font::RichFormat::Iterator::hasNext() const
{
    return index + 1 < format._ranges.size();
}

void Font::RichFormat::Iterator::next()
{
    index++;
    DENG2_ASSERT(index < format._ranges.size());
}

bool Font::RichFormat::Iterator::isOriginal() const
{
    return (fequal(sizeFactor(), 1.f)      &&
            weight()     == OriginalWeight &&
            style()      == OriginalStyle  &&
            colorIndex() == OriginalColor);
}

Rangei Font::RichFormat::Iterator::range() const
{
    return format._ranges[index].range;
}

float Font::RichFormat::Iterator::sizeFactor() const
{
    return format._ranges[index].sizeFactor;
}

Font::RichFormat::Weight Font::RichFormat::Iterator::weight() const
{
    return format._ranges[index].weight;
}

Font::RichFormat::Style Font::RichFormat::Iterator::style() const
{
    return format._ranges[index].style;
}

int Font::RichFormat::Iterator::colorIndex() const
{
    return format._ranges[index].colorIndex;
}

Font::RichFormat::IStyle::Color Font::RichFormat::Iterator::color() const
{
    if(format._style)
    {
        return format._style->richStyleColor(colorIndex());
    }
    // Fall back to white.
    return Vector4ub(255, 255, 255, 255);
}

bool Font::RichFormat::Iterator::markIndent() const
{
    return format._ranges[index].markIndent;
}

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

    QFont alteredFont(RichFormat::Iterator const &rich) const
    {
        if(!rich.isOriginal())
        {
            QFont mod = font;

            if(!fequal(rich.sizeFactor(), 1.f))
            {
                mod.setPointSizeF(mod.pointSizeF() * rich.sizeFactor());
            }
            if(rich.style() != RichFormat::OriginalStyle)
            {
                mod.setItalic(rich.style() == RichFormat::Italic);
            }
            if(rich.weight() != RichFormat::OriginalWeight)
            {
                mod.setWeight(rich.weight() == RichFormat::Normal? QFont::Normal :
                              rich.weight() == RichFormat::Bold?   QFont::Bold   :
                                                                   QFont::Light);
            }
            return mod;
        }
        return font;
    }

    QFontMetrics alteredMetrics(RichFormat::Iterator const &rich) const
    {
        if(!rich.isOriginal())
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

Rectanglei Font::measure(String const &textLine) const
{
    return measure(textLine, RichFormat::fromPlainText(textLine));
}

Rectanglei Font::measure(String const &textLine, RichFormat const &format) const
{
    Rectanglei bounds;
    int advance = 0;

    RichFormat::Iterator iter(format);
    while(iter.hasNext())
    {
        iter.next();

        QFontMetrics const metrics = d->alteredMetrics(iter);

        String const part = textLine.substr(iter.range());
        Rectanglei rect = Rectanglei::fromQRect(metrics.boundingRect(part));

        if(rect.height() == 0)
        {
            // It seems measuring the bounds of a Tab character produce
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

int Font::advanceWidth(String const &textLine, RichFormat const &format) const
{
    int advance = 0;
    RichFormat::Iterator iter(format);
    while(iter.hasNext())
    {
        iter.next();

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
                       RichFormat const &format,
                       Vector4ub const &foreground,
                       Vector4ub const &background) const
{
    if(textLine.isEmpty())
    {
        return QImage();
    }

    Rectanglei const bounds = measure(textLine, format);

    QColor fgColor(foreground.x, foreground.y, foreground.z, foreground.w);
    QColor bgColor(background.x, background.y, background.z, background.w);

    QImage img(QSize(bounds.width(),
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

        QFont font = d->font;

        if(iter.isOriginal())
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
        painter.drawText(advance, d->metrics->ascent(), part);
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

} // namespace de
