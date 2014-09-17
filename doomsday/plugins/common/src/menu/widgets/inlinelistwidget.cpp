/** @file inlinelistwidget.cpp  UI widget for a selectable, inline-list of items.
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

#include <QtAlgorithms>
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

void InlineListWidget::draw(Point2Raw const *origin)
{
    Item const *item = items()[selection()];

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(mnRendState->textFonts[_pageFontIdx]);
    FR_SetColorAndAlphav(mnRendState->textColors[_pageColorIdx]);
    FR_DrawText3(item->text().toUtf8().constData(), origin, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));

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
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
        }
        return true;
      }
    default:
        return false; // Not eaten.
    }
}

void InlineListWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);
    Item *item = items()[selection()];
    Size2Raw size;

    FR_SetFont(page->predefinedFont(mn_page_fontid_t(_pageFontIdx)));
    FR_TextSize(&size, item->text().toUtf8().constData());
    Rect_SetWidthHeight(_geometry, size.width, size.height);
}

} // namespace menu
} // namespace common
