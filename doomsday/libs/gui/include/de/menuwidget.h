/** @file widgets/menuwidget.h
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_MENUWIDGET_H
#define LIBAPPFW_MENUWIDGET_H

#include "de/ui/data.h"
#include "de/ui/actionitem.h"
#include "de/ui/submenuitem.h"
#include "de/ui/variabletoggleitem.h"
#include "de/childwidgetorganizer.h"
#include "de/gridlayout.h"
#include "de/scrollareawidget.h"
#include "de/buttonwidget.h"
#include "de/panelwidget.h"

#include <de/asset.h>

namespace de {

/**
 * Menu with an N-by-M grid of items (child widgets).
 *
 * One or both of the dimensions of the menu grid can be configured to use ui::Expand
 * policy, in which case the child widgets must manage their size on that axis by
 * themselves.
 *
 * A sort order for the items can be optionally defined using
 * MenuWidget::ISortOrder. Sorting affects layout only, not the actual order of
 * the children.
 *
 * MenuWidget uses a ChildWidgetOrganizer to create widgets based on the
 * provided menu items. The organizer can be queried to find widgets matching
 * specific items.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC MenuWidget : public ScrollAreaWidget, public IAssetGroup
{
public:
    /**
     * Notified when an item in the menu is triggered. The corresponding UI item is
     * passed as argument.
     */
    DE_AUDIENCE(ItemTriggered, void menuItemTriggered(const ui::Item &))

    /**
     * Called when a submenu/widget is opened by one of the items.
     *
     * @param panel  Panel that was opened.
     */
    DE_AUDIENCE(SubWidgetOpened, void subWidgetOpened(MenuWidget &, PanelWidget *subwidget))

public:
    MenuWidget(const String &name = String());

    AssetGroup &assets() override;

    /**
     * Configures the layout grid.
     *
     * ui::Fixed policy means that the size of the menu rectangle is fixed, and
     * the size of the children is not modified.
     *
     * ui::Filled policy means that the size of the menu rectangle is fixed,
     * and the size of the children is adjusted to evenly fill the entire menu
     * rectangle.
     *
     * If a dimension is set to ui::Expand policy, the menu's size in that dimension is
     * determined by the summed up size of the children.
     *
     * If the number of columns/rows is set to zero, it means that the number of
     * columns/rows will increase without limitation. Both dimensions cannot be set to
     * zero columns/rows.
     *
     * @param columns       Number of columns in the grid.
     * @param columnPolicy  Policy for sizing columns.
     * @param rows          Number of rows in the grid.
     * @param rowPolicy     Policy for sizing rows.
     * @param layoutMode    Layout mode (column or row first).
     */
    void setGridSize(int columns, ui::SizePolicy columnPolicy,
                     int rows, ui::SizePolicy rowPolicy,
                     GridLayout::Mode layoutMode = GridLayout::ColumnFirst);

    ui::Data &items();

    const ui::Data &items() const;

    /**
     * Sets the data context of the menu to some existing context. The context
     * must remain in existence until the MenuWidget is deleted.
     *
     * @param items  Ownership not taken.
     */
    void setItems(const ui::Data &items);

    void useDefaultItems();

    bool isUsingDefaultItems() const;

    ChildWidgetOrganizer &organizer();
    const ChildWidgetOrganizer &organizer() const;

    void setVirtualizationEnabled(bool enabled, int averageItemHeight = 0);

    /**
     * Enables or disables the variant labels and images from `VariantActionItem`s.
     * Affects items will be updated.
     *
     * @param variantsEnabled  @c true to enable variants.
     */
    void setVariantItemsEnabled(bool variantsEnabled);

    bool variantItemsEnabled() const;

    template <typename WidgetType>
    WidgetType &itemWidget(const ui::Item &item) const {
        return organizer().itemWidget(item)->as<WidgetType>();
    }

    template <typename WidgetType>
    WidgetType *itemWidget(ui::DataPos itemPos) const {
        if (itemPos < items().size()) {
            if (auto *widget = organizer().itemWidget(items().at(itemPos))) {
                return maybeAs<WidgetType>(widget);
            }
        }
        return nullptr;
    }

    ui::DataPos findItem(const GuiWidget &widget) const;

    /**
     * Returns the number of visible items in the menu. Hidden items are not
     * included in this count.
     */
    int count() const;

    /**
     * Determines if a widget is included in the menu. Hidden widgets are not
     * part of the menu.
     *
     * @param widget  Widget.
     *
     * @return @c true, if the widget is laid out as part of the menu.
     */
    bool isWidgetPartOfMenu(const GuiWidget &widget) const;

    /**
     * Lays out children of the menu according to the grid setup. This should
     * be called if children are manually added or removed from the menu.
     */
    void updateLayout();

    /**
     * Provides read-only access to the layout metrics.
     */
    const GridLayout &layout() const;

    GridLayout &layout();

    const Rule &contentHeight() const;

    // Events.
    void offerFocus() override;
    void update() override;
    bool handleEvent(const Event &event) override;

    void dismissPopups();

protected:
    void updateStyle() override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_MENUWIDGET_H
