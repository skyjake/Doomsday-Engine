/** @file statuswindow.h  Widget for showing server's status.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QWidget>
#include <de/String>
#include <de/shell/Link>
#include <de/shell/Protocol>

/**
 * Widget for showing server's status.
 */
class StatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatusWidget(QWidget *parent = 0);

    void setGameState(QString mode, QString rules, QString mapId, QString mapTitle);
    void setMapOutline(de::shell::MapOutlinePacket const &outline);
    void setPlayerInfo(de::shell::PlayerInfoPacket const &plrInfo);

    void paintEvent(QPaintEvent *);

public slots:
    void linkConnected(de::shell::Link *link);
    void linkDisconnected();

private:
    DENG2_PRIVATE(d)
};

#endif // STATUSWIDGET_H
