/** @file buttonwidget.cpp  Button widget.
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
#include "menu/widgets/buttonwidget.h"

#include "g_defs.h"
#include "hu_menu.h"   // menu sounds
#include "hu_stuff.h"
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

DE_PIMPL_NOREF(ButtonWidget)
{
    String    text;           ///< Label text.
    patchid_t patch     = -1; ///< Used when drawing this instead of text, if set.
    bool      noAltText = false;
    bool      silent    = false;
};

ButtonWidget::ButtonWidget(const String &text, patchid_t patch)
    : Widget()
    , d(new Impl)
{
    setFont(MENU_FONT2);
    setColor(MENU_COLOR1);

    setText(text);
    setPatch(patch);
}

ButtonWidget::~ButtonWidget()
{}

void ButtonWidget::draw() const
{
    const fontid_t fontId    = mnRendState->textFonts[font()];
    const Vec4f &  textColor = mnRendState->textColors[color()];
    const Vec4f    color     = selectionFlashColor(textColor);
    const float    fadeout   = scrollingFadeout();

    if (fadeout < .001f) return;

    FR_SetFont(fontId);
    FR_SetColorAndAlpha(color.x, color.y, color.z, color.w * fadeout);
    DGL_Color4f(1, 1, 1, color.w * fadeout);

    if(d->patch >= 0)
    {
        String replacement;
        if(!d->noAltText)
        {
            replacement = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.menuPatchReplaceMode), d->patch, d->text);
        }

        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch(d->patch,
                     replacement,
                     geometry().topLeft,
                     ALIGN_TOPLEFT,
                     0,
                     Hu_MenuMergeEffectWithDrawTextFlags(0));
        DGL_Disable(DGL_TEXTURE_2D);

        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawTextXY3(d->text, geometry().topLeft.x, geometry().topLeft.y,
                   ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));
    DGL_Disable(DGL_TEXTURE_2D);
}

int ButtonWidget::handleCommand(menucommand_e cmd)
{
    if (cmd == MCMD_SELECT)
    {
        if (!isActive())
        {
            setFlags(Active);
            execAction(Activated);
        }
        // We are not going to receive a separate "up event".
        if (!d->silent)
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
        }
        setFlags(Active, UnsetFlags);
        execAction(Deactivated);
        return true;
    }
    return false; // Not eaten.
}

void ButtonWidget::setSilent(bool silent)
{
    d->silent = silent;
}

void ButtonWidget::updateGeometry()
{
    String useText = d->text;

    // @todo What if patch replacement is disabled?
    if(d->patch >= 0)
    {
        if(!d->noAltText)
        {
            // Use the replacement string?
            useText = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.menuPatchReplaceMode), d->patch, d->text);
        }

        if(useText.isEmpty())
        {
            // Use the original patch.
            patchinfo_t info;
            R_GetPatchInfo(d->patch, &info);
            geometry().setSize(Vec2ui(info.geometry.size.width, info.geometry.size.height));
            return;
        }
    }

    FR_PushAttrib();
    Size2Raw size;
    FR_SetFont(page().predefinedFont(mn_page_fontid_t(font())));
    FR_TextSize(&size, useText);
    geometry().setSize(Vec2ui(size.width, size.height));
    FR_PopAttrib();
}

String ButtonWidget::text() const
{
    return d->text;
}

ButtonWidget &ButtonWidget::setText(const String &newText)
{
    d->text = labelText(newText);
    return *this;
}

patchid_t ButtonWidget::patch() const
{
    return d->patch;
}

ButtonWidget &ButtonWidget::setPatch(patchid_t newPatch)
{
    d->patch = newPatch;
    return *this;
}

bool ButtonWidget::noAltText() const
{
    return d->noAltText;
}

ButtonWidget &ButtonWidget::setNoAltText(bool yes)
{
    d->noAltText = yes;
    return *this;
}

} // namespace menu
} // namespace common
