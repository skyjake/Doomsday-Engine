/** @file homemenuwidget.h  Menu for the Home.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_CLIENT_UI_HOME_HOMEMENUWIDGET_H
#define DE_CLIENT_UI_HOME_HOMEMENUWIDGET_H

#include <de/menuwidget.h>

class HomeItemWidget;
class ColumnWidget;

/**
 * Menu for items in Home columns.
 *
 * Handles common selection and focusing logic.
 */
class HomeMenuWidget : public de::MenuWidget
{
public:
    DE_AUDIENCE(Selection, void selectedIndexChanged(HomeMenuWidget &, de::ui::DataPos index))
    DE_AUDIENCE(Click,     void menuItemClicked(HomeMenuWidget &, de::ui::DataPos index))

public:
    HomeMenuWidget(const de::String &name = de::String());

    void unselectAll();
    void restorePreviousSelection();

    /**
     * Returns the selected widget's index number in the list of menu children.
     */
    de::ui::DataPos selectedIndex() const;

    const de::ui::Item *selectedItem() const;

    /**
     * @brief setSelectedIndex
     * @param index
     *
     * @return The highlighted widget, if one was highlighted. Otherwise, returns nullptr.
     */
    void setSelectedIndex(de::ui::DataPos index);

    const de::ui::Item *interactedItem() const;
    const de::ui::Item *actionItem() const;
    void setInteractedItem(const de::ui::Item *menuItem,
                           const de::ui::Item *actionItem);

    ColumnWidget *parentColumn() const;

protected:
    void mouseActivityInItem();
    void itemSelectionChanged();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_HOME_HOMEMENUWIDGET_H
