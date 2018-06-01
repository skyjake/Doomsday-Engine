/** @file sidebarwidget.h  Base class for sidebar widgets.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_UI_SIDEBARWIDGET_H
#define DE_CLIENT_UI_SIDEBARWIDGET_H

#include <de/ButtonWidget>
#include <de/ConstantRule>
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
    SidebarWidget(de::String const &title, de::String const &name = de::String());

    de::SequentialLayout &layout();
    de::LabelWidget &     title();
    de::ScrollAreaWidget &containerWidget();
    de::ButtonWidget &    closeButton();
    de::IndirectRule &    firstColumnWidth();
    de::Rule const &      maximumOfAllGroupFirstColumns() const;

protected:
    /**
     * @param minWidth     Minimum width.
     * @param extraHeight  Height in addition to title and layout.
     */
    void updateSidebarLayout(de::Rule const &minWidth    = de::ConstantRule::zero(),
                             de::Rule const &extraHeight = de::ConstantRule::zero());

    void preparePanelForOpening();
    void panelDismissed();

private:
    DE_PRIVATE(d)
};


#endif // DE_CLIENT_UI_SIDEBARWIDGET_H
