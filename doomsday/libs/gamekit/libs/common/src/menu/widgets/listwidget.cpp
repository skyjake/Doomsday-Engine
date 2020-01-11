/** @file listwidget.cpp  UI widget for a selectable list of items.
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
#include "menu/widgets/listwidget.h"

#include "hu_menu.h" // menu sounds
#include "menu/page.h"

using namespace de;

namespace common {
namespace menu {

ListWidget::Item::Item(const String &text, int userValue)
{
    setText(text);
    setUserValue(userValue);
}

void ListWidget::Item::setText(const String &newText)
{
    _text = newText;
}

String ListWidget::Item::text() const
{
    return _text;
}

void ListWidget::Item::setUserValue(int newUserValue)
{
    _userValue = newUserValue;
}

int ListWidget::Item::userValue() const
{
    return _userValue;
}

DE_PIMPL_NOREF(ListWidget)
{
    Items items;
    int   selection      = 0; ///< Selected item (-1 if none).
    int   first          = 0; ///< First visible item.
    int   numvis         = 0;
    bool  reorderEnabled = false;

    ~Impl() { deleteAll(items); }
};

ListWidget::ListWidget()
    : Widget()
    , d(new Impl)
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR1);
}

ListWidget::~ListWidget()
{}

ListWidget &ListWidget::addItem(Item *item)
{
    if(item) d->items << item;
    return *this;
}

ListWidget &ListWidget::addItems(const Items &itemsToAdd)
{
    for(Item *item : itemsToAdd) addItem(item);
    return *this;
}

const ListWidget::Items &ListWidget::items() const
{
    return d->items;
}

void ListWidget::updateGeometry()
{
    geometry().setSize(Vec2ui(0, 0));
    FR_PushAttrib();
    FR_SetFont(page().predefinedFont(mn_page_fontid_t(font())));

    RectRaw itemGeometry{};
    for(int i = 0; i < itemCount(); ++i)
    {
        Item *item = d->items[i];

        FR_TextSize(&itemGeometry.size, item->text());
        if(i != itemCount() - 1)
        {
            itemGeometry.size.height *= 1 + MNDATA_LIST_LEADING;
        }

        geometry() |=
            Rectanglei::fromSize(Vec2i(itemGeometry.origin.xy),
                                 Vec2ui(itemGeometry.size.width, itemGeometry.size.height));

        itemGeometry.origin.y += itemGeometry.size.height;
    }
    FR_PopAttrib();
}

void ListWidget::draw() const
{
    const bool flashSelection = (isActive() && selectionIsVisible());
    const Vec4f &textColor = mnRendState->textColors[color()];

    Vec4f flashColor = textColor;
    if (flashSelection)
    {
        flashColor = selectionFlashColor(flashColor);
    }

    const Vec4f dimColor = Vec4f(Vec3f(textColor) * MNDATA_LIST_NONSELECTION_LIGHT, textColor.w);

    if(d->first < d->items.count() && d->numvis > 0)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(mnRendState->textFonts[font()]);

        Vec2i origin = geometry().topLeft;
        int itemIdx = d->first;
        do
        {
            const Item *item = d->items[itemIdx];

            const Vec4f &color =
                d->selection == itemIdx ? (flashSelection ? flashColor : textColor) : dimColor;

            const int itemHeight =
                FR_TextHeight(item->text()) * (1 + MNDATA_LIST_LEADING);

            FR_SetColorAndAlpha(color.x,
                                color.y,
                                color.z,
                                color.w * scrollingFadeout(origin.y, origin.y + itemHeight));
            FR_DrawTextXY3(item->text(), origin.x, origin.y, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));
            origin.y += itemHeight;
        } while (++itemIdx < d->items.count() && itemIdx < d->first + d->numvis);

        DGL_Disable(DGL_TEXTURE_2D);
    }
}

int ListWidget::handleCommand(menucommand_e cmd)
{
    switch (cmd)
    {
    case MCMD_NAV_DOWN:
    case MCMD_NAV_UP:
        if (isActive())
        {
            int oldSelection = d->selection;
            if(MCMD_NAV_DOWN == cmd)
            {
                if(d->selection < itemCount() - 1)
                    selectItem(d->selection + 1);
            }
            else
            {
                if(d->selection > 0)
                    selectItem(d->selection - 1);
            }

            if(d->selection != oldSelection)
            {
                S_LocalSound(cmd == MCMD_NAV_DOWN? SFX_MENU_NAV_DOWN : SFX_MENU_NAV_UP, NULL);
                execAction(Modified);
            }
            return true;
        }
        return false; // Not eaten.

    case MCMD_NAV_OUT:
        if (isActive())
        {
            S_LocalSound(SFX_MENU_CANCEL, NULL);
            setFlags(Active, UnsetFlags);
            execAction(Closed);
            return true;
        }
        return false; // Not eaten.

    case MCMD_SELECT:
        if (!isActive())
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            setFlags(Active);
            execAction(Activated);
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            setFlags(Active, UnsetFlags);
            execAction(Deactivated);
        }
        return true;

    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT:
        if (d->reorderEnabled && isActive())
        {
            if (reorder(selection(), cmd == MCMD_NAV_LEFT ? -1 : +1))
            {
                S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
                execAction(Modified);
            }
        }
        return true;

    default: return false; // Not eaten.
    }
}

int ListWidget::selection() const
{
    return d->selection;
}

int ListWidget::first() const
{
    return d->first;
}

bool ListWidget::selectionIsVisible() const
{
    return (d->selection >= d->first && d->selection < d->first + d->numvis);
}

void ListWidget::updateVisibleSelection()
{
    d->numvis = itemCount();
    if(d->selection >= 0)
    {
        if(d->selection < d->first)
            d->first = d->selection;
        if(d->selection > d->first + d->numvis - 1)
            d->first = d->selection - d->numvis + 1;
    }
}

void ListWidget::pageActivated()
{
    Widget::pageActivated();

    // Determine number of potentially visible items.
    updateVisibleSelection();
}

int ListWidget::itemData(int index) const
{
    if (index >= 0 && index < itemCount())
    {
        return d->items[index]->userValue();
    }
    return 0;
}

int ListWidget::findItem(int userValue) const
{
    for(int i = 0; i < d->items.count(); ++i)
    {
        Item *item = d->items[i];
        if(item->userValue() == userValue)
        {
            return i;
        }
    }
    return -1;
}

bool ListWidget::selectItem(int itemIndex, int flags)
{
    if(itemIndex >= 0 && itemIndex < itemCount())
    {
        if(d->selection != itemIndex)
        {
            d->selection = itemIndex;
            if(!(flags & MNLIST_SIF_NO_ACTION))
            {
                execAction(Modified);
            }
            return true;
        }
    }
    return false;
}

bool ListWidget::selectItemByValue(int userValue, int flags)
{
    return selectItem(findItem(userValue), flags);
}

bool ListWidget::reorder(int itemIndex, int indexOffset)
{
    if (itemIndex + indexOffset < 0 || itemIndex + indexOffset >= d->items.sizei())
    {
        return false; // Would go out of bounds.
    }

    if (d->selection == itemIndex)
    {
        d->selection += indexOffset;
    }

    while (indexOffset < 0)
    {
        std::swap(d->items[itemIndex - 1], d->items[itemIndex]);
        --itemIndex;
        ++indexOffset;
    }
    while (indexOffset > 0)
    {
        std::swap(d->items[itemIndex + 1], d->items[itemIndex]);
        ++itemIndex;
        --indexOffset;
    }
    return true;
}

ListWidget &ListWidget::setReorderingEnabled(bool reorderEnabled)
{
    d->reorderEnabled = reorderEnabled;
    return *this;
}

} // namespace menu
} // namespace common
