/** @file labelwidget.cpp  Text label widget.
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
#include "menu/widgets/labelwidget.h"

#include "hu_menu.h"   // Hu_MenuMergeEffectWithDrawTextFlags
#include "hu_stuff.h"
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

DE_PIMPL_NOREF(LabelWidget)
{
    String text;
    patchid_t *patch = nullptr;  ///< Used instead of text if Patch Replacement is in use.
    int flags = 0;               ///< @ref mnTextFlags
};

LabelWidget::LabelWidget(const String &text, patchid_t *patch)
    : Widget()
    , d(new Impl)
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR1);
    setFlags(NoFocus); // never focusable.
    setText(text);
    setPatch(patch);
}

LabelWidget::~LabelWidget()
{}

void LabelWidget::draw() const
{
    fontid_t fontId           = mnRendState->textFonts[font()];
    const Vec4f &textColor = mnRendState->textColors[color()];
//    float t = (isFocused()? 1 : 0);

//    // Flash if focused.
//    if(isFocused() && cfg.common.menuTextFlashSpeed > 0)
//    {
//        const float speed = cfg.common.menuTextFlashSpeed / 2.f;
//        t = (1 + sin(page().timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
//    }

//    const Vector4f color = de::lerp(textColor, Vector4f(Vector3f(cfg.common.menuTextFlashColor), textColor.w), t);

    const Vec4f color = selectionFlashColor(textColor);

    const float fadeout = scrollingFadeout();
    if (fadeout < .001f) return;

    DGL_Color4f(1, 1, 1, color.w * fadeout);
    FR_SetFont(fontId);
    FR_SetColorAndAlpha(color.x, color.y, color.z, color.w * fadeout);

    if (d->patch)
    {
        String replacement;
        if(!(d->flags & MNTEXT_NO_ALTTEXT))
        {
            replacement = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.menuPatchReplaceMode), *d->patch, d->text);
        }

        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch(*d->patch, replacement, geometry().topLeft, ALIGN_TOPLEFT, 0, Hu_MenuMergeEffectWithDrawTextFlags(0));
        DGL_Disable(DGL_TEXTURE_2D);

        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawTextXY3(d->text, geometry().topLeft.x, geometry().topLeft.y, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));
    DGL_Disable(DGL_TEXTURE_2D);
}

void LabelWidget::updateGeometry()
{
    /// @todo What if patch replacement is disabled?
    if(d->patch)
    {
        patchinfo_t info;
        R_GetPatchInfo(*d->patch, &info);
        geometry().setSize(Vec2ui(info.geometry.size.width, info.geometry.size.height));
        return;
    }

    FR_PushAttrib();
    Size2Raw size;
    FR_SetFont(page().predefinedFont(mn_page_fontid_t(font())));
    FR_TextSize(&size, d->text);
    geometry().setSize(Vec2ui(size.width, size.height));
    FR_PopAttrib();
}

LabelWidget &LabelWidget::setPatch(patchid_t *newPatch)
{
    d->patch = newPatch;
    return *this;
}

LabelWidget &LabelWidget::setText(const String &text)
{
    d->text = labelText(text);
    return *this;
}

} // namespace menu
} // namespace common
