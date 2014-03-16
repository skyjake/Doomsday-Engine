/** @file savegameselectionwidget.h
 *
 * @authors Copyright © 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_SAVEGAMESELECTIONWIDGET_H
#define DENG_CLIENT_SAVEGAMESELECTIONWIDGET_H

#include <de/MenuWidget>
#include <de/game/SavedSession>

/**
 * Menu that populates itself with available saved game sessions.
 *
 * @ingroup ui
 */
class SavegameSelectionWidget : public de::MenuWidget
{
    Q_OBJECT

public:
    DENG2_DEFINE_AUDIENCE(Selection, void gameSelected(de::game::SavedSession const &session))

    /**
     * Action for loading a saved session.
     */
    class LoadAction : public de::Action
    {
    public:
        LoadAction(de::game::SavedSession const &session);
        void trigger();

    private:
        DENG2_PRIVATE(d)
    };

public:
    SavegameSelectionWidget();

    /**
     * Enables or disables loading saved game sessions by pressing the menu items in the
     * widget. By default, this is enabled. If disabled, one will only get a notification
     * about the selection.
     *
     * @param enableLoad  @c true to allow automatic loading, @c false to disallow.
     */
    void setLoadGameWhenSelected(bool enableLoad);

    void setColumns(int numberOfColumns);

    de::game::SavedSession const &savedSession(de::ui::DataPos pos) const;

    // Events.
    void update();

signals:
    void availabilityChanged();
    void gameSelected();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_SAVEGAMESELECTIONWIDGET_H
