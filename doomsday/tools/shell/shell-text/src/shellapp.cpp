/** @file shellapp.cpp  Doomsday shell connection app.
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

#include "shellapp.h"
#include "statuswidget.h"
#include "openconnectiondialog.h"
#include "localserverdialog.h"
#include "aboutdialog.h"
#include "persistentdata.h"
#include <de/shell/LabelWidget>
#include <de/shell/MenuWidget>
#include <de/shell/CommandLineWidget>
#include <de/shell/LogWidget>
#include <de/shell/Action>
#include <de/shell/Link>
#include <de/shell/LocalServer>
#include <de/shell/ServerFinder>
#include <de/LogBuffer>
#include <QStringList>
#include <QTimer>

using namespace de;
using namespace shell;

DENG2_PIMPL(ShellApp)
{
    PersistentData persist;
    MenuWidget *menu;
    LogWidget *log;
    CommandLineWidget *cli;
    LabelWidget *menuLabel;
    StatusWidget *status;
    Link *link;
    ServerFinder finder;

    Instance(Public &i) : Base(i), link(0)
    {
        RootWidget &root = self.rootWidget();

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
        menuLabel->setLabel(tr(" F9:Menu "));
        menuLabel->setAttribs(TextCanvas::Char::Bold);
        menuLabel->rule()
                .setInput(Rule::Left,   root.viewLeft())
                .setInput(Rule::Width,  Const(menuLabel->label().size()))
                .setInput(Rule::Bottom, status->rule().top());

        menuLabel->addAction(new shell::Action(KeyEvent(Qt::Key_F9), &self, SLOT(openMenu())));
        menuLabel->addAction(new shell::Action(KeyEvent(Qt::Key_Z, KeyEvent::Control), &self, SLOT(openMenu())));
        menuLabel->addAction(new shell::Action(KeyEvent(Qt::Key_C, KeyEvent::Control), &self, SLOT(openMenu())));
        menuLabel->addAction(new shell::Action(KeyEvent(Qt::Key_X, KeyEvent::Control), &self, SLOT(quit())));

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

        log->addAction(new shell::Action(KeyEvent(Qt::Key_F5), log, SLOT(scrollToBottom())));

        // Main menu.
        menu = new MenuWidget(MenuWidget::Popup);
        menu->appendItem(new shell::Action(tr("Connect to..."),
                                    &self, SLOT(askToOpenConnection())));
        menu->appendItem(new shell::Action(tr("Disconnect"), &self, SLOT(closeConnection())));
        menu->appendSeparator();
        menu->appendItem(new shell::Action(tr("Start local server"), &self, SLOT(askToStartLocalServer())));
        menu->appendSeparator();
        menu->appendItem(new shell::Action(tr("Scroll to bottom"), log, SLOT(scrollToBottom())), "F5");
        menu->appendItem(new shell::Action(tr("About"), &self, SLOT(showAbout())));
        menu->appendItem(new shell::Action(tr("Quit Shell"), &self, SLOT(quit())), "Ctrl-X");
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
        QObject::connect(cli, SIGNAL(commandEntered(de::String)), &self, SLOT(sendCommandToServer(de::String)));
        QObject::connect(menu, SIGNAL(closed()), &self, SLOT(menuClosed()));
        QObject::connect(&finder, SIGNAL(updated()), &self, SLOT(updateMenuWithFoundServers()));
    }

    ~Instance()
    {
        delete link;
    }
};

ShellApp::ShellApp(int &argc, char **argv)
    : CursesApp(argc, argv), d(new Instance(*this))
{
    // Metadata.
    setOrganizationDomain ("dengine.net");
    setOrganizationName   ("Deng Team");
    setApplicationName    ("doomsday-shell-text");
    setApplicationVersion (SHELL_VERSION);

    // Configure the log buffer.
    LogBuffer &buf = LogBuffer::appBuffer();
    buf.setMaxEntryCount(50); // buffered here rather than appBuffer
    buf.addSink(d->log->logSink());
#ifdef _DEBUG
    buf.enable(LogEntry::DEBUG);
#endif

    QStringList args = arguments();
    if(args.size() > 1)
    {
        // Open a connection.
        openConnection(args[1]);
    }
}

ShellApp::~ShellApp()
{
    LogBuffer::appBuffer().removeSink(d->log->logSink());
}

void ShellApp::openConnection(String const &address)
{
    closeConnection();

    LogBuffer::appBuffer().flush();
    d->log->clear();

    LOG_INFO("Opening connection to %s") << address;

    // Keep trying to connect to 30 seconds.
    d->link = new Link(address, 30);
    d->status->setShellLink(d->link);

    connect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
    connect(d->link, SIGNAL(disconnected()), this, SLOT(disconnected()));
}

void ShellApp::showAbout()
{
    AboutDialog().exec(rootWidget());
}

void ShellApp::closeConnection()
{
    if(d->link)
    {
        LOG_INFO("Closing existing connection to %s") << d->link->address();

        // Get rid of the old connection.
        disconnect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
        disconnect(d->link, SIGNAL(disconnected()), this, SLOT(disconnected()));
        delete d->link;
        d->link = 0;
        d->status->setShellLink(0);
    }
}

void ShellApp::askForPassword()
{
    InputDialog dlg;
    dlg.setDescription(tr("The server requires a password."));
    dlg.setPrompt("Password: ");
    dlg.lineEdit().setEchoMode(LineEditWidget::PasswordEchoMode);
    dlg.lineEdit().setSignalOnEnter(false);

    if(dlg.exec(rootWidget()))
    {
        if(d->link) *d->link << d->link->protocol().passwordResponse(dlg.text());
    }
    else
    {
        QTimer::singleShot(1, this, SLOT(closeConnection()));
    }

    rootWidget().setFocus(d->cli);
}

void ShellApp::askToOpenConnection()
{
    OpenConnectionDialog dlg;
    dlg.exec(rootWidget());
    if(!dlg.address().isEmpty())
    {
        openConnection(dlg.address());
    }
}

void ShellApp::askToStartLocalServer()
{
    closeConnection();

    LocalServerDialog dlg;
    if(dlg.exec(rootWidget()))
    {
        QStringList opts = dlg.text().split(' ', QString::SkipEmptyParts);

        LocalServer sv;
        sv.start(dlg.port(), dlg.gameMode(), opts);

        openConnection("localhost:" + String::number(dlg.port()));
    }
}

void ShellApp::updateMenuWithFoundServers()
{
    String oldSel = d->menu->itemAction(d->menu->cursor()).label();

    // Remove old servers.
    for(int i = 2; i < d->menu->itemCount() - 3; ++i)
    {
        if(d->menu->itemAction(i).label()[0].isDigit() ||
           d->menu->itemAction(i).label().startsWith("localhost"))
        {
            d->menu->removeItem(i);
            --i;
        }
    }

    int pos = 2;
    foreach(Address const &sv, d->finder.foundServers())
    {
        String label = sv.asText() + String(" (%1; %2/%3)")
                .arg(d->finder.name(sv).left(20))
                .arg(d->finder.playerCount(sv))
                .arg(d->finder.maxPlayers(sv));

        d->menu->insertItem(pos++, new shell::Action(label, this, SLOT(connectToFoundServer())));
    }

    // Update cursor position after changing menu items.
    d->menu->setCursorByLabel(oldSel);
}

void ShellApp::connectToFoundServer()
{
    String label = d->menu->itemAction(d->menu->cursor()).label();

    LOG_INFO("Selected: ") << label;

    openConnection(label.left(label.indexOf('(') - 1));
}

void ShellApp::sendCommandToServer(String command)
{
    if(d->link)
    {
        LOG_INFO(">") << command;

        QScopedPointer<Packet> packet(d->link->protocol().newCommand(command));
        *d->link << *packet;
    }
}

void ShellApp::handleIncomingPackets()
{
    forever
    {
        DENG2_ASSERT(d->link != 0);

        QScopedPointer<Packet> packet(d->link->nextPacket());
        if(packet.isNull()) break;

        packet->execute();

        // Process packet contents.
        shell::Protocol &protocol = d->link->protocol();
        switch(protocol.recognize(packet.data()))
        {
        case shell::Protocol::PasswordChallenge:
            askForPassword();
            break;

        case shell::Protocol::ConsoleLexicon:
            // Terms for auto-completion.
            d->cli->setLexicon(protocol.lexicon(*packet));
            break;

        case shell::Protocol::GameState: {
            Record &rec = static_cast<RecordPacket *>(packet.data())->record();
            d->status->setGameState(
                    rec["mode"].value().asText(),
                    rec["rules"].value().asText(),
                    rec["mapId"].value().asText());
            break; }

        default:
            break;
        }

        LogBuffer::appBuffer().flush();
    }
}

void ShellApp::disconnected()
{
    if(!d->link) return;

    // The link was disconnected.
    disconnect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
    d->link->deleteLater();
    d->link = 0;
    d->status->setShellLink(0);
}

void ShellApp::openMenu()
{
    d->menuLabel->setAttribs(TextCanvas::Char::Reverse);
    d->menu->open();
}

void ShellApp::menuClosed()
{
    d->menuLabel->setAttribs(TextCanvas::Char::Bold);
    rootWidget().setFocus(d->cli);
}
