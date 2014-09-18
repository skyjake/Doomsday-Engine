/** @file labelwidget.cpp  Text label widget.
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
#include "menu/widgets/labelwidget.h"

#include "hu_menu.h" // Hu_MenuMergeEffectWithDrawTextFlags
#include "hu_lib.h" // lerpColor
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

DENG2_PIMPL_NOREF(LabelWidget)
{
    String text;
    patchid_t *patch = nullptr;  ///< Used instead of text if Patch Replacement is in use.
    int flags = 0;               ///< @ref mnTextFlags
};

LabelWidget::LabelWidget(String const &text, patchid_t *patch)
    : Widget()
    , d(new Instance)
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR1);
    setFlags(NoFocus); // never focusable.
    setText(text);
    setPatch(patch);
}

LabelWidget::~LabelWidget()
{}

void LabelWidget::draw(Point2Raw const *origin)
{
    fontid_t fontId = mnRendState->textFonts[font()];
    float textColor[4], t = (isFocused()? 1 : 0);

    // Flash if focused.
    if(isFocused() && cfg.menuTextFlashSpeed > 0)
    {
        float const speed = cfg.menuTextFlashSpeed / 2.f;
        t = (1 + sin(page().timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
    }

    lerpColor(textColor, mnRendState->textColors[color()], cfg.menuTextFlashColor, t, false/*rgb mode*/);
    textColor[CA] = mnRendState->textColors[color()][CA];

    DGL_Color4f(1, 1, 1, textColor[CA]);
    FR_SetFont(fontId);
    FR_SetColorAndAlphav(textColor);

    if(d->patch)
    {
        String replacement;
        if(!(d->flags & MNTEXT_NO_ALTTEXT))
        {
            replacement = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), *d->patch, d->text);
        }

        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch(*d->patch, replacement, Vector2i(origin->x, origin->y),
                     ALIGN_TOPLEFT, 0, Hu_MenuMergeEffectWithDrawTextFlags(0));
        DGL_Disable(DGL_TEXTURE_2D);

        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawText3(d->text.toUtf8().constData(), origin, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));
    DGL_Disable(DGL_TEXTURE_2D);
}

void LabelWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);

    /// @todo What if patch replacement is disabled?
    if(d->patch)
    {
        patchinfo_t info;
        R_GetPatchInfo(*d->patch, &info);
        Rect_SetWidthHeight(geometry(), info.geometry.size.width, info.geometry.size.height);
        return;
    }

    Size2Raw size;
    FR_SetFont(page->predefinedFont(mn_page_fontid_t(font())));
    FR_TextSize(&size, d->text.toUtf8().constData());
    Rect_SetWidthHeight(geometry(), size.width, size.height);
}

void LabelWidget::setPatch(patchid_t *newPatch)
{
    d->patch = newPatch;
}

void LabelWidget::setText(String const &newText)
{
    d->text = newText;
}

} // namespace menu
} // namespace common
