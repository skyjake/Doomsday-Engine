/** @file savedsessionmenuwidget.h
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

#ifndef DENG_CLIENT_SAVEDSESSIONMENUWIDGET_H
#define DENG_CLIENT_SAVEDSESSIONMENUWIDGET_H

#include "sessionmenuwidget.h"
#include <de/game/SavedSession>

/**
 * Menu that populates itself with available saved game sessions.
 *
 * @ingroup ui
 */
class SavedSessionMenuWidget : public SessionMenuWidget
{
public:
    SavedSessionMenuWidget();

    de::Action *makeAction(de::ui::Item const &item);

    // Widget factory.
    GuiWidget *makeItemWidget(de::ui::Item const &, GuiWidget const *);
    void updateItemWidget(GuiWidget &, de::ui::Item const &);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_SAVEDSESSIONMENUWIDGET_H
