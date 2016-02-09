/** @file savelistwidget.h  List showing the available saves of a game.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_UI_HOME_SAVELISTWIDGET_H
#define DENG_CLIENT_UI_HOME_SAVELISTWIDGET_H

#include <de/MenuWidget>
#include <de/ui/Data>

class GamePanelButtonWidget;

class SaveListWidget : public de::MenuWidget
{
    Q_OBJECT

public:
    SaveListWidget(GamePanelButtonWidget &owner);

    de::ui::DataPos selectedPos() const;
    void clearSelection();

signals:
    /**
     * Emitted when the selected item changes.
     *
     * @param pos  Position of the selected item in the shared saved sessions
     *             list data model.
     */
    void selectionChanged(unsigned int pos);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_HOME_SAVELISTWIDGET_H
