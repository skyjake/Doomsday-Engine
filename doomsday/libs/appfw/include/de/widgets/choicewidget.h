/** @file widgets/choicewidget.h  Widget for choosing from a set of alternatives.
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

#ifndef LIBAPPFW_CHOICEWIDGET_H
#define LIBAPPFW_CHOICEWIDGET_H

#include "../PopupButtonWidget"
#include "../PopupMenuWidget"
#include "../ui/ActionItem"

namespace de {

/**
 * Widget for choosing an item from a set of alternatives.
 *
 * The items of the widget should be ChoiceItem instances, or at least derived
 * from ActionItem/ChoiceItem.
 *
 * The default opening direction for the popup is to the right.
 *
 * @ingroup guiWidgets
 */
class LIBAPPFW_PUBLIC ChoiceWidget : public PopupButtonWidget
{
public:
    DE_DEFINE_AUDIENCE2(Selection,     void selectionChanged(ChoiceWidget &, ui::DataPos pos))
    DE_DEFINE_AUDIENCE2(UserSelection, void selectionChangedByUser(ChoiceWidget &, ui::DataPos pos))

    /**
     * The items of the widget are expected to be instanced of
     * ChoiceWidget::Item or derived from it (or at least ui::ActionItem).
     */
    class Item : public ui::ActionItem
    {
    public:
        Item(String const &label, Image const &image = Image())
            : ui::ActionItem(image, label) {}

        Item(String const &label, const Value &userData, Image const &image = Image())
            : ui::ActionItem(image, label)
        {
            setData(userData);
        }

        Item(String const &label, const String &userText, Image const &image = Image());
        Item(String const &label, dint userNumber, Image const &image = Image());
        Item(String const &label, ddouble userNumber, Image const &image = Image());
    };

public:
    ChoiceWidget(String const &name = String());

    ui::Data &items();

    /**
     * Sets the data model of the choice widget to some existing one. The data must
     * remain in existence until the ChoiceWidget is deleted.
     *
     * @param items  Ownership not taken.
     */
    void setItems(ui::Data const &items);

    void setNoSelectionHint(String const &hint);

    void useDefaultItems();

    PopupMenuWidget &popup();

    void setSelected(ui::Data::Pos pos);

    bool isValidSelection() const;
    ui::Data::Pos selected() const;
    ui::Item const &selectedItem() const;

    /**
     * Returns a rule that determines what is the maximum width of the widget. This is
     * the length of the longest item plus margins.
     *
     * A choice widget keeps changing its size depending on the selected item. Also, only
     * the selected item uses the "choice.selected" font, so the maximum width depends on
     * what is the widest item using that font.
     */
    Rule const &maximumWidth() const;

    void openPopup();

private:
    DE_PRIVATE(d)
};

typedef ChoiceWidget::Item ChoiceItem;

} // namespace de

#endif // LIBAPPFW_CHOICEWIDGET_H
