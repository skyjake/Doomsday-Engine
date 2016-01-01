/** @file sidebarwidget.h  Base class for sidebar widgets.
 *
 * @authors Copyright (c) 2015-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_UI_SIDEBARWIDGET_H
#define DENG_CLIENT_UI_SIDEBARWIDGET_H

#include <de/PanelWidget>
#include <de/ScrollAreaWidget>
#include <de/SequentialLayout>

/**
 * Editor for changing model asset parameters.
 */
class SidebarWidget : public de::PanelWidget
{
    Q_OBJECT

public:
    SidebarWidget(de::String const &title, de::String const &name = "");

    de::SequentialLayout &layout();
    de::ScrollAreaWidget &containerWidget();
    de::IndirectRule &firstColumnWidth();
    de::Rule const &maximumOfAllGroupFirstColumns() const;

protected:
    void updateSidebarLayout(de::Rule const &minWidth);

    void preparePanelForOpening();
    void panelDismissed();

private:
    DENG2_PRIVATE(d)
};


#endif // DENG_CLIENT_UI_SIDEBARWIDGET_H
