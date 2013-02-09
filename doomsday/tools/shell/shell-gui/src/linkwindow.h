/** @file linkwindow.h  Window for a server link.
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

#ifndef LINKWINDOW_H
#define LINKWINDOW_H

#include <QMainWindow>
#include <de/String>

/**
 * Window for a server link.
 */
class LinkWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    LinkWindow(QWidget *parent = 0);
    ~LinkWindow();

    void setTitle(QString const &title);

    bool isConnected() const;
    void closeEvent(QCloseEvent *);

signals:
    void closed(LinkWindow *window);

public slots:
    void openConnection(QString address);
    void closeConnection();
    void switchToStatus();
    void switchToConsole();
    void updateWhenConnected();

protected slots:
    void handleIncomingPackets();
    void sendCommandToServer(de::String command);
    void addressResolved();
    void connected();
    void disconnected();

private:
    struct Instance;
    Instance *d;
};

#endif // LINKWINDOW_H
