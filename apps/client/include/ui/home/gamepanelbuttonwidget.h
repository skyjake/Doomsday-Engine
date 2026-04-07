/** @file gamepanelbuttonwidget.h  Panel button for games.
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

#ifndef DE_CLIENT_UI_HOME_GAMEPANELBUTTONWIDGET_H
#define DE_CLIENT_UI_HOME_GAMEPANELBUTTONWIDGET_H

#include "../widgets/panelbuttonwidget.h"

#include <doomsday/gameprofiles.h>

class SaveListData;

/**
 * Drawer button for a particular game. Also shows the list of saved games.
 */
class GamePanelButtonWidget : public PanelButtonWidget
{
public:
    GamePanelButtonWidget(GameProfile &game,
                          const SaveListData &savedItems);

    void setSelected(bool selected) override;

    void updateContent();
    void unselectSave();

    de::ButtonWidget &playButton();

    // Events.
    bool handleEvent(const de::Event &event) override;

    void play();
    void selectPackages();
    void clearPackages();

protected:
    void saveSelected(de::ui::DataPos savePos);
    void saveDoubleClicked(de::ui::DataPos savePos);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_HOME_GAMEPANELBUTTONWIDGET_H
