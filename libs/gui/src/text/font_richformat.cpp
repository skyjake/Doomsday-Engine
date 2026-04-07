/** @file font_richformat.cpp  Rich formatting instructions for text.
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

#include "de/font.h"

#include <de/escapeparser.h>
#include <de/cstring.h>

namespace de {

DE_PIMPL_NOREF(Font::RichFormat)
, DE_OBSERVES(EscapeParser, PlainText)
, DE_OBSERVES(EscapeParser, EscapeSequence)
{
    struct Format
    {
        float  sizeFactor  = 1.f;
        Weight weight      = OriginalWeight;
        Style  style       = OriginalStyle;
        int    colorIndex  = -1;
        bool   markIndent  = false;
        bool   resetIndent = false;
        int    tabStop     = -1; // untabbed

        Format() {}
    };
    struct FormatRange
    {
        CString range;
        Format  format;

        FormatRange(const CString &r = {}, const Format &frm = {})
            : range(r)
            , format(frm)
        {}
    };
    typedef List<FormatRange> Ranges;

    const IStyle *style;
    Ranges ranges;

    /// Tab stops are only applicable on the first line of a set of wrapped
    /// lines. Subsequent lines use the latest accessed tab stop as the indent.
    TabStops tabs;

    EscapeParser esc;
    List<Format> stack;
//    int plainPos;

    Impl() : style(nullptr) {}

    Impl(const IStyle &style) : style(&style) {}

    Impl(const Impl &other)
        : style(other.style)
        , ranges(other.ranges)
        , tabs(other.tabs)
    {}

    CString fullRange() const
    {
        if (ranges)
        {
            return CString(ranges.front().range.begin(), ranges.back().range.end());
        }
        return {};
    }

    void handlePlainText(const CString &range)
    {
        DE_ASSERT(range.ptr());
        
        // Append a formatted range using the stack's current format.
        ranges.emplace_back(range, stack.last());

        // Properties that span a single range only.
        stack.last().markIndent = stack.last().resetIndent = false;
    }

    void handleEscapeSequence(const CString &range)
    {
        // Save the previous format on the stack.
        stack.emplace_back(stack.last());

        mb_iterator iter = range.begin();

        // Check the escape sequence.
        Char ch = *iter++;

        switch (ch)
        {
        case '(':
            // Sequence of tab stops effective in the entire content.
            tabs.clear();
            //for (int i = 1; i < code.size() - 1; ++i)
            for (auto end = range.end() - 1; iter != end; ++iter)
            {
                tabs << ((*iter).delta('a') + 1);
            }
            break;

        case '.': // pop a format off the stack
            stack.removeLast(); // ignore the one just added
            if (stack.size() > 1)
            {
                Format form = stack.takeLast();
                stack.last().tabStop = form.tabStop; // Retain tab stop.
                stack.last().markIndent = form.markIndent;
            }
            break;

        case '>':
            stack.last().markIndent = true;
            // Insert an empty range for marking the indent.
            handlePlainText(CString(iter, iter));
            break;

        case '<':
            stack.last().resetIndent = true;
            // Insert an empty range for reseting the indent.
            handlePlainText(CString(iter, iter));
            break;

        case '\t':
            stack.last().tabStop++;
            break;

        case 'T':
            stack.last().tabStop = de::max(-1, int(*iter - 'a'));
            // Note: _E(T`): tabStop -1, i.e., switch to untabbed
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
        case 'F': // Alternative Accent color
            stack.last().colorIndex = ch.delta('A');
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
            if (style)
            {
                style->richStyleFormat(ch.delta('0'),
                                       stack.last().sizeFactor,
                                       stack.last().weight,
                                       stack.last().style,
                                       stack.last().colorIndex);
            }
            break;
        }
    }
};

Font::RichFormat::RichFormat() : d(new RichFormat::Impl)
{}

Font::RichFormat::RichFormat(const IStyle &style) : d(new RichFormat::Impl(style))
{}

Font::RichFormat::RichFormat(const RichFormat &other) : d(new RichFormat::Impl(*other.d))
{}

Font::RichFormat &Font::RichFormat::operator=(const RichFormat &other)
{
    d.reset(new RichFormat::Impl(*other.d));
    return *this;
}

void Font::RichFormat::clear()
{
    d->ranges.clear();
    d->tabs.clear();
    d->stack.clear();
    d->stack.emplace_back();
}

void Font::RichFormat::setStyle(const IStyle &style)
{
    d->style = &style;
}

bool Font::RichFormat::hasStyle() const
{
    return d->style != nullptr;
}

const Font::RichFormat::IStyle &Font::RichFormat::style() const
{
    return *d->style;
}

Font::RichFormat Font::RichFormat::fromPlainText(const String &plainText)
{
    Impl::FormatRange all;
    all.range = plainText;
    RichFormat form;
    form.d->ranges << all;
    return form;
}

void Font::RichFormat::initFromStyledText(const String &styledText)
{
    clear();

    d->esc.audienceForEscapeSequence() += d;
    d->esc.audienceForPlainText()      += d;
    d->esc.parse(styledText);

#if 0
    debug("[Font::RichFormat] Styled text: [%s]", styledText.c_str());
    debug("  plain:[%s]", d->esc.plainText().c_str());
    for (const Impl::FormatRange &r : d->ranges)
    {
        debug("    [%s](%zu) size:%.1f weight:%i style:%i color:%i tab:%i indent:mark=%s,reset=%s",
              r.range.toString().c_str(),
              r.range.size(),
              r.format.sizeFactor,
              r.format.weight,
              r.format.style,
              r.format.colorIndex,
              r.format.tabStop,
              DE_BOOL_YESNO(r.format.markIndent),
              DE_BOOL_YESNO(r.format.resetIndent));
    }
    debug("  tabs:%i", d->tabs.size());
    for (int i : d->tabs)
    {
        debug("    %i", i);
    }
#endif

//    return d->esc.plainText();
}

Font::RichFormatRef Font::RichFormat::subRange(const CString &range) const
{
    return RichFormatRef(*this, range);
}

const Font::TabStops &Font::RichFormat::tabStops() const
{
    return d->tabs;
}

int Font::RichFormat::tabStopXWidth(int stop) const
{
    if (stop < 0 || d->tabs.isEmpty()) return 0;

    DE_ASSERT(stop < 50);

    int x = 0;
    for (int i = 0; i <= stop; ++i)
    {
        if (i >= d->tabs.sizei())
            x += d->tabs.last();
        else
            x += d->tabs[i];
    }
    return x;
}

//------------------------------------------------------------------------------------------------

Font::RichFormat::Ref::Ref(const Ref &ref)
    : _ref(ref.format()), _span(ref._span), _indices(ref._indices)
{
    DE_ASSERT(_span.ptr());
}

Font::RichFormat::Ref::Ref(const Ref &ref, const CString &subSpan)
    : _ref(ref.format()), _span(subSpan)
{
    DE_ASSERT(_span.ptr());
    updateIndices();
}

Font::RichFormat::Ref::Ref(const RichFormat &richFormat)
    : _ref(richFormat)
    , _span(richFormat.d->fullRange())
    , _indices(0, richFormat.d->ranges.sizei())
{
    DE_ASSERT(_span.ptr());
}

Font::RichFormat::Ref::Ref(const RichFormat &richFormat, const CString &subSpan)
    : _ref(richFormat), _span(subSpan)
{
    DE_ASSERT(_span.ptr());
    updateIndices();
}

const Font::RichFormat &Font::RichFormat::Ref::format() const
{
    return _ref;
}

int Font::RichFormat::Ref::rangeCount() const
{
    return _indices.size();
}

CString Font::RichFormat::Ref::range(int index) const
{
    DE_ASSERT(_span.ptr() <= _span.endPtr());
    
    const CString &r = _ref.d->ranges.at(_indices.start + index).range;
    const char *start = r.ptr();
    const char *end   = r.endPtr();
    if (index == 0)
    {
        // Clip the beginning.
        //r.setStart(de::max(r.ptr(), _span.ptr()));
        start = de::max(start, _span.ptr());
        end   = de::max(end,   _span.ptr());
    }
    if (index == rangeCount() - 1)
    {
        // Clip the end in the last range.
        //r.setEnd(de::max(r.ptr(), de::min(r.endPtr(), _span.endPtr())));
        start = de::min(start, _span.endPtr());
        end   = de::min(end, _span.endPtr());
    }

    DE_ASSERT(start);
    DE_ASSERT(end);
    DE_ASSERT(start >= _span.ptr());
    DE_ASSERT(end <= _span.endPtr());
    DE_ASSERT(start <= end);
    {
        const char *okPtr;
        DE_ASSERT((mb_iterator(start).decode(&okPtr), okPtr != nullptr));
    }

    return {start, end};
}

Font::RichFormat::Ref Font::RichFormat::Ref::subRef(const CString &subSpan) const
{
    return Ref(*this, subSpan);
}

void Font::RichFormat::Ref::updateIndices()
{
    const auto &ranges = format().d->ranges;
    _indices           = Rangei(0, 0);

    int i = 0;
    for (; i < ranges.sizei(); ++i)
    {
        const CString &r = ranges.at(i).range;
        if (r.endPtr() > _span.ptr())
        {
            _indices.start = i;
            _indices.end = i + 1;
            break;
        }
    }
    for (++i; i < ranges.sizei(); ++i, ++_indices.end)
    {
        // Empty ranges are accepted at the end of the span.
        const CString &r = ranges.at(i).range;
        if (( r.isEmpty() && r.ptr() >  _span.endPtr()) ||
            (!r.isEmpty() && r.ptr() >= _span.endPtr())) break;
    }

    DE_ASSERT(_indices.start <= _indices.end);
}

Font::RichFormat::Iterator::Iterator(const Ref &f) : format(f), index(-1) {}

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
    DE_ASSERT(index < size());
}

bool Font::RichFormat::Iterator::isDefault() const
{
    return (fequal(sizeFactor(), 1.f)      &&
            weight()     == OriginalWeight &&
            style()      == OriginalStyle  &&
            colorIndex() == OriginalColor);
}

CString Font::RichFormat::Iterator::range() const
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
    if (format.format().d->style)
    {
        return format.format().d->style->richStyleColor(colorIndex());
    }
    // Fall back to white.
    return IStyle::Color(255, 255, 255, 255);
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

} // namespace de
