/** @file gameselectionwidget.h
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

#ifndef DENG_CLIENT_GAMESELECTIONWIDGET_H
#define DENG_CLIENT_GAMESELECTIONWIDGET_H

#include <de/ScrollAreaWidget>
#include <de/FoldPanelWidget>
#include <de/ButtonWidget>
#include <de/IPersistent>

#include "gamefilterwidget.h"

/**
 * Menu for selecting
 */
class GameSelectionWidget : public de::ScrollAreaWidget, public de::IPersistent
{
    Q_OBJECT

public:
    GameSelectionWidget(de::String const &name = "gameselection");

    void setTitleColor(de::DotPath const &colorId,
                       de::DotPath const &hoverColorId,
                       de::ButtonWidget::HoverColorMode mode = de::ButtonWidget::ModulateColor);
    void setTitleFont(de::DotPath const &fontId);

    /**
     * Returns the filter header widget. It can be placed manually by the user
     * of the widget.
     */
    GameFilterWidget &filter();

    de::FoldPanelWidget *subsetFold(de::String const &name);

    /**
     * Enables or disables execution of the session selection action by
     * pressing the menu items in the widget. By default, this is disabled.
     *
     * @param doAction  @c true to allow action execution, @c false to disallow.
     */
    void enableActionOnSelection(bool doAction);

    de::Action *makeAction(de::ui::Item const &item) const;

    // Events.
    void update();

    // Implements IPersistent.
    void operator >> (de::PersistentState &toState) const;
    void operator << (de::PersistentState const &fromState);

signals:    
    void gameSessionSelected(de::ui::Item const *item);

protected slots:
    void updateSubsetLayout();
    void select(de::ui::Item const *item);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_GAMESELECTIONWIDGET_H
