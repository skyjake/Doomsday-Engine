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
#include <de/EscapeParser>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>

#if defined(WIN32) || defined(MACOSX)
#  define LIBGUI_ACCURATE_TEXT_BOUNDS
#endif

namespace de {

DENG2_PIMPL_NOREF(Font::RichFormat),
DENG2_OBSERVES(EscapeParser, PlainText),
DENG2_OBSERVES(EscapeParser, EscapeSequence)
{
    IStyle const *style;

    struct Format
    {
        float sizeFactor;
        Weight weight;
        Style style;
        int colorIndex;
        bool markIndent;
        bool resetIndent;
        int tabStop;

        Format() : sizeFactor(1.f), weight(OriginalWeight),
            style(OriginalStyle), colorIndex(-1), markIndent(false), resetIndent(false),
            tabStop(-1 /* untabbed */) {}
    };

    struct FormatRange
    {
        Rangei range;
        Format format;

        FormatRange(Rangei const &r = Rangei(), Format const &frm = Format())
            : range(r), format(frm) {}
    };

    typedef QList<FormatRange> Ranges;
    Ranges ranges;

    /// Tab stops are only applicable on the first line of a set of wrapped
    /// lines. Subsequent lines use the latest accessed tab stop as the indent.
    TabStops tabs;

    EscapeParser esc;
    QList<Format> stack;
    int plainPos;

    Instance() : style(0) {}

    Instance(IStyle const &style) : style(&style) {}

    Instance(Instance const &other)
        : style(other.style), ranges(other.ranges), tabs(other.tabs)
    {}

    void handlePlainText(Rangei const &range)
    {       
        Rangei plainRange(plainPos, plainPos + range.size());
        plainPos += range.size();

        // Append a formatted range using the stack's current format.
        ranges << Instance::FormatRange(plainRange, stack.last());

        // Properties that span a single range only.
        stack.last().markIndent = stack.last().resetIndent = false;
    }

    void handleEscapeSequence(Rangei const &range)
    {
        // Save the previous format on the stack.
        stack << Instance::Format(stack.last());

        String const code = esc.originalText().substr(range);

        // Check the escape sequence.
        char ch = code[0].toLatin1();

        switch(ch)
        {
        case '(':
            // Sequence of tab stops effective in the entire content.
            tabs.clear();
            for(int i = 1; i < code.size() - 1; ++i)
            {
                tabs << (code[i].toLatin1() - 'a' + 1);
            }
            break;

        case '.': // pop a format off the stack
            stack.removeLast(); // ignore the one just added
            if(stack.size() > 1)
            {
                Format form = stack.takeLast();
                stack.last().tabStop = form.tabStop; // Retain tab stop.
                stack.last().markIndent = form.markIndent;
                //stack.last().resetIndent = form.resetIndent;
            }
            break;

        case '>':
            stack.last().markIndent = true;
            break;

        case '<':
            stack.last().resetIndent = true;

            // Insert an empty range for reseting the indent.
            handlePlainText(Rangei(0, 0));
            break;

        case '\t':
            stack.last().tabStop++;
            break;

        case 'T':
            stack.last().tabStop = de::max(-1, code[1].toLatin1() - 'a');
            break;

        case 'b':
            stack.last().weight = Bold;
            break;

        case 'l':
            stack.last().weight = Light;
            break;

        case 'w':
            stack.last().weight = Normal;
            break;

        case 'r':
            stack.last().style = Regular;
            break;

        case 'i':
            stack.last().style = Italic;
            break;

        case 'm':
            stack.last().style = Monospace;
            break;

        case 's':
            stack.last().sizeFactor = .8f;
            break;

        case 't':
            stack.last().sizeFactor = .75f;
            break;

        case 'n':
            stack.last().sizeFactor = .6f;
            break;

        case 'A': // Normal color
        case 'B': // Highlight color
        case 'C': // Dimmed color
        case 'D': // Accent color
        case 'E': // Dim Accent color
            stack.last().colorIndex = ch - 'A';
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
            if(style)
            {
                style->richStyleFormat(ch - '0',
                                       stack.last().sizeFactor,
                                       stack.last().weight,
                                       stack.last().style,
                                       stack.last().colorIndex);
            }
            break;
        }
    }
};

Font::RichFormat::RichFormat() : d(new RichFormat::Instance)
{}

Font::RichFormat::RichFormat(IStyle const &style) : d(new RichFormat::Instance(style))
{}

Font::RichFormat::RichFormat(RichFormat const &other) : d(new RichFormat::Instance(*other.d))
{}

Font::RichFormat &Font::RichFormat::operator = (RichFormat const &other)
{
    d.reset(new RichFormat::Instance(*other.d));
    return *this;
}

void Font::RichFormat::clear()
{
    d->ranges.clear();
    d->tabs.clear();
    d->stack.clear();
    d->stack << Instance::Format();
    d->plainPos = 0;
}

void Font::RichFormat::setStyle(IStyle const &style)
{
    d->style = &style;
}

bool Font::RichFormat::hasStyle() const
{
    return d->style != 0;
}

Font::RichFormat::IStyle const &Font::RichFormat::style() const
{
    return *d->style;
}

Font::RichFormat Font::RichFormat::fromPlainText(String const &plainText)
{
    Instance::FormatRange all;
    all.range = Rangei(0, plainText.size());
    RichFormat form;
    form.d->ranges << all;
    return form;
}

String Font::RichFormat::initFromStyledText(String const &styledText)
{
    clear();

    d->esc.audienceForEscapeSequence += d;
    d->esc.audienceForPlainText += d;

    d->esc.parse(styledText);

#if 0
    qDebug() << "Styled text:" << styledText;
    qDebug() << "plain:" << d->esc.plainText();
    foreach(Instance::FormatRange const &r, d->ranges)
    {
        qDebug() << r.range.asText()
                 << d->esc.plainText().substr(r.range)
                 << "size:" << r.format.sizeFactor
                 << "weight:" << r.format.weight
                 << "style:" << r.format.style
                 << "color:" << r.format.colorIndex
                 << "tab:" << r.format.tabStop
                 << "indent (m/r):" << r.format.markIndent << r.format.resetIndent;
    }
    qDebug() << "Tabs:" << d->tabs.size();
    foreach(int i, d->tabs)
    {
        qDebug() << i;
    }
#endif

    return d->esc.plainText();
}

Font::RichFormatRef Font::RichFormat::subRange(Rangei const &range) const
{
    return RichFormatRef(*this, range);
}

Font::TabStops const &Font::RichFormat::tabStops() const
{
    return d->tabs;
}

int Font::RichFormat::tabStopXWidth(int stop) const
{
    if(stop < 0 || d->tabs.isEmpty()) return 0;

    DENG2_ASSERT(stop < 50);

    int x = 0;
    for(int i = 0; i <= stop; ++i)
    {
        if(i >= d->tabs.size())
            x += d->tabs.last();
        else
            x += d->tabs[i];
    }
    return x;
}

Font::RichFormat::Ref::Ref(Ref const &ref)
    : _ref(ref.format()), _span(ref._span), _indices(ref._indices)
{}

Font::RichFormat::Ref::Ref(Ref const &ref, Rangei const &subSpan)
    : _ref(ref.format()), _span(subSpan + ref._span.start)
{
    updateIndices();
}

Font::RichFormat::Ref::Ref(RichFormat const &richFormat)
    : _ref(richFormat), _indices(0, richFormat.d->ranges.size())
{
    if(!richFormat.d->ranges.isEmpty())
    {
        _span = Rangei(0, richFormat.d->ranges.last().range.end);
    }
}

Font::RichFormat::Ref::Ref(RichFormat const &richFormat, Rangei const &subSpan)
    : _ref(richFormat), _span(subSpan)
{
    updateIndices();
}

Font::RichFormat const &Font::RichFormat::Ref::format() const
{
    return _ref;
}

int Font::RichFormat::Ref::rangeCount() const
{
    return _indices.size();
}

Rangei Font::RichFormat::Ref::range(int index) const
{
    Rangei r = _ref.d->ranges.at(_indices.start + index).range;

    if(index == 0)
    {
        // Clip the beginning.
        r.start = de::max(r.start, _span.start);
    }
    if(index == rangeCount() - 1)
    {
        // Clip the end in the last range.
        r.end = de::min(r.end, _span.end);
    }

    DENG2_ASSERT(r.start >= _span.start);
    DENG2_ASSERT(r.end <= _span.end);
    DENG2_ASSERT(r.start <= r.end);

    // Make sure it's relative to the start of the subspan.
    return r - _span.start;
}

Font::RichFormat::Ref Font::RichFormat::Ref::subRef(Rangei const &subSpan) const
{
    return Ref(*this, subSpan);
}

void Font::RichFormat::Ref::updateIndices()
{
    _indices = Rangei(0, 0);

    Instance::Ranges const &ranges = format().d->ranges;

    int i = 0;
    for(; i < ranges.size(); ++i)
    {
        Rangei const &r = ranges.at(i).range;
        if(r.end > _span.start)
        {
            _indices.start = i;
            _indices.end = i + 1;
            break;
        }
    }
    for(++i; i < ranges.size(); ++i, ++_indices.end)
    {
        // Empty ranges are accepted at the end of the span.
        Rangei const &r = ranges.at(i).range;
        if(( r.isEmpty() && r.start >  _span.end) ||
           (!r.isEmpty() && r.start >= _span.end))
            break;
    }

    DENG2_ASSERT(_indices.start <= _indices.end);
}

Font::RichFormat::Iterator::Iterator(Ref const &f) : format(f), index(-1) {}

int Font::RichFormat::Iterator::size() const
{
    return format.rangeCount();
}

bool Font::RichFormat::Iterator::hasNext() const
{
    return index + 1 < size();
}

void Font::RichFormat::Iterator::next()
{
    index++;
    DENG2_ASSERT(index < size());
}

bool Font::RichFormat::Iterator::isDefault() const
{
    return (fequal(sizeFactor(), 1.f)      &&
            weight()     == OriginalWeight &&
            style()      == OriginalStyle  &&
            colorIndex() == OriginalColor);
}

Rangei Font::RichFormat::Iterator::range() const
{
    return format.range(index);
}

#define REF_RANGE_AT(Idx) format.format().d->ranges.at(format.rangeIndices().start + Idx)

float Font::RichFormat::Iterator::sizeFactor() const
{
    return REF_RANGE_AT(index).format.sizeFactor;
}

Font::RichFormat::Weight Font::RichFormat::Iterator::weight() const
{
    return REF_RANGE_AT(index).format.weight;
}

Font::RichFormat::Style Font::RichFormat::Iterator::style() const
{
    return REF_RANGE_AT(index).format.style;
}

int Font::RichFormat::Iterator::colorIndex() const
{
    return REF_RANGE_AT(index).format.colorIndex;
}

Font::RichFormat::IStyle::Color Font::RichFormat::Iterator::color() const
{
    if(format.format().d->style)
    {
        return format.format().d->style->richStyleColor(colorIndex());
    }
    // Fall back to white.
    return Vector4ub(255, 255, 255, 255);
}

bool Font::RichFormat::Iterator::markIndent() const
{
    return REF_RANGE_AT(index).format.markIndent;
}

bool Font::RichFormat::Iterator::resetIndent() const
{
    return REF_RANGE_AT(index).format.resetIndent;
}

int Font::RichFormat::Iterator::tabStop() const
{
    return REF_RANGE_AT(index).format.tabStop;
}

bool Font::RichFormat::Iterator::isTabless() const
{
    return tabStop() < 0;
}

#undef REF_RANGE_AT

DENG2_PIMPL(Font)
{
    QFont font;
    QScopedPointer<QFontMetrics> metrics;
    ConstantRule *heightRule;
    ConstantRule *ascentRule;
    ConstantRule *descentRule;
    ConstantRule *lineSpacingRule;
    int ascent;

    Instance(Public *i) : Base(i), ascent(0)
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

    QFont alteredFont(RichFormat::Iterator const &rich) const
    {
        if(!rich.isDefault())
        {
            QFont mod = font;

            if(!fequal(rich.sizeFactor(), 1.f))
            {
                mod.setPointSizeF(mod.pointSizeF() * rich.sizeFactor());
            }

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
