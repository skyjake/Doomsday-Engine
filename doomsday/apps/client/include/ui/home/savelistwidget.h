/** @file savelistwidget.h  List showing the available saves of a game.
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

#ifndef DE_CLIENT_UI_HOME_SAVELISTWIDGET_H
#define DE_CLIENT_UI_HOME_SAVELISTWIDGET_H

#include <de/menuwidget.h>
#include <de/ui/data.h>

class GamePanelButtonWidget;

class SaveListWidget : public de::MenuWidget
{
public:
    /**
     * Emitted when the selected item changes.
     *
     * @param pos  Position of the selected item in the shared saved sessions
     *             list data model.
     */
    DE_AUDIENCE(Selection, void saveListSelectionChanged(de::ui::DataPos pos))

    DE_AUDIENCE(DoubleClick, void saveListDoubleClicked(de::ui::DataPos pos))

public:
    SaveListWidget(GamePanelButtonWidget &owner);

    de::ui::DataPos selectedPos() const;
    void setSelectedPos(de::ui::DataPos pos);
    void clearSelection();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_HOME_SAVELISTWIDGET_H
