/** @file tabwidget.h  Tab widget.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_TABWIDGET_H
#define LIBAPPFW_TABWIDGET_H

#include "../GuiWidget"
#include "../ui/ImageItem"
#include "../ui/Data"

namespace de {

/**
 * Tab buttons. Behaves like radio buttons, with one of the buttons being selected at a
 * time. One of the tabs is always selected.
 *
 * The widget sets its own height based on the height of the tab buttons. Tab buttons
 * are centered in the width of the widget.
 */
class LIBAPPFW_PUBLIC TabWidget : public GuiWidget
{
    Q_OBJECT

public:
    class TabItem : public ui::ImageItem
    {
    public:
        TabItem(String const &label, QVariant const &userData = QVariant())
            : ImageItem(ShownAsButton, label)
        {
            setData(userData);
        }
        TabItem(Image const &img, String const &label)
            : ImageItem(ShownAsButton, img, label) {}
    };

public:
    TabWidget(String const &name = "");

    void useInvertedStyle();

    /**
     * Items representing the tabs in the widget.
     *
     * @return
     */
    ui::Data &items();

    /**
     * Returns the currently selected tab index. One of the tabs is always selected.
     */
    ui::Data::Pos current() const;

    TabItem &currentItem();

    void setCurrent(ui::Data::Pos itemPos);

    // Events.
    void update();

signals:
    void currentTabChanged();

private:
    DENG2_PRIVATE(d)
};

typedef TabWidget::TabItem TabItem;

} // namespace de

#endif // LIBAPPFW_TABWIDGET_H
