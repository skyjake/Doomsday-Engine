/** @file cvartextualsliderwidget.cpp  UI widget for a textual slider.
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
#include "menu/widgets/cvartextualsliderwidget.h"

#include "hu_menu.h" // Hu_MenuMergeEffectWithDrawTextFlags
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

DENG2_PIMPL(CVarTextualSliderWidget)
{
    //String templateString;
    String onethSuffix;
    String nthSuffix;
    String emptyText;

    Instance(Public *i) : Base(i) {}

    inline bool valueIsOne(float value)
    {
        if(self.floatMode())
        {
            return INRANGE_OF(1, value, .0001f);
        }
        return (value > 0 && 1 == int(value + .5f));
    }

    String chooseSuffix(float value)
    {
        if(!onethSuffix.isEmpty() && valueIsOne(value))
        {
            return onethSuffix;
        }
        if(!nthSuffix.isEmpty())
        {
            return nthSuffix;
        }
        return "";
    }

    String composeTextualValue(float value, int precision = 0)
    {
        if(self.floatMode() && !valueIsOne(value))
        {
            return String::number(value, 'f', de::max(0, precision));
        }
        else
        {
            return String::number(int(value));
        }
    }

    String valueAsText(float emptyValue = 0, int precision = 0)
    {
        float const value = de::clamp(self.min(), self.value(), self.max()); /// @todo clamp necessary?

        // Is the empty-value-string in use?
        if(!emptyText.isEmpty() && INRANGE_OF(value, emptyValue, .0001f))
        {
            return emptyText;
        }

        String const suffix      = chooseSuffix(value);
        String const valueAsText = composeTextualValue(value, precision);

#if 0
        // Are we substituting the textual value into a template?
        if(!templateString.isEmpty())
        {
            // Reserve a conservative amount of storage, we assume the caller
            // knows best and take the value given as the output buffer size.
            AutoStr *compStr = AutoStr_NewStd();
            Str_Reserve(compStr, bufSize);

            // Composite the final string.
            char const *c;
            char *beginSubstring = templateString;
            for(c = beginSubstring; *c; c++)
            {
                if(c[0] == '%' && c[1] == '1')
                {
                    Str_PartAppend(compStr, beginSubstring, 0, c - beginSubstring);
                    Str_Appendf(compStr, "%s%s", textualValue, suffix);
                    // Next substring will begin from here.
                    beginSubstring = c + 2;
                    c += 1;
                }
            }
            // Anything remaining?
            if(beginSubstring != c)
                Str_Append(compStr, beginSubstring);

            return String(Str_Text(compStr));
        }
        else
#endif
        {
            return valueAsText + suffix;
        }
    }
};

CVarTextualSliderWidget::CVarTextualSliderWidget(char const *cvarPath, float min, float max,
                                                 float step, bool floatMode)
    : CVarSliderWidget(cvarPath, min, max, step, floatMode)
    , d(new Instance(this))
{
    setColor(MENU_COLOR3);
}

CVarTextualSliderWidget::~CVarTextualSliderWidget()
{}

void CVarTextualSliderWidget::draw(Point2Raw const *origin)
{
    DENG2_ASSERT(origin != 0);

    String const valueAsText = d->valueAsText();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(origin->x, origin->y, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(mnRendState->textFonts[font()]);
    FR_SetColorAndAlphav(mnRendState->textColors[color()]);
    FR_DrawTextXY3(valueAsText.toUtf8().constData(), 0, 0, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(-origin->x, -origin->y, 0);
}

void CVarTextualSliderWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);

    String const valueAsText = d->valueAsText();

    FR_SetFont(page->predefinedFont(mn_page_fontid_t(font())));
    Size2Raw size; FR_TextSize(&size, valueAsText.toUtf8().constData());

    Rect_SetWidthHeight(geometry(), size.width, size.height);
}

CVarTextualSliderWidget &CVarTextualSliderWidget::setEmptyText(String const &newEmptyText)
{
    d->emptyText = newEmptyText;
    return *this;
}

String CVarTextualSliderWidget::emptyText() const
{
    return d->emptyText;
}

CVarTextualSliderWidget &CVarTextualSliderWidget::setOnethSuffix(String const &newOnethSuffix)
{
    d->onethSuffix = newOnethSuffix;
    return *this;
}

String CVarTextualSliderWidget::onethSuffix() const
{
    return d->onethSuffix;
}

CVarTextualSliderWidget &CVarTextualSliderWidget::setNthSuffix(String const &newNthSuffix)
{
    d->nthSuffix = newNthSuffix;
    return *this;
}

String CVarTextualSliderWidget::nthSuffix() const
{
    return d->nthSuffix;
}

} // namespace menu
} // namespace common
