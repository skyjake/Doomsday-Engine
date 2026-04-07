/** @file shellapp.h  Shell GUI application.
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

#ifndef GUISHELLAPP_H
#define GUISHELLAPP_H

#include <de/baseguiapp.h>
#include <de/imagebank.h>
#include <de/popupmenuwidget.h>
#include <de/serverfinder.h>
#include <de/ui/listdata.h>
#include <de/ui/actionitem.h>

class LinkWindow;

class GuiShellApp : public de::BaseGuiApp
{
public:
    using MenuItems = de::ui::ListDataT<de::ui::ActionItem>;

public:
    GuiShellApp(const de::StringList &args);

    void initialize();
    void quitRequested() override;

    LinkWindow *newOrReusedConnectionWindow();
    int countOpenConnections() const;
    de::ServerFinder &serverFinder();

    static GuiShellApp &app();
    static de::ImageBank &imageBank();
//    de::PopupMenuWidget &localServersMenu();
//    de::PopupMenuWidget *makeHelpMenu();
    const MenuItems &localServerMenuItems() const;

    void connectToServer();
    void connectToLocalServer();
    void disconnectFromServer();
    void closeActiveWindow();
    void startLocalServer();
    void aboutShell();
    void showHelp();
    void openWebAddress(const de::String &address);
    void showPreferences();
//    void updateMenu();

    DE_AUDIENCE(LocalServerStop, void localServerStopped(int port))

protected:
    void checkLocalServers();

private:
    DE_PRIVATE(d)
};

#endif // GUISHELLAPP_H
