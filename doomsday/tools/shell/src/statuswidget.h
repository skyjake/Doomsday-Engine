/** @file statuswindow.h  Widget for showing server's status.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef STATUSWIDGET_H
#define STATUSWIDGET_H

#include <de/guiwidget.h>
#include <de/string.h>
#include <doomsday/network/link.h>

/**
 * Widget for showing server's status.
 */
class StatusWidget : public de::GuiWidget
{    
public:
    explicit StatusWidget();

    void setGameState(de::String mode, de::String rules, de::String mapId, de::String mapTitle);
    void setMapOutline(const network::MapOutlinePacket &outline);
    void setPlayerInfo(const network::PlayerInfoPacket &plrInfo);

//    void paintEvent(QPaintEvent *);

    void linkConnected(network::Link *link);
    void linkDisconnected();

private:
    DE_PRIVATE(d)
};

#endif // STATUSWIDGET_H
