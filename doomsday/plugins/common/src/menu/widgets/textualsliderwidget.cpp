/** @file textualsliderwidget.cpp  UI widget for a textual slider.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "menu/widgets/textualsliderwidget.h"

#include "hu_menu.h" // Hu_MenuMergeEffectWithDrawTextFlags
#include "menu/page.h" // mnRendState

/// @todo remove me
#include "menu/widgets/cvarsliderwidget.h" // CVarSliderWidget_UpdateCVar

using namespace de;

namespace common {
namespace menu {

DENG2_PIMPL(TextualSliderWidget)
{
    void *data1 = nullptr;
    //char const *templateString = nullptr;
    char const *onethSuffix = nullptr;
    char const *nthSuffix   = nullptr;
    char const *emptyText   = nullptr;

    Instance(Public *i) : Base(i) {}

    inline bool valueIsOne(float value)
    {
        if(self.floatMode())
        {
            return INRANGE_OF(1, value, .0001f);
        }
        return (value > 0 && 1 == int(value + .5f));
    }

    char *composeTextualValue(float value, int precision, size_t bufSize, char *buf)
    {
        DENG2_ASSERT(0 != bufSize && buf);
        precision = de::max(0, precision);
        if(self.floatMode() && !valueIsOne(value))
        {
            dd_snprintf(buf, bufSize, "%.*f", precision, value);
        }
        else
        {
            dd_snprintf(buf, bufSize, "%.*i", precision, int(value));
        }
        return buf;
    }

    char *composeValueString(float value, float defaultValue, int precision,
                             size_t bufSize, char *buf)
    {
        DENG2_ASSERT(0 != bufSize && buf);

        //dd_bool const haveTemplateString = (templateString && templateString[0]);
        bool const haveOnethSuffix = (onethSuffix && onethSuffix[0]);
        bool const haveNthSuffix   = (nthSuffix && nthSuffix[0]);
        bool const haveEmptyText   = (emptyText && emptyText[0]);
        char const *suffix = 0;
        char textualValue[11];

        // Is the default-value-string in use?
        if(haveEmptyText && INRANGE_OF(value, defaultValue, .0001f))
        {
            strncpy(buf, emptyText, bufSize);
            buf[bufSize] = '\0';
            return buf;
        }

        composeTextualValue(value, precision, 10, textualValue);

        // Choose a suffix.
        if(haveOnethSuffix && valueIsOne(value))
        {
            suffix = onethSuffix;
        }
        else if(haveNthSuffix)
        {
            suffix = nthSuffix;
        }
        else
        {
            suffix = "";
        }

#if 0
        // Are we substituting the textual value into a template?
        if(haveTemplateString)
        {
            char const *c, *beginSubstring = 0;
            ddstring_t compStr;

            // Reserve a conservative amount of storage, we assume the caller
            // knows best and take the value given as the output buffer size.
            Str_Init(&compStr);
            Str_Reserve(&compStr, bufSize);

            // Composite the final string.
            beginSubstring = templateString;
            for(c = beginSubstring; *c; c++)
            {
                if(c[0] == '%' && c[1] == '1')
                {
                    Str_PartAppend(&compStr, beginSubstring, 0, c - beginSubstring);
                    Str_Appendf(&compStr, "%s%s", textualValue, suffix);
                    // Next substring will begin from here.
                    beginSubstring = c + 2;
                    c += 1;
                }
            }
            // Anything remaining?
            if(beginSubstring != c)
                Str_Append(&compStr, beginSubstring);

            strncpy(buf, Str_Text(&compStr), bufSize);
            buf[bufSize] = '\0';
            Str_Free(&compStr);
        }
        else
#endif
        {
            dd_snprintf(buf, bufSize, "%s%s", textualValue, suffix);
        }

        return buf;
    }
};

TextualSliderWidget::TextualSliderWidget(float min, float max, float step, bool floatMode)
    : SliderWidget(min, max, step, floatMode)
    , d(new Instance(this))
{
    Widget::_pageColorIdx = MENU_COLOR3;
    Widget::actions[Widget::MNA_MODIFIED].callback = CVarSliderWidget_UpdateCVar;
    Widget::actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
}

TextualSliderWidget::~TextualSliderWidget()
{}

void TextualSliderWidget::draw(Point2Raw const *origin)
{
    DENG2_ASSERT(origin != 0);

    float const val = de::clamp(min(), value(), max());
    char textualValue[41];
    char const *str = d->composeValueString(val, 0, 0, 40, textualValue);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(origin->x, origin->y, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(mnRendState->textFonts[_pageFontIdx]);
    FR_SetColorAndAlphav(mnRendState->textColors[_pageColorIdx]);
    FR_DrawTextXY3(str, 0, 0, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(-origin->x, -origin->y, 0);
}

void TextualSliderWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);

    fontid_t const font = page->predefinedFont(mn_page_fontid_t(_pageFontIdx));
    float const val = de::clamp(min(), value(), max());
    char textualValue[41];
    char const *str = d->composeValueString(val, 0, 0, 40, textualValue);

    FR_SetFont(font);

    Size2Raw size; FR_TextSize(&size, str);

    Rect_SetWidthHeight(_geometry, size.width, size.height);
}

void TextualSliderWidget::setEmptyText(char const *newEmptyText)
{
    d->emptyText = newEmptyText;
}

char const *TextualSliderWidget::emptyText() const
{
    return d->emptyText;
}

void TextualSliderWidget::setOnethSuffix(char const *newOnethSuffix)
{
    d->onethSuffix = newOnethSuffix;
}

char const *TextualSliderWidget::onethSuffix() const
{
    return d->onethSuffix;
}

void TextualSliderWidget::setNthSuffix(char const *newNthSuffix)
{
    d->nthSuffix = newNthSuffix;
}

char const *TextualSliderWidget::nthSuffix() const
{
    return d->nthSuffix;
}

} // namespace menu
} // namespace common
