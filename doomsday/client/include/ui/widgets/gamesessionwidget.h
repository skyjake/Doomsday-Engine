/** @file gamesessionwidget.h
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_GAMESESSIONWIDGET_H
#define DENG_CLIENT_GAMESESSIONWIDGET_H

#include <de/ButtonWidget>
#include <de/DocumentWidget>

/**
 * Widget for representing an item (game session) in the game selection menu.
 *
 * It has two buttons: one for starting the game and one for configuring it.
 */
class GameSessionWidget : public de::GuiWidget
{
public:
    GameSessionWidget();

    de::ButtonWidget &loadButton();
    de::ButtonWidget &infoButton();
    de::DocumentWidget &document();

    virtual void updateInfoContent();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_GAMESESSIONWIDGET_H
