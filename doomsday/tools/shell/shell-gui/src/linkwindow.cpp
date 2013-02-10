/** @file linkwindow.cpp  Window for a server link.
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

#include "linkwindow.h"
#include "statuswidget.h"
#include "qtrootwidget.h"
#include "qttextcanvas.h"
#include "guishellapp.h"
#include <de/LogBuffer>
#include <de/shell/LogWidget>
#include <de/shell/CommandLineWidget>
#include <de/shell/Link>
#include <QToolBar>
#include <QMenuBar>
#include <QTimer>
#include <QCloseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>

using namespace de;
using namespace de::shell;

DENG2_PIMPL(LinkWindow)
{
    LogBuffer logBuffer;
    LogWidget *log;
    CommandLineWidget *cli;
    Link *link;
    QToolButton *statusButton;
    QToolButton *consoleButton;
    QStackedWidget *stack;
    StatusWidget *status;
    QtRootWidget *root;
    QLabel *timeCounter;
    QLabel *currentHost;
    QAction *stopAction;

    Instance(Public &i)
        : Private(i),
          link(0),
          statusButton(0),
          consoleButton(0),
          stack(0),
          status(0),
          root(0),
          timeCounter(0),
          currentHost(0)
    {
        // Configure the log buffer.
        logBuffer.setMaxEntryCount(50); // buffered here rather than appBuffer
#ifdef _DEBUG
        logBuffer.enable(LogEntry::DEBUG);
#endif

        // Shell widgets.
        cli = new CommandLineWidget;
        log = new LogWidget;

        logBuffer.addSink(log->logSink());

        QObject::connect(cli, SIGNAL(commandEntered(de::String)), &self, SLOT(sendCommandToServer(de::String)));
    }

    void updateStyle()
    {
        if(self.isConnected())
        {
            root->canvas().setBackgroundColor(Qt::white);
            root->canvas().setForegroundColor(Qt::black);
        }
        else
        {
            root->canvas().setBackgroundColor(QColor(192, 192, 192));
            root->canvas().setForegroundColor(QColor(64, 64, 64));
        }
    }

    void updateCurrentHost()
    {
        QString txt;
        if(link)
        {
            if(self.isConnected() && !link->address().isNull())
            {
                txt = tr("<b>%1</b>:%2")
                        .arg(link->address().host().toString())
                        .arg(link->address().port());
            }
            else if(self.isConnected() && link->address().isNull())
            {
                txt = tr("Looking up host...");
            }
        }
#ifdef MACOSX
        currentHost->setText("<small>" + txt + "</small>");
#else
        currentHost->setText(txt);
#endif
    }

    void disconnected()
    {
        self.setTitle(tr("Disconnected"));
        root->setOverlaidMessage(tr("Disconnected"));
        self.statusBar()->clearMessage();
        stopAction->setDisabled(true);

        status->linkDisconnected();
        updateCurrentHost();
        updateStyle();
    }
};

LinkWindow::LinkWindow(QWidget *parent)
    : QMainWindow(parent), d(new Instance(*this))
{
    setUnifiedTitleAndToolBarOnMac(true);

    GuiShellApp *app = &GuiShellApp::app();

    d->stopAction = new QAction(tr("Stop Server"), this);
    connect(d->stopAction, SIGNAL(triggered()), app, SLOT(stopServer()));

#ifndef MACOSX
    // Menus are window-specific on non-Mac platforms.
    QMenu *menu = menuBar()->addMenu(tr("Server"));
    menu->addAction(tr("Connect..."), app, SLOT(connectToServer()),
                    QKeySequence(tr("Ctrl+O", "Server|Connect")));
    menu->addAction(tr("Disconnect"), this, SLOT(closeConnection()),
                    QKeySequence(tr("Ctrl+D", "Server|Disconnect")));
    menu->addSeparator();
    menu->addAction(tr("Start Local Server"), app, SLOT(startLocalServer()),
                    QKeySequence(tr("Ctrl+N", "Server|Start Local Server")));
    menu->addAction(d->stopAction);

    menu->addMenu(app->localServersMenu());

    menu->addSeparator();

    menu->addAction(tr("&Quit"), app, SLOT(quit()), QKeySequence(tr("Ctrl+Q")));
#endif

    d->stack = new QStackedWidget;

    // Status page.
    d->status = new StatusWidget;
    d->stack->addWidget(d->status);

    // Console page.
    d->root = new QtRootWidget;
    d->stack->addWidget(d->root);

#ifdef MACOSX
    d->root->setFont(QFont("Menlo", 13));
#else
    d->root->setFont(QFont("Fixedsys", 9));
#endif
    d->updateStyle();

    d->stack->setCurrentIndex(0); // status

    setCentralWidget(d->stack);

    // Status bar.
#ifdef MACOSX
    d->timeCounter = new QLabel("<small>0:00:00</small>");
#else
    d->timeCounter = new QLabel("0:00:00");
#endif
    d->currentHost = new QLabel;
    statusBar()->addPermanentWidget(d->currentHost);
    statusBar()->addPermanentWidget(d->timeCounter);

    QToolBar *tools = addToolBar(tr("View"));

    d->statusButton = new QToolButton;
    d->statusButton->setFocusPolicy(Qt::NoFocus);
    d->statusButton->setText(tr("Status"));
    d->statusButton->setCheckable(true);
    d->statusButton->setChecked(true);
    connect(d->statusButton, SIGNAL(pressed()), this, SLOT(switchToStatus()));
    tools->addWidget(d->statusButton);

    d->consoleButton = new QToolButton;
    d->consoleButton->setFocusPolicy(Qt::NoFocus);
    d->consoleButton->setText(tr("Console"));
    d->consoleButton->setCheckable(true);
    connect(d->consoleButton, SIGNAL(pressed()), this, SLOT(switchToConsole()));
    tools->addWidget(d->consoleButton);

    // Set up the widgets.
    TextRootWidget &root = d->root->rootWidget();
    d->cli->rule()
            .setInput(Rule::Left,   root.viewLeft())
            .setInput(Rule::Width,  root.viewWidth())
            .setInput(Rule::Bottom, root.viewBottom());
    d->log->rule()
            .setInput(Rule::Top,    root.viewTop())
            .setInput(Rule::Left,   root.viewLeft())
            .setInput(Rule::Right,  root.viewRight())
            .setInput(Rule::Bottom, d->cli->rule().top());

    root.add(d->log);
    root.add(d->cli);
    root.setFocus(d->cli);

    // Initial state for the window.
    resize(QSize(640, 480));

    d->root->setOverlaidMessage(tr("Disconnected"));
    setTitle(tr("Disconnected"));
    d->stopAction->setDisabled(true);
}

LinkWindow::~LinkWindow()
{
    delete d;
}

void LinkWindow::setTitle(const QString &title)
{
    setWindowTitle(title + " - " + tr("Doomsday Shell"));
}

bool LinkWindow::isConnected() const
{
    return d->link && d->link->status() != Link::Disconnected;
}

void LinkWindow::closeEvent(QCloseEvent *event)
{
    if(isConnected())
    {
        if(QMessageBox::question(
                    this,
                    tr("Close Connection?"),
                    tr("Connection is still open. Do you want to close it?"),
                    QMessageBox::Close | QMessageBox::Cancel) == QMessageBox::Cancel)
        {
            event->ignore();
            return;
        }
    }

    closeConnection();
    event->accept();

    emit closed(this);

    QMainWindow::closeEvent(event);
}

void LinkWindow::openConnection(QString address)
{
    closeConnection();

    qDebug() << "Opening connection to" << address;

    // Keep trying to connect to 30 seconds.
    d->link = new Link(address, 30);
    //d->status->setShellLink(d->link);

    connect(d->link, SIGNAL(addressResolved()), this, SLOT(addressResolved()));
    connect(d->link, SIGNAL(connected()), this, SLOT(connected()));
    connect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
    connect(d->link, SIGNAL(disconnected()), this, SLOT(disconnected()));

    setTitle(address);    
    d->root->setOverlaidMessage(tr("Looking up host..."));
    statusBar()->showMessage(tr("Looking up host..."));
    d->status->linkConnected(d->link);
    d->updateCurrentHost();
    d->updateStyle();
}

void LinkWindow::closeConnection()
{
    if(d->link)
    {
        qDebug() << "Closing existing connection to" << d->link->address().asText();

        // Get rid of the old connection.
        disconnect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
        disconnect(d->link, SIGNAL(disconnected()), this, SLOT(disconnected()));

        delete d->link;
        d->link = 0;

        d->disconnected();
    }
}

void LinkWindow::switchToStatus()
{
    d->consoleButton->setChecked(false);
    d->stack->setCurrentWidget(d->status);
}

void LinkWindow::switchToConsole()
{
    d->statusButton->setChecked(false);
    d->stack->setCurrentWidget(d->root);
    d->root->setFocus();
}

void LinkWindow::updateWhenConnected()
{
    if(d->link)
    {
        TimeDelta elapsed = d->link->connectedAt().since();
#ifdef MACOSX
        String time = String("<small>%1:%2:%3</small>")
#else
        String time = String("%1:%2:%3")
#endif
                .arg(int(elapsed.asHours()))
                .arg(int(elapsed.asMinutes()) % 60, 2, 10, QLatin1Char('0'))
                .arg(int(elapsed) % 60, 2, 10, QLatin1Char('0'));
        d->timeCounter->setText(time);

        QTimer::singleShot(1000, this, SLOT(updateWhenConnected()));
    }
}

void LinkWindow::handleIncomingPackets()
{
    forever
    {
        DENG2_ASSERT(d->link != 0);

        QScopedPointer<Packet> packet(d->link->nextPacket());
        if(packet.isNull()) break;

        // Process packet contents.
        shell::Protocol &protocol = d->link->protocol();
        switch(protocol.recognize(packet.data()))
        {
        case shell::Protocol::LogEntries: {
            // Add the entries into the local log buffer.
            LogEntryPacket *pkt = static_cast<LogEntryPacket *>(packet.data());
            foreach(LogEntry *e, pkt->entries())
            {
                d->logBuffer.add(new LogEntry(*e, LogEntry::Remote));
            }
            break; }

        case shell::Protocol::ConsoleLexicon:
            // Terms for auto-completion.
            d->cli->setLexicon(protocol.lexicon(*packet));
            break;

        case shell::Protocol::GameState: {
            Record &rec = static_cast<RecordPacket *>(packet.data())->record();
            d->status->setGameState(
                    rec["mode"].value().asText(),
                    rec["rules"].value().asText(),
                    rec["mapId"].value().asText(),
                    rec["mapTitle"].value().asText());
            break; }

        case shell::Protocol::MapOutline:
            d->status->setMapOutline(*static_cast<MapOutlinePacket *>(packet.data()));
            break;

        default:
            break;
        }
    }
}

void LinkWindow::sendCommandToServer(de::String command)
{
    if(d->link)
    {
        // Echo the command locally.
        LogEntry *e = new LogEntry(LogEntry::INFO, "", 0, ">",
                                   LogEntry::Args() << new LogEntry::Arg(command));
        d->logBuffer.add(e);

        QScopedPointer<Packet> packet(d->link->protocol().newCommand(command));
        *d->link << *packet;
    }
}

void LinkWindow::addressResolved()
{
    d->root->setOverlaidMessage(tr("Connecting..."));
    statusBar()->showMessage(tr("Connecting..."));
    d->updateCurrentHost();
}

void LinkWindow::connected()
{
    d->root->setOverlaidMessage("");
    d->status->linkConnected(d->link);
    statusBar()->clearMessage();
    updateWhenConnected();
    d->stopAction->setEnabled(true);
}

void LinkWindow::disconnected()
{
    if(!d->link) return;

    // The link was disconnected.
    disconnect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));

    d->link->deleteLater();
    d->link = 0;

    d->disconnected();
}
