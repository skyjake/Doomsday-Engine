/** @file choicewidget.h  Widget for choosing from a set of alternatives.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_CHOICEWIDGET_H
#define DENG_CLIENT_CHOICEWIDGET_H

#include "buttonwidget.h"
#include "popupmenuwidget.h"
#include "actionitem.h"

/**
 * Widget for choosing an item from a set of alternatives.
 *
 * The items of the widget should be ChoiceItem instances, or at least derived
 * from ActionItem/ChoiceItem.
 *
 * The default opening direction for the popup is to the right.
 */
class ChoiceWidget : public ButtonWidget
{
    Q_OBJECT

public:
    /**
     * The items of the widget are expected to be instanced of
     * ChoiceWidget::Item or derived from it (or at least ui::ActionItem).
     */
    class Item : public ui::ActionItem
    {
    public:
        Item(de::String const &label, de::Image const &image = de::Image())
            : ui::ActionItem(image, label) {}

        Item(de::String const &label, QVariant const &userData, de::Image const &image = de::Image())
            : ui::ActionItem(image, label)
        {
            setData(userData);
        }
    };

public:
    ChoiceWidget(de::String const &name = "");

    void setOpeningDirection(ui::Direction dir);

    ui::Context &items();

    PopupMenuWidget &popup();

    void setSelected(ui::Context::Pos pos);

    ui::Context::Pos selected() const;
    ui::Item const &selectedItem() const;

public slots:
    void openPopup();

signals:
    void selectionChanged(uint pos);
    void selectionChangedByUser(uint pos);

private:
    DENG2_PRIVATE(d)
};

typedef ChoiceWidget::Item ChoiceItem;

#endif // DENG_CLIENT_CHOICEWIDGET_H
