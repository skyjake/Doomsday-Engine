/** @file inlinelistwidget.cpp  UI widget for a selectable, inline-list of items.
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
#include "menu/widgets/inlinelistwidget.h"

#include "hu_menu.h" // menu sounds
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

InlineListWidget::InlineListWidget()
    : ListWidget()
{}

InlineListWidget::~InlineListWidget()
{}

void InlineListWidget::draw() const
{
    const Item *item  = items()[selection()];
    const Vec4f color = selectionFlashColor(mnRendState->textColors[this->color()]);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(mnRendState->textFonts[font()]);
    FR_SetColorAndAlpha(color.x, color.y, color.z, color.w * scrollingFadeout());
    FR_DrawTextXY3(item->text(), geometry().topLeft.x, geometry().topLeft.y,
                   ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

int InlineListWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_SELECT: // Treat as @c MCMD_NAV_RIGHT
    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT: {
        int oldSelection = selection();

        if(MCMD_NAV_LEFT == cmd)
        {
            if(selection() > 0)
                selectItem(selection() - 1);
            else
                selectItem(itemCount() - 1);
        }
        else
        {
            if(selection() < itemCount() - 1)
                selectItem(selection() + 1);
            else
                selectItem(0);
        }

        updateVisibleSelection();

        if(oldSelection != selection())
        {
            S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
            execAction(Modified);
        }
        return true;
      }
    default:
        return false; // Not eaten.
    }
}

void InlineListWidget::updateGeometry()
{
    FR_PushAttrib();
    FR_SetFont(page().predefinedFont(mn_page_fontid_t(font())));
    Size2Raw maxSize{};
    Size2Raw size;
    for (auto i = 0u; i < items().size(); ++i)
    {
        FR_TextSize(&size, items().at(i)->text());
        maxSize.width = de::max(maxSize.width, size.width);
        maxSize.height = de::max(maxSize.height, size.height);
    }
    geometry().setSize(Vec2ui(maxSize.width, maxSize.height));
    FR_PopAttrib();
}

} // namespace menu
} // namespace common
