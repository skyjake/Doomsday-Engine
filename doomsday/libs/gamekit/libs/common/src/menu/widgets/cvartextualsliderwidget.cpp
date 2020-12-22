/** @file cvartextualsliderwidget.cpp  UI widget for a textual slider.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

DE_PIMPL(CVarTextualSliderWidget)
{
    //String templateString;
    String onethSuffix;
    String nthSuffix;
    String emptyText;

    Impl(Public *i) : Base(i) {}

    inline bool valueIsOne(float value)
    {
        if(self().floatMode())
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
        if(self().floatMode() && !valueIsOne(value))
        {
            return String::asText(value, de::max(0, precision));
        }
        else
        {
            return String::asText(int(value));
        }
    }

    String valueAsText(float emptyValue = 0, int precision = 0)
    {
        const float value = de::clamp(self().min(), self().value(), self().max()); /// @todo clamp necessary?

        // Is the empty-value-string in use?
        if(!emptyText.isEmpty() && INRANGE_OF(value, emptyValue, .0001f))
        {
            return emptyText;
        }

        const String suffix      = chooseSuffix(value);
        const String valueAsText = composeTextualValue(value, precision);

#if 0
        // Are we substituting the textual value into a template?
        if(!templateString.isEmpty())
        {
            // Reserve a conservative amount of storage, we assume the caller
            // knows best and take the value given as the output buffer size.
            AutoStr *compStr = AutoStr_NewStd();
            Str_Reserve(compStr, bufSize);

            // Composite the final string.
            const char *c;
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

CVarTextualSliderWidget::CVarTextualSliderWidget(const char *cvarPath, float min, float max,
                                                 float step, bool floatMode)
    : CVarSliderWidget(cvarPath, min, max, step, floatMode)
    , d(new Impl(this))
{
    setColor(MENU_COLOR3);
}

CVarTextualSliderWidget::~CVarTextualSliderWidget()
{}

void CVarTextualSliderWidget::draw() const
{
    const Vec2i &origin    = geometry().topLeft;
    const String valueAsText  = d->valueAsText();
    const Vec4f &textColor = mnRendState->textColors[color()];

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(origin.x, origin.y, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(mnRendState->textFonts[font()]);
    FR_SetColorAndAlpha(textColor.x, textColor.y, textColor.z, textColor.w * scrollingFadeout());
    FR_DrawTextXY3(valueAsText, 0, 0, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(-origin.x, -origin.y, 0);
}

void CVarTextualSliderWidget::updateGeometry()
{
    const String valueAsText = d->valueAsText();

    FR_PushAttrib();
    FR_SetFont(page().predefinedFont(mn_page_fontid_t(font())));
    Size2Raw size; FR_TextSize(&size, valueAsText);
    geometry().setSize(Vec2ui(size.width, size.height));
    FR_PopAttrib();
}

CVarTextualSliderWidget &CVarTextualSliderWidget::setEmptyText(const String &newEmptyText)
{
    d->emptyText = newEmptyText;
    return *this;
}

String CVarTextualSliderWidget::emptyText() const
{
    return d->emptyText;
}

CVarTextualSliderWidget &CVarTextualSliderWidget::setOnethSuffix(const String &newOnethSuffix)
{
    d->onethSuffix = newOnethSuffix;
    return *this;
}

String CVarTextualSliderWidget::onethSuffix() const
{
    return d->onethSuffix;
}

CVarTextualSliderWidget &CVarTextualSliderWidget::setNthSuffix(const String &newNthSuffix)
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
