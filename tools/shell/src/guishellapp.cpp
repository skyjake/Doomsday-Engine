/** @file shellapp.cpp  Shell GUI application.
 *
 * @authors Copyright © 2013-2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "aboutdialog.h"
#include "linkwindow.h"
#include "localserverdialog.h"
#include "opendialog.h"
#include "preferences.h"

#include <de/config.h>
#include <de/escapeparser.h>
#include <de/filesystem.h>
#include <de/garbage.h>
#include <de/id.h>
#include <de/packageloader.h>
#include <de/serverfinder.h>
#include <de/textvalue.h>
#include <de/timer.h>
#include <de/windowsystem.h>
#include <doomsday/network/localserver.h>
#include <SDL_messagebox.h>

using namespace de;
using namespace network;

DE_PIMPL(GuiShellApp)
, DE_OBSERVES(ServerFinder, Update)
, DE_OBSERVES(GuiLoop, Iteration)
{
    ServerFinder finder;

    ImageBank imageBank;
//    QMenuBar *menuBar;
//    QMenu *localMenu;
//    PopupMenuWidget *localMenu;
#ifdef MACOSX
//    QAction *stopAction;
//    QAction *disconnectAction;
#endif
//    QList<LinkWindow *> windows;
    Hash<int, LocalServer *> localServers; // port as key
    Timer localCheckTimer;
    ui::ListDataT<ui::ActionItem> localServerMenuItems;

    Preferences *prefs;

    Impl(Public *i) : Base(i), prefs(nullptr)
    {
        localCheckTimer.setInterval(1.0_s);
        localCheckTimer.setSingleShot(false);

        finder.audienceForUpdate() += this;

        GuiLoop::get().audienceForIteration() += this;
    }

    ~Impl() override
    {
        self().glDeinit();
    }

    void loopIteration() override
    {
        Garbage_Recycle();
    }

    void foundServersUpdated() override
    {
        DE_ASSERT(inMainThread());

        const auto found =
            map<StringList>(finder.foundServers(), [](const Address &a) { return a.asText(); });

        // Add new servers.
        for (const auto &sv : found)
        {
            if (localServerMenuItems.findData(TextValue(sv)) == ui::Data::InvalidPos)
            {
                const String address = sv;

                auto *item = new ui::ActionItem(
                    ui::Item::ShownAsButton | ui::Item::ActivationClosesPopup |
                        ui::Item::ClosesParentPopup,
                    sv,
                    [address]() {
                        if (auto *win = GuiShellApp::app().newOrReusedConnectionWindow())
                        {
                            win->openConnection(address);
                        }
                    });

                item->setData(TextValue(sv));
                localServerMenuItems << item;
            }
        }

        // Remove servers that are not present.
        for (auto i = localServerMenuItems.begin(); i != localServerMenuItems.end(); )
        {
            if (found.indexOf((*i)->data().asText()) < 0)
            {
                auto *item = *i;
                i = localServerMenuItems.erase(i);
                delete item;
                continue;
            }
            i++;
        }
    }

    void loadAllShaders()
    {
        // Load all the shader program definitions.
        FS::FoundFiles found;
        self().findInPackages("shaders.dei", found);
        DE_FOR_EACH(FS::FoundFiles, i, found)
        {
            LOG_MSG("Loading shader definitions from %s") << (*i)->description();
            self().shaders().addFromInfo(**i);
        }
    }

    DE_PIMPL_AUDIENCE(LocalServerStop)
};

DE_AUDIENCE_METHOD(GuiShellApp, LocalServerStop)

GuiShellApp::GuiShellApp(const StringList &args)
    : BaseGuiApp(args)
    , d(new Impl(this))
{
    // Application metadata.
    {
        auto &md = metadata();
        md.set(ORG_DOMAIN, "dengine.net");
        md.set(ORG_NAME, "Deng Team");
        md.set(APP_NAME, "Shell");
        md.set(APP_VERSION, SHELL_VERSION);
    }

//    d->localMenu = new QMenu(tr("Running Servers"));
//    connect(d->localMenu, SIGNAL(aboutToShow()), this, SLOT(updateLocalServerMenu()));

#ifdef MACOSX
//    setQuitOnLastWindowClosed(false);

//    // On macOS, the menu is not window-specific.
//    d->menuBar = new QMenuBar(0);

//    QMenu *menu = d->menuBar->addMenu(tr("Connection"));
//    menu->addAction(tr("Connect..."), this, SLOT(connectToServer()),
//                    QKeySequence(tr("Ctrl+O", "Connection|Connect")));
//    d->disconnectAction = menu->addAction(tr("Disconnect"), this, SLOT(disconnectFromServer()),
//                                          QKeySequence(tr("Ctrl+D", "Connection|Disconnect")));
//    d->disconnectAction->setDisabled(true);
//    menu->addSeparator();
//    menu->addAction(tr("Close Window"), this, SLOT(closeActiveWindow()),
//                    QKeySequence(tr("Ctrl+W", "Connection|Close Window")));

//    QMenu *svMenu = d->menuBar->addMenu(tr("Server"));
//    svMenu->addAction(tr("New Local Server..."), this, SLOT(startLocalServer()),
//                      QKeySequence(tr("Ctrl+N", "Server|New Local Server")));
//    d->stopAction = svMenu->addAction(tr("Stop"), this, SLOT(stopServer()));
//    svMenu->addSeparator();
//    svMenu->addMenu(d->localMenu);

//    connect(menu, SIGNAL(aboutToShow()), this, SLOT(updateMenu()));
//    connect(svMenu, SIGNAL(aboutToShow()), this, SLOT(updateMenu()));

//    // These will appear in the application menu:
//    menu->addAction(tr("Preferences..."), this, SLOT(showPreferences()), QKeySequence(tr("Ctrl+,")));
//    menu->addAction(tr("About"), this, SLOT(aboutShell()));

//    d->menuBar->addMenu(makeHelpMenu());
#endif

    d->localCheckTimer.audienceForTrigger() += [this]() { checkLocalServers(); };
    d->localCheckTimer.start();

//    newOrReusedConnectionWindow();
}

void GuiShellApp::initialize()
{
    addInitPackage("net.dengine.shell");

    initSubsystems();
    windowSystem().style().load(packageLoader().package("net.dengine.stdlib.gui"));

    d->imageBank.addFromInfo(FS::locate<const File>("/packs/net.dengine.shell/images.dei"));
    d->loadAllShaders();
}

void GuiShellApp::quitRequested()
{
    if (countOpenConnections() == 1)
    {
        // The window will ask for confirmation when receiving a close event.
        return;
    }
    // Too many or no open connections, so just quit without asking.
    GuiApp::quitRequested();
}

LinkWindow *GuiShellApp::newOrReusedConnectionWindow()
{
    LinkWindow *found = nullptr;

    // Look for a window with a closed connection.
    windowSystem().forAll([&found](GLWindow &w) {
        auto &win = w.as<LinkWindow>();
        if (!win.isConnected())
        {
            found = &win;
            found->raise();
            return LoopAbort;
        }
        return LoopContinue;
    });

    if (!found)
    {
        found = windowSystem().newWindow<LinkWindow>(Stringf("link%04u", windowSystem().count()));
//        connect(found, SIGNAL(linkOpened(LinkWindow*)),this, SLOT(updateMenu()));
//        connect(found, SIGNAL(linkClosed(LinkWindow*)), this, SLOT(updateMenu()));
//        connect(found, SIGNAL(closed(LinkWindow *)), this, SLOT(windowClosed(LinkWindow *)));

        found->show();

        // Initial position and size.
//        if (other)
//        {
//            found->move(other->pos() + QPoint(30, 30));
//        }
    }

    windowSystem().setFocusedWindow(found->id());

//    d->windows.prepend(found);
//    found->show();
    return found;
}

int GuiShellApp::countOpenConnections() const
{
    int count = 0;
    windowSystem().forAll([&count](GLWindow &w) {
        if (auto *win = maybeAs<LinkWindow>(&w))
        {
            if (win->isConnected()) ++count;
        }
        return LoopContinue;
    });
    return count;
}

GuiShellApp &GuiShellApp::app()
{
    return *static_cast<GuiShellApp *>(DE_BASE_GUI_APP);
}

//PopupMenuWidget &GuiShellApp::localServersMenu()
//{
//    return *d->localMenu;
//}

//QMenu *GuiShellApp::makeHelpMenu()
//{
//    QMenu *helpMenu = new QMenu(tr("&Help"));
//    helpMenu->addAction(tr("Shell Help"), this, SLOT(showHelp()));
//    return helpMenu;
//}

ServerFinder &GuiShellApp::serverFinder()
{
    return d->finder;
}

void GuiShellApp::connectToServer()
{
    LinkWindow *win = newOrReusedConnectionWindow();
    OpenDialog *dlg = new OpenDialog;
    dlg->setDeleteAfterDismissed(true);
    if (dlg->exec(win->root()))
    {
        win->openConnection(dlg->address());
    }
}

void GuiShellApp::connectToLocalServer()
{
//    QAction *act = dynamic_cast<QAction *>(sender());
//    Address host = act->data().value<Address>();

//    LinkWindow *win = newOrReusedConnectionWindow();
//    win->openConnection(convert(host.asText()));
}

void GuiShellApp::disconnectFromServer()
{
//    LinkWindow *win = dynamic_cast<LinkWindow *>(activeWindow());
//    if (win)
//    {
//        win->closeConnection();
//    }
}

void GuiShellApp::closeActiveWindow()
{
//    QWidget *win = activeWindow();
//    if (win) win->close();
}

void GuiShellApp::startLocalServer()
{
    try
    {
#ifdef MACOSX
        // App folder randomization means we can't find Doomsday.app on our own.
        if (!Config::get().has("Preferences.appFolder"))
        {
            showPreferences();
            return;
        }
#endif
        auto *win = &windowSystem().focusedWindow()->as<LinkWindow>();
        auto *dlg = new LocalServerDialog;
        dlg->setDeleteAfterDismissed(true);
        if (dlg->exec(win->root()))
        {
            StringList opts = dlg->additionalOptions();
            if (!Preferences::iwadFolder().isEmpty())
            {
                // TODO: Make the subdir recursion a setting.
                opts << "-iwadr" << Preferences::iwadFolder();
            }

            auto *sv = new LocalServer;
            sv->setApplicationPath(Config::get().gets("Preferences.appFolder"));
            if (!dlg->name().isEmpty())
            {
                sv->setName(dlg->name());
            }
            sv->start(dlg->port(),
                      dlg->gameMode(),
                      opts,
                      dlg->runtimeFolder());
            d->localServers[dlg->port()] = sv;

            newOrReusedConnectionWindow()->waitForLocalConnection(
                dlg->port(), sv->errorLogPath(), dlg->name());
        }
    }
    catch (const Error &er)
    {
        EscapeParser esc;
        esc.parse(er.asText());

        SDL_MessageBoxData mbox{};
        mbox.title = "Failed to Start Server";
        mbox.message = esc.plainText();
        SDL_ShowMessageBox(&mbox, nullptr);

        showPreferences();
    }
}

//void GuiShellApp::updateLocalServerMenu()
//{
//    d->localMenu->setDisabled(d->finder.foundServers().isEmpty());
//    d->localMenu->clear();

//    foreach (const Address &host, d->finder.foundServers())
//    {
//        QString label = QString("%1 - %2 (%3/%4)")
//                .arg(host.asText().c_str())
//                .arg(d->finder.name(host).c_str())
//                .arg(d->finder.playerCount(host))
//                .arg(d->finder.maxPlayers(host));

//        QAction *act = d->localMenu->addAction(label, this, SLOT(connectToLocalServer()));
//        act->setData(QVariant::fromValue(host));
//    }
//}

void GuiShellApp::aboutShell()
{
    auto &win = windowSystem().focusedWindow()->as<LinkWindow>();
    auto *about = new AboutDialog;
    about->setDeleteAfterDismissed(true);
    about->exec(win.root());
}

void GuiShellApp::showHelp()
{
    openBrowserUrl("https://manual.dengine.net/multiplayer/shell_help");
}

void GuiShellApp::openWebAddress(const String &url)
{
    openBrowserUrl(url);
}

void GuiShellApp::showPreferences()
{
    LinkWindow &win = windowSystem().focusedWindow()->as<LinkWindow>();

    auto *prefs = new Preferences;
    prefs->setDeleteAfterDismissed(true);
    prefs->exec(win.root());

//    if (!d->prefs)
//    {
//        d->prefs = new Preferences;
//        connect(d->prefs, SIGNAL(finished(int)), this, SLOT(preferencesDone()));
//        foreach (LinkWindow *win, d->windows)
//        {
//            connect(d->prefs, SIGNAL(consoleFontChanged()), win, SLOT(updateConsoleFontFromPreferences()));
//        }
//        d->prefs->show();
//    }
//    else
//    {
//        d->prefs->activateWindow();
//    }
}

//void GuiShellApp::updateMenu()
//{
//#ifdef MACOSX
//    LinkWindow *win = dynamic_cast<LinkWindow *>(activeWindow());
//    d->stopAction->setEnabled(win && win->isConnected());
//    d->disconnectAction->setEnabled(win && win->isConnected());
//#endif
//    updateLocalServerMenu();
//}

void GuiShellApp::checkLocalServers()
{
    List<int> stoppedPorts;
    for (auto iter = d->localServers.begin(); iter != d->localServers.end(); )
    {
        LocalServer *sv = iter->second;
        if (!sv->isRunning())
        {
            stoppedPorts << iter->first;
            delete sv;
            iter = d->localServers.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    for (int port : stoppedPorts)
    {
        DE_NOTIFY(LocalServerStop, i)
        {
            i->localServerStopped(port);
        }
    }
}

ImageBank &GuiShellApp::imageBank()
{
    return app().d->imageBank;
}

const GuiShellApp::MenuItems &GuiShellApp::localServerMenuItems() const
{
    return d->localServerMenuItems;
}
