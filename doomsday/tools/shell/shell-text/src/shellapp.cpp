/** @file shellapp.cpp Doomsday shell connection app.
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
#include "logwidget.h"
#include "commandlinewidget.h"
#include "statuswidget.h"
#include "openconnectiondialog.h"
#include <de/shell/LabelWidget>
#include <de/shell/MenuWidget>
#include <de/shell/Link>
#include <de/shell/Action>
#include <de/LogBuffer>
#include <QStringList>

using namespace de;
using namespace shell;

struct ShellApp::Instance
{
    ShellApp &self;
    MenuWidget *menu;
    LogWidget *log;
    CommandLineWidget *cli;
    LabelWidget *menuLabel;
    StatusWidget *status;
    Link *link;

    Instance(ShellApp &a) : self(a), link(0)
    {
        RootWidget &root = self.rootWidget();

        // Status bar in the bottom of the view.
        status = new StatusWidget;
        status->rule()
                .setInput(RuleRectangle::Height, *refless(new ConstantRule(1)))
                .setInput(RuleRectangle::Bottom, root.viewBottom())
                .setInput(RuleRectangle::Width,  root.viewWidth())
                .setInput(RuleRectangle::Left,   root.viewLeft());

        // Menu button at the left edge.
        menuLabel = new LabelWidget;
        menuLabel->setAlignment(AlignTop);
        menuLabel->setLabel(tr(" F9:Menu "));
        menuLabel->setAttribs(TextCanvas::Char::Bold);
        menuLabel->rule()
                .setInput(RuleRectangle::Left,   root.viewLeft())
                .setInput(RuleRectangle::Width,  *refless(new ConstantRule(menuLabel->label().size())))
                .setInput(RuleRectangle::Bottom, status->rule().top());

        menuLabel->addAction(new Action(KeyEvent(Qt::Key_F9), &self, SLOT(openMenu())));
        menuLabel->addAction(new Action(KeyEvent(Qt::Key_Z, KeyEvent::Control), &self, SLOT(openMenu())));
        menuLabel->addAction(new Action(KeyEvent(Qt::Key_X, KeyEvent::Control), &self, SLOT(openMenu())));
        menuLabel->addAction(new Action(KeyEvent(Qt::Key_C, KeyEvent::Control), &self, SLOT(openMenu())));

        // Expanding command line widget.
        cli = new CommandLineWidget;
        cli->rule()
                .setInput(RuleRectangle::Left,   menuLabel->rule().right())
                .setInput(RuleRectangle::Right,  root.viewRight())
                .setInput(RuleRectangle::Bottom, status->rule().top());

        menuLabel->rule().setInput(RuleRectangle::Top, cli->rule().top());

        // Log history covers the rest of the view.
        log = new LogWidget;
        log->rule()
                .setInput(RuleRectangle::Left,   root.viewLeft())
                .setInput(RuleRectangle::Width,  root.viewWidth())
                .setInput(RuleRectangle::Top,    root.viewTop())
                .setInput(RuleRectangle::Bottom, cli->rule().top());

        // Main menu.
        menu = new MenuWidget;
        menu->hide(); // closed initially
        menu->appendItem(new Action(tr("Connect to..."),
                                    KeyEvent("o"),
                                    &self, SLOT(openConnection())), "O");
        menu->appendItem(new Action(tr("Disconnect")));
        menu->appendSeparator();
        menu->appendItem(new Action(tr("Start new server")));
        menu->appendSeparator();
        menu->appendItem(new Action(tr("About")));
        menu->appendItem(new Action(tr("Quit Shell"),
                                    KeyEvent(Qt::Key_X, KeyEvent::Control),
                                    &self, SLOT(quit())), "Ctrl-X");
        menu->rule()
                .setInput(RuleRectangle::Bottom, menuLabel->rule().top())
                .setInput(RuleRectangle::Left,   menuLabel->rule().left());

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
    }

    ~Instance()
    {
        delete link;
    }
};

ShellApp::ShellApp(int &argc, char **argv)
    : CursesApp(argc, argv), d(new Instance(*this))
{
    LogBuffer &buf = LogBuffer::appBuffer();
    buf.setMaxEntryCount(50); // buffered here rather than appBuffer
    buf.addSink(d->log->logSink());

    QStringList args = arguments();
    if(args.size() > 1)
    {
        // Open a connection.
        d->link = new Link(Address(args[1]));
        d->status->setShellLink(d->link);

        connect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
        connect(d->link, SIGNAL(disconnected()), this, SLOT(disconnected()));
    }
}

ShellApp::~ShellApp()
{
    LogBuffer::appBuffer().removeSink(d->log->logSink());

    delete d;
}

void ShellApp::openConnection()
{
    RootWidget &root = rootWidget();

    OpenConnectionDialog dlg;
    dlg.exec(root);
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
    rootWidget().setFocus(d->menu);
    d->menu->open();
}

void ShellApp::menuClosed()
{
    d->menuLabel->setAttribs(TextCanvas::Char::Bold);
    rootWidget().setFocus(d->cli);
}
