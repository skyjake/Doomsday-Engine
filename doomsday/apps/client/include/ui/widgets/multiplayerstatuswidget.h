/** @file multiplayerstatuswidget.h
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_MULTIPLAYERSTATUSWIDGET_H
#define DE_CLIENT_MULTIPLAYERSTATUSWIDGET_H

#include <de/popupmenuwidget.h>

/**
 * Popup menu that appears in the task bar when joined to a multiplayer game.
 *
 * @ingroup ui
 */
class MultiplayerStatusWidget : public de::PopupMenuWidget
{
public:
    MultiplayerStatusWidget();

    void updateElapsedTime();

protected:
    void preparePanelForOpening();
    void panelClosing();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_MULTIPLAYERSTATUSWIDGET_H
