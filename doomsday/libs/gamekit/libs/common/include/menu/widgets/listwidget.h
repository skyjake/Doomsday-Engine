/** @file listwidget.h  UI widget for a selectable list of items.
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

#ifndef LIBCOMMON_UI_LISTWIDGET
#define LIBCOMMON_UI_LISTWIDGET

#include <de/list.h>
#include <de/string.h>
#include "widget.h"

namespace common {
namespace menu {

#define MNDATA_LIST_LEADING             .5f  ///< Inter-item leading factor (does not apply to MNListInline_Drawer).
#define MNDATA_LIST_NONSELECTION_LIGHT  .7f  ///< Light value multiplier for non-selected items (does not apply to MNListInline_Drawer).

/**
 * @defgroup mnlistSelectItemFlags  MNList Select Item Flags
 */
///@{
#define MNLIST_SIF_NO_ACTION            0x1  ///< Do not call any linked action function.
///@}

/**
 * UI list selection widget.
 *
 * @ingroup menu
 */
class ListWidget : public Widget
{
public:
    struct Item
    {
    public:
        explicit Item(const de::String &text = "", int userValue = 0);

        void setText(const de::String &newText);
        de::String text() const;

        void setUserValue(int newUserValue);
        int userValue() const;

    private:
        de::String _text;
        int _userValue = 0;
    };

    typedef de::List<Item *> Items;

public:
    ListWidget();
    virtual ~ListWidget();

    void draw() const;
    void updateGeometry();
    int handleCommand(menucommand_e command);

    /**
     * Add an Item to the ListWidget. Ownership of the Item is given to ListWidget.
     */
    ListWidget &addItem(Item *item);

    /**
     * Add set of Items to the ListWidget in order. Ownership of the Items is given to ListWidget.
     */
    ListWidget &addItems(const Items &itemsToAdd);

    const Items &items() const;

    inline int itemCount() const { return items().count(); }

    /// @return  Data of item at position @a index. 0 if index is out of bounds.
    int itemData(int index) const;

    /// @return  Index of the found item associated with @a dataValue else -1.
    int findItem(int dataValue) const;

    /**
     * Change the currently selected item.
     * @param flags  @ref mnlistSelectItemFlags
     * @param itemIndex  Index of the new selection.
     * @return  @c true if the selected item changed.
     */
    bool selectItem(int itemIndex, int flags = MNLIST_SIF_NO_ACTION);

    /**
     * Change the currently selected item by looking up its data value.
     * @param flags  @ref mnlistSelectItemFlags
     * @param dataValue  Value associated to the candidate item being selected.
     * @return  @c true if the selected item changed.
     */
    bool selectItemByValue(int itemIndex, int flags = MNLIST_SIF_NO_ACTION);

    bool reorder(int itemIndex, int indexOffset);

    ListWidget &setReorderingEnabled(bool reorderEnabled);

    /// @return  Index of the currently selected item else -1.
    int selection() const;

    /// @return  Index of the first visible item else -1.
    int first() const;

    /// @return  @c true if the currently selected item is presently visible.
    bool selectionIsVisible() const;

    void updateVisibleSelection();

    void pageActivated() override;

private:
    DE_PRIVATE(d)
};

typedef ListWidget::Item ListWidgetItem;

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_LISTWIDGET
