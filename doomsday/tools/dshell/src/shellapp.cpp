/** @file shellapp.cpp  Doomsday shell connection app.
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

#include "shellapp.h"
#include "statuswidget.h"
#include "openconnectiondialog.h"
#include "localserverdialog.h"
#include "aboutdialog.h"

#include <doomsday/network/link.h>
#include <doomsday/network/localserver.h>
#include <de/term/labelwidget.h>
#include <de/term/menuwidget.h>
#include <de/term/commandlinewidget.h>
#include <de/term/logwidget.h>
#include <de/term/action.h>
#include <de/commandline.h>
#include <de/config.h>
#include <de/garbage.h>
#include <de/logbuffer.h>
#include <de/regexp.h>
#include <de/serverfinder.h>

using namespace de;
using namespace de::term;

DE_PIMPL(ShellApp)
, DE_OBSERVES(CommandLineWidget, Command)
{
    MenuWidget *       menu;
    LogWidget *        log;
    CommandLineWidget *cli;
    LabelWidget *      menuLabel;
    StatusWidget *     status;
    network::Link *    link = nullptr;
    ServerFinder       finder;

    Impl(Public &i) : Base(i)
    {
        RootWidget &root = self().rootWidget();

        // Status bar in the bottom of the view.
        status = new StatusWidget;
        status->rule()
                .setInput(Rule::Height, Const(1))
                .setInput(Rule::Bottom, root.viewBottom())
                .setInput(Rule::Width,  root.viewWidth())
                .setInput(Rule::Left,   root.viewLeft());

        // Menu button at the left edge.
        menuLabel = new LabelWidget;
        menuLabel->setAlignment(AlignTop);
        menuLabel->setLabel(" F9:Menu ");
        menuLabel->setAttribs(TextCanvas::AttribChar::Bold);
        menuLabel->rule()
                .setInput(Rule::Left,   root.viewLeft())
                .setInput(Rule::Width,  Constu(menuLabel->label().length()))
                .setInput(Rule::Bottom, status->rule().top());

        auto openMenu = [this](){ self().openMenu(); };

        menuLabel->addAction(new term::Action(KeyEvent(Key::F9),         openMenu));
        menuLabel->addAction(new term::Action(KeyEvent(Key::Substitute), openMenu));
        menuLabel->addAction(new term::Action(KeyEvent(Key::Break),      openMenu));
        menuLabel->addAction(new term::Action(KeyEvent(Key::Cancel), [this](){ self().quit(); }));

        // Expanding command line widget.
        cli = new CommandLineWidget;
        cli->rule()
                .setInput(Rule::Left,   menuLabel->rule().right())
                .setInput(Rule::Right,  root.viewRight())
                .setInput(Rule::Bottom, status->rule().top());

        menuLabel->rule().setInput(Rule::Top, cli->rule().top());

        // Log history covers the rest of the view.
        log = new LogWidget;
        log->rule()
                .setInput(Rule::Left,   root.viewLeft())
                .setInput(Rule::Width,  root.viewWidth())
                .setInput(Rule::Top,    root.viewTop())
                .setInput(Rule::Bottom, cli->rule().top());

        log->addAction(new term::Action(KeyEvent(Key::F5), [this]() { log->scrollToBottom(); }));

        // Main menu.
        menu = new MenuWidget(MenuWidget::Popup);
        menu->appendItem(new term::Action("Connect to...",
                                    [this]() { self().askToOpenConnection(); }));
        menu->appendItem(new term::Action("Disconnect", [this](){ self().closeConnection(); }));
        menu->appendSeparator();
        menu->appendItem(new term::Action("Start local server", [this](){ self().askToStartLocalServer(); }));
        menu->appendSeparator();
        menu->appendItem(new term::Action("Scroll to bottom", [this](){ log->scrollToBottom(); }), "F5");
        menu->appendItem(new term::Action("About", [this]() { self().showAbout(); }));
        menu->appendItem(new term::Action("Quit Shell", [this]() { self().quit(); }), "Ctrl-X");
        menu->rule()
                .setInput(Rule::Bottom, menuLabel->rule().top())
                .setInput(Rule::Left,   menuLabel->rule().left());

        // Compose the UI.
        root.add(status);
        root.add(cli);
        root.add(log);
        root.add(menuLabel);
        root.add(menu);

        root.setFocus(cli);

        // Signals.
        cli->audienceForCommand()  +=  this;
        menu->audienceForClose()   += [this](){ self().menuClosed(); };
        finder.audienceForUpdate() += [this](){ self().updateMenuWithFoundServers(); };
    }

    ~Impl() override
    {
        delete link;
    }

    void commandEntered(const String &command) override
    {
        self().sendCommandToServer(command);
    }
};

ShellApp::ShellApp(int &argc, char **argv)
    : CursesApp(argc, argv), d(new Impl(*this))
{
    auto &amd = metadata();
    amd.set(ORG_DOMAIN, "dengine.net");
    amd.set(ORG_NAME,   "Deng Team");
    amd.set(APP_NAME,   "dshell");
    amd.set(APP_VERSION, SHELL_VERSION);

    // Configure the log buffer.
    LogBuffer &buf = LogBuffer::get();
    buf.setMaxEntryCount(50); // buffered here rather than appBuffer
    buf.enableFlushing();
    buf.addSink(d->log->logSink());

    auto &cmdLine = commandLine();
    if (cmdLine.size() > 1)
    {
        // Open a connection.
        openConnection(cmdLine.at(1));
    }
}

ShellApp::~ShellApp()
{
    LogBuffer::get().removeSink(d->log->logSink());
}

void ShellApp::openConnection(const String &address)
{
    closeConnection();

    LogBuffer::get().flush();
    d->log->clear();

    LOG_NET_NOTE("Opening connection to %s") << address;

    // Keep trying to connect to 30 seconds.
    d->link = new network::Link(address, 30.0);
    d->status->setShellLink(d->link);

    d->link->audienceForPacketsReady() += [this]() { handleIncomingPackets(); };
    d->link->audienceForDisconnected() += [this]() { disconnected(); };

    d->link->connectLink();
}

void ShellApp::showAbout()
{
    AboutDialog().exec(rootWidget());
}

void ShellApp::closeConnection()
{
    if (d->link)
    {
        LOG_NET_NOTE("Closing existing connection to %s") << d->link->address();

        // Get rid of the old connection.
//        disconnect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
//        disconnect(d->link, SIGNAL(disconnected()), this, SLOT(disconnected()));
        delete d->link;
        d->link = nullptr;
        d->status->setShellLink(nullptr);
    }
}

void ShellApp::askForPassword()
{
    InputDialogWidget dlg;
    dlg.setDescription("The server requires a password.");
    dlg.setPrompt("Password: ");
    dlg.lineEdit().setEchoMode(LineEditWidget::PasswordEchoMode);
    dlg.lineEdit().setSignalOnEnter(false);

    if (dlg.exec(rootWidget()))
    {
        if (d->link) *d->link << d->link->protocol().passwordResponse(dlg.text());
    }
    else
    {
        Loop::timer(0.01, [this](){ closeConnection(); });
    }

    rootWidget().setFocus(d->cli);
}

void ShellApp::askToOpenConnection()
{
    OpenConnectionDialog dlg;
    dlg.exec(rootWidget());
    if (!dlg.address().isEmpty())
    {
        openConnection(dlg.address());
    }
}

void ShellApp::askToStartLocalServer()
{
    closeConnection();

    LocalServerDialog dlg;
    if (dlg.exec(rootWidget()))
    {
        StringList opts = dlg.text().split(RegExp::WHITESPACE);

        network::LocalServer sv;
        sv.start(dlg.port(), dlg.gameMode(), opts);

        openConnection("localhost:" + String::asText(dlg.port()));
    }
}

void ShellApp::updateMenuWithFoundServers()
{
    String oldSel = d->menu->itemAction(d->menu->cursor()).label();

    // Remove old servers.
    for (int i = 2; i < d->menu->itemCount() - 3; ++i)
    {
        if (d->menu->itemAction(i).label().first().isNumeric() ||
            d->menu->itemAction(i).label().beginsWith("localhost"))
        {
            d->menu->removeItem(i);
            --i;
        }
    }

    int pos = 2;
    for (const Address &sv : d->finder.foundServers())
    {
        String label = sv.asText() + Stringf(" (%s; %d/%d)",
                d->finder.name(sv).left(CharPos(20)).c_str(),
                d->finder.playerCount(sv),
                d->finder.maxPlayers(sv));

        d->menu->insertItem(pos++, new term::Action(label, [this]() { connectToFoundServer(); }));
    }

    // Update cursor position after changing menu items.
    d->menu->setCursorByLabel(oldSel);
}

void ShellApp::connectToFoundServer()
{
    String label = d->menu->itemAction(d->menu->cursor()).label();

    LOG_NOTE("Selected: ") << label;

    openConnection(label.left(label.indexOf('(') - 1));
}

void ShellApp::sendCommandToServer(const String& command)
{
    if (d->link)
    {
        LOG_NOTE(">") << command;

        std::unique_ptr<Packet> packet(d->link->protocol().newCommand(command));
        *d->link << *packet;
    }
}

void ShellApp::handleIncomingPackets()
{
    for (;;)
    {
        DE_ASSERT(d->link != nullptr);

        std::unique_ptr<Packet> packet(d->link->nextPacket());
        if (!packet) break;

        packet->execute();

        // Process packet contents.
        network::Protocol &protocol = d->link->protocol();
        switch (protocol.recognize(packet.get()))
        {
        case network::Protocol::PasswordChallenge:
            askForPassword();
            break;

        case network::Protocol::ConsoleLexicon:
            // Terms for auto-completion.
            d->cli->setLexicon(protocol.lexicon(*packet));
            break;

        case network::Protocol::GameState: {
            Record &rec = static_cast<RecordPacket *>(packet.get())->record();
            d->status->setGameState(
                    rec["mode"].value().asText(),
                    rec["rules"].value().asText(),
                    rec["mapId"].value().asText());
            break; }

        default:
            break;
        }

        LogBuffer::get().flush();
    }
}

void ShellApp::disconnected()
{
    if (!d->link) return;

    // The link was disconnected.
    d->link->audienceForPacketsReady().clear();
    trash(d->link);
    d->link = nullptr;
    d->status->setShellLink(nullptr);
}

void ShellApp::openMenu()
{
    d->menuLabel->setAttribs(TextCanvas::AttribChar::Reverse);
    d->menu->open();
}

void ShellApp::menuClosed()
{
    d->menuLabel->setAttribs(TextCanvas::AttribChar::Bold);
    rootWidget().setFocus(d->cli);
}
