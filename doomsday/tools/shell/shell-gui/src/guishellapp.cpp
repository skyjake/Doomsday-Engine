/** @file shellapp.cpp  Shell GUI application.
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

#include "guishellapp.h"
#include "mainwindow.h"
#include "opendialog.h"
#include <QMenuBar>
#include <de/shell/ServerFinder>

using namespace de;
using namespace de::shell;

struct GuiShellApp::Instance
{
    ServerFinder finder;

    QMenuBar *menuBar;
    QMenu *localMenu;
    QList<MainWindow *> windows;

    ~Instance()
    {
        foreach(MainWindow *win, windows)
        {
            delete win;
        }
    }
};

GuiShellApp::GuiShellApp(int &argc, char **argv)
    : QtGuiApp(argc, argv), d(new Instance)
{
    // Metadata.
    setOrganizationDomain ("dengine.net");
    setOrganizationName   ("Deng Team");
    setApplicationName    ("doomsday-shell-gui");
    setApplicationVersion (SHELL_VERSION);

    setQuitOnLastWindowClosed(false);

    d->menuBar = new QMenuBar(0);

    QMenu *menu = d->menuBar->addMenu(tr("Server"));
    menu->addAction(tr("Connect..."), this, SLOT(connectToServer()),
                    QKeySequence(tr("Ctrl+O", "Server|Connect")));
    menu->addAction(tr("Disconnect"), this, SLOT(disconnectFromServer()),
                    QKeySequence(tr("Ctrl+W", "Server|Disconnect")));
    menu->addSeparator();
    menu->addAction(tr("Start Local Server"), this, SLOT(startLocalServer()),
                    QKeySequence(tr("Ctrl+N", "Server|Start Local Server")));

    d->localMenu = menu->addMenu(tr("Local Servers"));
    connect(d->localMenu, SIGNAL(aboutToShow()), this, SLOT(updateLocalServerMenu()));

    menu->addAction(tr("About"), this, SLOT(aboutShell()));

    openNewConnectionWindow();

}

GuiShellApp::~GuiShellApp()
{
    delete d;
}

void GuiShellApp::openNewConnectionWindow()
{
    MainWindow *win = new MainWindow;
    d->windows.prepend(win);
    win->show();
}

MainWindow *GuiShellApp::newOrReusedConnectionWindow()
{
    MainWindow *found = 0;

    // Look for a window with a closed connection.
    foreach(MainWindow *win, d->windows)
    {
        if(!win->isConnected())
        {
            found = win;
            found->raise();
            found->activateWindow();
            d->windows.removeOne(win);
            break;
        }
    }

    if(!found)
    {
        found = new MainWindow;
    }

    d->windows.prepend(found);
    found->show();
    return found;
}

GuiShellApp &GuiShellApp::app()
{
    return *static_cast<GuiShellApp *>(qApp);
}

ServerFinder &GuiShellApp::serverFinder()
{
    return d->finder;
}

void GuiShellApp::connectToServer()
{
    MainWindow *win = newOrReusedConnectionWindow();

    QScopedPointer<OpenDialog> dlg(new OpenDialog(win));
    dlg->setWindowModality(Qt::WindowModal);

    if(dlg->exec() == OpenDialog::Accepted)
    {
        win->openConnection(dlg->address());
    }
}

void GuiShellApp::connectToLocalServer()
{
    QAction *act = dynamic_cast<QAction *>(sender());
    qDebug() << act;
}

void GuiShellApp::disconnectFromServer()
{
}

void GuiShellApp::startLocalServer()
{
}

void GuiShellApp::updateLocalServerMenu()
{
    d->localMenu->clear();

    foreach(Address const &host, d->finder.foundServers())
    {
        QString label = QString("%1 - %2 (%3/%4)")
                .arg(host.asText())
                .arg(d->finder.name(host))
                .arg(d->finder.playerCount(host))
                .arg(d->finder.maxPlayers(host));

        d->localMenu->addAction(label, this, SLOT(connectToLocalServer()));
    }
}

void GuiShellApp::aboutShell()
{
}



