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

class MainWindow;

class GuiShellApp : public QtGuiApp
{
    Q_OBJECT

public:
    GuiShellApp(int &argc, char **argv);
    ~GuiShellApp();

    void openNewConnectionWindow();
    MainWindow *newOrReusedConnectionWindow();

    static GuiShellApp &app();

    de::shell::ServerFinder &serverFinder();

public slots:
    void connectToServer();
    void connectToLocalServer();
    void disconnectFromServer();
    void startLocalServer();
    void updateLocalServerMenu();
    void aboutShell();

private:
    struct Instance;
    Instance *d;
};

#endif // GUISHELLAPP_H
