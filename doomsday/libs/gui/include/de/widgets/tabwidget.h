/** @file tabwidget.h  Tab widget.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/NoneValue>

namespace de {

/**
 * Tab buttons. Behaves like radio buttons, with one of the buttons being selected at a
 * time. One of the tabs is always selected.
 *
 * The widget sets its own height based on the height of the tab buttons. Tab buttons
 * are centered in the width of the widget.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC TabWidget : public GuiWidget
{
public:
    DE_DEFINE_AUDIENCE2(Tab, void currentTabChanged(TabWidget &))

    class TabItem : public ui::ImageItem
    {
    public:
        TabItem(String const &label, const Value &userData = NoneValue())
            : ImageItem(ShownAsButton, label)
        {
            setData(userData);
        }
        TabItem(Image const &img, String const &label)
            : ImageItem(ShownAsButton, img, label)
        {}
        void setShortcutKey(int ddKey)
        {
            _shortcutKey = ddKey;
            notifyChange();
        }
        int shortcutKey() const
        {
            return _shortcutKey;
        }
    private:
        int _shortcutKey; // DDKEY
    };

public:
    TabWidget(String const &name = String());

    void useInvertedStyle();

    void clearItems();

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
    void update() override;
    bool handleEvent(de::Event const &ev) override;

private:
    DE_PRIVATE(d)
};

typedef TabWidget::TabItem TabItem;

} // namespace de

#endif // LIBAPPFW_TABWIDGET_H
