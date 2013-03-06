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
#include "linkwindow.h"
#include "opendialog.h"
#include "aboutdialog.h"
#include "localserverdialog.h"
#include "preferences.h"
#include <de/shell/LocalServer>
#include <de/shell/ServerFinder>
#include <QMenuBar>
#include <QMessageBox>

Q_DECLARE_METATYPE(de::Address)

using namespace de;
using namespace de::shell;

struct GuiShellApp::Instance
{
    ServerFinder finder;

    QMenuBar *menuBar;
    QMenu *localMenu;
#ifdef MACOSX
    QAction *stopAction;
    QAction *disconnectAction;
#endif
    QList<LinkWindow *> windows;

    Preferences *prefs;

    Instance() : prefs(0)
    {}

    ~Instance()
    {
        foreach(LinkWindow *win, windows)
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

    d->localMenu = new QMenu(tr("Running Servers"));
    connect(d->localMenu, SIGNAL(aboutToShow()), this, SLOT(updateLocalServerMenu()));

#ifdef MACOSX
    setQuitOnLastWindowClosed(false);

    // On Mac OS X, the menu is not window-specific.
    d->menuBar = new QMenuBar(0);

    QMenu *menu = d->menuBar->addMenu(tr("Connection"));
    menu->addAction(tr("Connect..."), this, SLOT(connectToServer()),
                    QKeySequence(tr("Ctrl+O", "Connection|Connect")));
    d->disconnectAction = menu->addAction(tr("Disconnect"), this, SLOT(disconnectFromServer()),
                                          QKeySequence(tr("Ctrl+D", "Connection|Disconnect")));
    d->disconnectAction->setDisabled(true);
    menu->addSeparator();
    menu->addAction(tr("Close Window"), this, SLOT(closeActiveWindow()),
                    QKeySequence(tr("Ctrl+W", "Connection|Close Window")));

    QMenu *svMenu = d->menuBar->addMenu(tr("Local Server"));
    svMenu->addAction(tr("Start..."), this, SLOT(startLocalServer()),
                      QKeySequence(tr("Ctrl+N", "Local Server|Start")));
    d->stopAction = svMenu->addAction(tr("Stop"), this, SLOT(stopServer()));
    svMenu->addSeparator();
    svMenu->addMenu(d->localMenu);

    connect(menu, SIGNAL(aboutToShow()), this, SLOT(updateMenu()));
    connect(svMenu, SIGNAL(aboutToShow()), this, SLOT(updateMenu()));

    // These will appear in the application menu:
    menu->addAction(tr("Preferences..."), this, SLOT(showPreferences()), QKeySequence(tr("Ctrl+,")));
    menu->addAction(tr("About"), this, SLOT(aboutShell()));
#endif

    newOrReusedConnectionWindow();
}

GuiShellApp::~GuiShellApp()
{
    delete d;
}

LinkWindow *GuiShellApp::newOrReusedConnectionWindow()
{
    LinkWindow *found = 0;
    QWidget *other = activeWindow(); // for positioning a new window

    // Look for a window with a closed connection.
    foreach(LinkWindow *win, d->windows)
    {
        if(!win->isConnected())
        {
            found = win;
            found->raise();
            found->activateWindow();
            d->windows.removeOne(win);
            break;
        }
        if(!other) other = win;
    }

    if(!found)
    {
        found = new LinkWindow;
        connect(found, SIGNAL(linkOpened(LinkWindow*)),this, SLOT(updateMenu()));
        connect(found, SIGNAL(linkClosed(LinkWindow*)), this, SLOT(updateMenu()));
        connect(found, SIGNAL(closed(LinkWindow *)), this, SLOT(windowClosed(LinkWindow *)));

        // Initial position and size.
        if(other)
        {
            found->move(other->pos() + QPoint(30, 30));
        }
    }

    d->windows.prepend(found);
    found->show();
    return found;
}

GuiShellApp &GuiShellApp::app()
{
    return *static_cast<GuiShellApp *>(qApp);
}

QMenu *GuiShellApp::localServersMenu()
{
    return d->localMenu;
}

ServerFinder &GuiShellApp::serverFinder()
{
    return d->finder;
}

void GuiShellApp::connectToServer()
{
    LinkWindow *win = newOrReusedConnectionWindow();

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
    Address host = act->data().value<Address>();

    LinkWindow *win = newOrReusedConnectionWindow();
    win->openConnection(host.asText());
}

void GuiShellApp::disconnectFromServer()
{
    LinkWindow *win = dynamic_cast<LinkWindow *>(activeWindow());
    if(win)
    {
        win->closeConnection();
    }
}

void GuiShellApp::closeActiveWindow()
{
    QWidget *win = activeWindow();
    if(win) win->close();
}

void GuiShellApp::startLocalServer()
{
    try
    {
        LocalServerDialog dlg;
        if(dlg.exec() == QDialog::Accepted)
        {
            QStringList opts = dlg.additionalOptions();
            if(!Preferences::iwadFolder().isEmpty())
            {
                opts << "-iwad" << Preferences::iwadFolder().toString();
            }

            LocalServer sv;
            sv.start(dlg.port(), dlg.gameMode(), opts, dlg.runtimeFolder());

            newOrReusedConnectionWindow()->openConnection(sv.openLink());
        }
    }
    catch(Error const &er)
    {
        QMessageBox::critical(0, tr("Failed to Start Server"), er.asText());
    }
}

void GuiShellApp::stopServer()
{
    LinkWindow *win = dynamic_cast<LinkWindow *>(activeWindow());
    if(win && win->isConnected())
    {
        if(QMessageBox::question(win, tr("Stop Server?"),
                                 tr("Are you sure you want to stop this server?"),
                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            win->sendCommandToServer("quit");
        }
    }
}

void GuiShellApp::updateLocalServerMenu()
{
    d->localMenu->setDisabled(d->finder.foundServers().isEmpty());
    d->localMenu->clear();

    foreach(Address const &host, d->finder.foundServers())
    {
        QString label = QString("%1 - %2 (%3/%4)")
                .arg(host.asText())
                .arg(d->finder.name(host))
                .arg(d->finder.playerCount(host))
                .arg(d->finder.maxPlayers(host));

        QAction *act = d->localMenu->addAction(label, this, SLOT(connectToLocalServer()));
        act->setData(QVariant::fromValue(host));
    }
}

void GuiShellApp::aboutShell()
{
    AboutDialog().exec();
}

void GuiShellApp::showPreferences()
{
    if(!d->prefs)
    {
        d->prefs = new Preferences;
        connect(d->prefs, SIGNAL(finished(int)), this, SLOT(preferencesDone()));
        foreach(LinkWindow *win, d->windows)
        {
            connect(d->prefs, SIGNAL(consoleFontChanged()), win, SLOT(updateConsoleFontFromPreferences()));
        }
        d->prefs->open();
    }
    else
    {
        d->prefs->activateWindow();
    }
}

void GuiShellApp::preferencesDone()
{
    d->prefs->deleteLater();
    d->prefs = 0;
}

void GuiShellApp::updateMenu()
{
#ifdef MACOSX
    LinkWindow *win = dynamic_cast<LinkWindow *>(activeWindow());
    d->stopAction->setEnabled(win && win->isConnected());
    d->disconnectAction->setEnabled(win && win->isConnected());
#endif
    updateLocalServerMenu();
}

void GuiShellApp::windowClosed(LinkWindow *window)
{
    d->windows.removeAll(window);
    window->deleteLater();
}
