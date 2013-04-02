/** @file shellapp.h  Shell GUI application.
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

#ifndef GUISHELLAPP_H
#define GUISHELLAPP_H

#include "qtguiapp.h"
#include <de/shell/ServerFinder>
#include <QMenu>

class LinkWindow;

class GuiShellApp : public QtGuiApp
{
    Q_OBJECT

public:
    GuiShellApp(int &argc, char **argv);

    LinkWindow *newOrReusedConnectionWindow();
    de::shell::ServerFinder &serverFinder();

    static GuiShellApp &app();
    QMenu *localServersMenu();
    QMenu *makeHelpMenu();

public slots:
    void connectToServer();
    void connectToLocalServer();
    void disconnectFromServer();
    void closeActiveWindow();
    void startLocalServer();
    void stopServer();
    void updateLocalServerMenu();
    void aboutShell();
    void showHelp();
    void openWebAddress(QString address);
    void showPreferences();
    void preferencesDone();
    void updateMenu();

signals:
    void consoleFontChanged();

protected slots:
    void windowClosed(LinkWindow *window);

private:
    DENG2_PRIVATE(d)
};

#endif // GUISHELLAPP_H
