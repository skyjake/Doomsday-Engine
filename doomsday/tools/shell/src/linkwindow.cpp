/** @file linkwindow.cpp  Window for a server link.
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

#include "linkwindow.h"
#include "statuswidget.h"
#include "qtrootwidget.h"
#include "qttextcanvas.h"
#include "guishellapp.h"
#include "optionspage.h"
#include "consolepage.h"
#include "preferences.h"
#include "errorlogdialog.h"
#include "utils.h"
#include <de/LogBuffer>
#include <de/shell/LogWidget>
#include <de/shell/CommandLineWidget>
#include <de/shell/Link>
#include <de/Garbage>
#include <QFile>
#include <QToolBar>
#include <QMenuBar>
#include <QInputDialog>
#include <QTimer>
#include <QCloseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QStatusBar>
#include <QScrollBar>
#include <QLabel>
#include <QCryptographicHash>

#ifndef MACOSX
#  define MENU_IN_LINK_WINDOW
#endif

using namespace de;
using namespace de::shell;

static QString statusText(const QString& txt)
{
#ifdef MACOSX
    return "<small>" + txt + "</small>";
#else
    return txt;
#endif
}

DE_PIMPL(LinkWindow)
, DE_OBSERVES(shell::CommandLineWidget, Command)
, DE_OBSERVES(shell::ServerFinder, Update)
{
    LogBuffer logBuffer;
    Link *link;
    duint16 waitingForLocalPort = 0;
    Time startedWaitingAt;
    QTimer waitTimeout;
    String linkName;
    NativePath errorLog;
    QToolBar *tools;
    QToolButton *statusButton;
    QToolButton *optionsButton;
    QToolButton *consoleButton;
    QStackedWidget *stack;
    QWidget *newLocalServerPage;
    StatusWidget *status;
    OptionsPage *options;
    ConsolePage *console;
    QLabel *gameStatus;
    QLabel *timeCounter;
    QLabel *currentHost;
    QAction *stopAction;
#ifdef MENU_IN_LINK_WINDOW
    QAction *disconnectAction;
#endif

    Impl(Public &i)
        : Base(i),
          link(0),
          tools(0),
          statusButton(0),
          consoleButton(0),
          stack(0),
          status(0),
          gameStatus(0),
          timeCounter(0),
          currentHost(0)
    {
        // Configure the log buffer.
        logBuffer.setMaxEntryCount(50); // buffered here rather than appBuffer
        logBuffer.setAutoFlushInterval(0.1);

        waitTimeout.setSingleShot(false);
        waitTimeout.setInterval(1000);
    }

    ~Impl() override
    {
        // Make sure the local sink is removed.
        LogBuffer::get().removeSink(console->log().logSink());
    }

    void updateStyle()
    {
        if (self().isConnected())
        {
            console->root().canvas().setBackgroundColor(Qt::white);
            console->root().canvas().setForegroundColor(Qt::black);
        }
        else
        {
            console->root().canvas().setBackgroundColor(QColor(192, 192, 192));
            console->root().canvas().setForegroundColor(QColor(64, 64, 64));
        }
    }

    void updateCurrentHost()
    {
        QString txt;
        if (link)
        {
            if (self().isConnected() && !link->address().isNull())
            {
                txt = tr("<b>%1</b>:%2")
                        .arg(link->address().isLocal()? "localhost"
                                                      : QString::fromUtf8(link->address().hostName()))
                        .arg(link->address().port());
            }
            else if (self().isConnected() && link->address().isNull())
            {
                txt = tr("Looking up host...");
            }
        }
        currentHost->setText(statusText(txt));
    }

    void disconnected()
    {
        self().setTitle(tr("Disconnected"));
        console->root().setOverlaidMessage(tr("Disconnected"));
        self().statusBar()->clearMessage();
        stopAction->setDisabled(true);
#ifdef MENU_IN_LINK_WINDOW
        disconnectAction->setDisabled(true);
#endif

        gameStatus->clear();
        status->linkDisconnected();
        updateCurrentHost();
        updateStyle();

        checkCurrentTab(false);
    }

    QString readErrorLogContents() const
    {
        QFile logFile(convert(errorLog));
        if (logFile.open(QFile::ReadOnly))
        {
            return QString::fromUtf8(logFile.readAll());
        }
        return QString();
    }

    bool checkForErrors()
    {
        return !readErrorLogContents().isEmpty();
    }

    void showErrorLog()
    {
        QString const text = readErrorLogContents();
        if (!text.isEmpty())
        {
            // Show a message box.
            ErrorLogDialog dlg;
            dlg.setLogContent(text);
            dlg.setMessage(tr("Failed to start the server. This may explain why:"));
            dlg.exec();
        }
    }

    QToolButton *addToolButton(QString const &label, QIcon const &icon)
    {
        QToolButton *tb = new QToolButton;
        tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        tb->setFocusPolicy(Qt::NoFocus);
        tb->setText(label);
        tb->setIcon(icon);
        tb->setCheckable(true);
#ifdef MACOSX
        // Tighter spacing, please.
        tb->setStyleSheet("padding-bottom:-1px");
#endif
        tools->addWidget(tb);
        return tb;
    }

    void updateStatusBarWithGameState(de::Record &rec)
    {
        String gameMode = rec["mode"].value().asText();
        String mapId    = rec["mapId"].value().asText();
        String rules    = rec["rules"].value().asText();

        String msg = gameMode;
        if (!mapId.isEmpty()) msg += " " + mapId;
        if (!rules.isEmpty()) msg += " (" + rules + ")";

        gameStatus->setText(statusText(convert(msg)));
    }

    void checkCurrentTab(bool connected)
    {
        if (stack->currentWidget() == newLocalServerPage && connected)
        {
            stack->setCurrentWidget(status);
        }
        else if (stack->currentWidget() == status && !connected)
        {
            stack->setCurrentWidget(newLocalServerPage);
        }
    }

    void commandEntered(const String &command) override
    {
        self().sendCommandToServer(command);
    }

    void foundServersUpdated() override
    {
        self().checkFoundServers();
    }
};

LinkWindow::LinkWindow(QWidget *parent)
    : QMainWindow(parent), d(new Impl(*this))
{
    setUnifiedTitleAndToolBarOnMac(true);
#ifndef MACOSX
    setWindowIcon(QIcon(":/images/shell.png"));
#endif

    GuiShellApp *app = &GuiShellApp::app();

    d->stopAction = new QAction(tr("S&top"), this);
    connect(d->stopAction, SIGNAL(triggered()), app, SLOT(stopServer()));

#ifdef MENU_IN_LINK_WINDOW
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Settings..."), app, SLOT(showPreferences()));
    fileMenu->addAction(tr("E&xit"), app, SLOT(quit()), QKeySequence(tr("Ctrl+Q")));

    // Menus are window-specific on non-Mac platforms.
    QMenu *menu = menuBar()->addMenu(tr("&Connection"));
    menu->addAction(tr("C&onnect..."), app, SLOT(connectToServer()),
                    QKeySequence(tr("Ctrl+O", "Connection|Connect")));
    d->disconnectAction = menu->addAction(tr("&Disconnect"), this, SLOT(closeConnection()),
                                          QKeySequence(tr("Ctrl+D", "Connection|Disconnect")));
    d->disconnectAction->setDisabled(true);

    QMenu *svMenu = menuBar()->addMenu(tr("&Server"));
    svMenu->addAction(tr("&New Local Server..."), app, SLOT(startLocalServer()),
                      QKeySequence(tr("Ctrl+N", "Server|New Local Server")));
    svMenu->addAction(d->stopAction);
    svMenu->addSeparator();
    svMenu->addMenu(app->localServersMenu());

    connect(svMenu, SIGNAL(aboutToShow()), app, SLOT(updateLocalServerMenu()));

    QMenu *helpMenu = app->makeHelpMenu();
    menuBar()->addMenu(helpMenu);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("About Doomsday Shell"), app, SLOT(aboutShell()));
#endif

    d->stack = new QStackedWidget;

    // Page for quickly starting a new local server.
    d->newLocalServerPage = new QWidget;
    d->stack->addWidget(d->newLocalServerPage);
    {
        auto *newButton = new QPushButton(tr("New Local Server..."));
        newButton->setMinimumWidth(200);
        connect(newButton, SIGNAL(pressed()), &GuiShellApp::app(), SLOT(startLocalServer()));

        auto *layout = new QVBoxLayout;
        layout->addStretch(1);
        layout->addWidget(newButton, 0, Qt::AlignCenter);
        layout->addStretch(1);
        d->newLocalServerPage->setLayout(layout);
    }

    // Status page.
    d->status = new StatusWidget;
    d->stack->addWidget(d->status);

    // Game options page.
    d->options = new OptionsPage;
    d->stack->addWidget(d->options);
    connect(d->options, SIGNAL(commandsSubmitted(QStringList)), this, SLOT(sendCommandsToServer(QStringList)));

    // Console page.
    d->console = new ConsolePage;
    d->stack->addWidget(d->console);
    d->logBuffer.addSink(d->console->log().logSink());
    d->console->cli().audienceForCommand() += d;

    d->updateStyle();

    d->stack->setCurrentIndex(0); // status

    setCentralWidget(d->stack);

    // Status bar.
#ifdef MACOSX
    QFont statusFont(font());
    statusFont.setPointSize(font().pointSize() * 4 / 5);
    statusBar()->setFont(statusFont);
#endif
    d->gameStatus = new QLabel;
    d->gameStatus->setContentsMargins(6, 0, 6, 0);
    d->currentHost = new QLabel;
    d->currentHost->setContentsMargins(6, 0, 6, 0);
    d->timeCounter = new QLabel(statusText("0:00:00"));
    d->timeCounter->setContentsMargins(6, 0, 0, 0);
    statusBar()->addPermanentWidget(d->gameStatus);
    statusBar()->addPermanentWidget(d->currentHost);
    statusBar()->addPermanentWidget(d->timeCounter);

    d->tools = addToolBar(tr("View"));
    d->tools->setMovable(false);
    d->tools->setFloatable(false);

    d->statusButton = d->addToolButton(tr("Status"), QIcon(imageResourcePath(":/images/toolbar_status.png")));
    d->statusButton->setShortcut(QKeySequence(tr("Ctrl+1")));
    connect(d->statusButton, SIGNAL(pressed()), this, SLOT(switchToStatus()));
    d->statusButton->setChecked(true);

#ifdef DE_DEBUG
    QIcon icon(imageResourcePath(":/images/toolbar_placeholder.png"));

    QToolButton *btn = d->addToolButton(tr("Frags"), icon);
    btn->setDisabled(true);

    btn = d->addToolButton(tr("Chat"), icon);
    btn->setDisabled(true);
#endif

    d->optionsButton = d->addToolButton(tr("Options"), QIcon(imageResourcePath(":/images/toolbar_options.png")));
    d->optionsButton->setShortcut(QKeySequence(tr("Ctrl+2")));
    connect(d->optionsButton, SIGNAL(pressed()), this, SLOT(switchToOptions()));

    d->consoleButton = d->addToolButton(tr("Console"), QIcon(imageResourcePath(":/images/toolbar_console.png")));
    d->consoleButton->setShortcut(QKeySequence(tr("Ctrl+3")));
    connect(d->consoleButton, SIGNAL(pressed()), this, SLOT(switchToConsole()));

    // Initial state for the window.
    resize(QSize(640, 480));

    d->console->root().setOverlaidMessage(tr("Disconnected"));
    setTitle(tr("Disconnected"));
    d->stopAction->setDisabled(true);

    // Observer local servers.
    GuiShellApp::app().serverFinder().audienceForUpdate() += d;
    connect(&GuiShellApp::app(), SIGNAL(localServerStopped(int)), this, SLOT(localServerStopped(int)));
    connect(&d->waitTimeout, SIGNAL(timeout()), this, SLOT(checkFoundServers()));
    d->waitTimeout.start();
}

void LinkWindow::setTitle(const QString &title)
{
    setWindowTitle(title + " - " + tr("Doomsday Shell"));
}

bool LinkWindow::isConnected() const
{
    return d->link && d->link->status() != Link::Disconnected;
}

void LinkWindow::changeEvent(QEvent *ev)
{
    if (ev->type() == QEvent::ActivationChange)
    {
        if (isActiveWindow())
        {
            // Log local messages here.
            LogBuffer::get().addSink(d->console->log().logSink());
        }
        else
        {
            LogBuffer::get().removeSink(d->console->log().logSink());
        }
    }
}

void LinkWindow::closeEvent(QCloseEvent *event)
{
    /*
    if (isConnected())
    {
        if (QMessageBox::question(
                    this,
                    tr("Close Connection?"),
                    tr("Connection is still open. Do you want to close the window regardless?"),
                    QMessageBox::Close | QMessageBox::Cancel) == QMessageBox::Cancel)
        {
            event->ignore();
            return;
        }
    }
    */

    closeConnection();
    event->accept();

    emit closed(this);

    QMainWindow::closeEvent(event);
}

void LinkWindow::waitForLocalConnection(duint16 localPort,
                                        NativePath const &errorLogPath,
                                        QString name)
{
    closeConnection();

    d->logBuffer.flush();
    d->console->log().clear();

    d->waitingForLocalPort = localPort;
    d->startedWaitingAt = Time();
    d->errorLog = errorLogPath;

    d->linkName = name + " - "  + tr("Local Server %1").arg(localPort);
    setTitle(d->linkName);

    d->console->root().setOverlaidMessage(tr("Waiting for local server..."));
    statusBar()->showMessage(tr("Waiting for local server..."));
    d->checkCurrentTab(true);
}

void LinkWindow::openConnection(Link *link, String name)
{
    closeConnection();

    d->logBuffer.flush();
    d->console->log().clear();

    d->link = link;

    connect(d->link, SIGNAL(addressResolved()), this, SLOT(addressResolved()));
    connect(d->link, SIGNAL(connected()), this, SLOT(connected()));
    connect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
    connect(d->link, SIGNAL(disconnected()), this, SLOT(disconnected()));

    if (!name.isEmpty())
    {
        d->linkName = name;
        setTitle(d->linkName);
    }
    d->console->root().setOverlaidMessage(tr("Looking up host..."));
    statusBar()->showMessage(tr("Looking up host..."));

    d->link->connectLink();
    d->status->linkConnected(d->link);
    d->checkCurrentTab(true);
    d->updateStyle();
}

void LinkWindow::openConnection(QString address)
{
    qDebug() << "Opening connection to" << address;

    // Keep trying to connect to 30 seconds.
    openConnection(new Link(address, 30), address);
}

void LinkWindow::closeConnection()
{
    d->waitingForLocalPort = 0;
    d->errorLog.clear();

    if (d->link)
    {
        qDebug() << "Closing existing connection to" << d->link->address().asText();

        // Get rid of the old connection.
        disconnect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
        disconnect(d->link, SIGNAL(disconnected()), this, SLOT(disconnected()));

        delete d->link;
        d->link = 0;

        emit linkClosed(this);
    }

    d->disconnected();
}

void LinkWindow::switchToStatus()
{
    d->optionsButton->setChecked(false);
    d->consoleButton->setChecked(false);
    d->stack->setCurrentWidget(d->link? d->status : d->newLocalServerPage);
}

void LinkWindow::switchToOptions()
{
    d->statusButton->setChecked(false);
    d->consoleButton->setChecked(false);
    d->stack->setCurrentWidget(d->options);
}

void LinkWindow::switchToConsole()
{
    d->statusButton->setChecked(false);
    d->optionsButton->setChecked(false);
    d->stack->setCurrentWidget(d->console);
    d->console->root().setFocus();
}

void LinkWindow::updateWhenConnected()
{
    if (d->link)
    {
        TimeSpan elapsed = d->link->connectedAt().since();
        String time = String("%1:%2:%3")
                .arg(int(elapsed.asHours()))
                .arg(int(elapsed.asMinutes()) % 60, 2, 10, QLatin1Char('0'))
                .arg(int(elapsed) % 60, 2, 10, QLatin1Char('0'));
        d->timeCounter->setText(statusText(time));

        QTimer::singleShot(1000, this, SLOT(updateWhenConnected()));
    }
}

void LinkWindow::handleIncomingPackets()
{
    forever
    {
        DE_ASSERT(d->link != 0);

        QScopedPointer<Packet> packet(d->link->nextPacket());
        if (packet.isNull()) break;

        //qDebug() << "Packet:" << packet->type();

        // Process packet contents.
        shell::Protocol &protocol = d->link->protocol();
        switch (protocol.recognize(packet.data()))
        {
        case shell::Protocol::PasswordChallenge:
            askForPassword();
            break;

        case shell::Protocol::LogEntries: {
            // Add the entries into the local log buffer.
            LogEntryPacket *pkt = static_cast<LogEntryPacket *>(packet.data());
            foreach (LogEntry *e, pkt->entries())
            {
                d->logBuffer.add(new LogEntry(*e, LogEntry::Remote));
            }
            // Flush immediately so we don't have to wait for the autoflush
            // to occur a bit later.
            d->logBuffer.flush();
            break; }

        case shell::Protocol::ConsoleLexicon:
            // Terms for auto-completion.
            d->console->cli().setLexicon(protocol.lexicon(*packet));
            break;

        case shell::Protocol::GameState: {
            Record &rec = static_cast<RecordPacket *>(packet.data())->record();
            String const rules = rec["rules"];
            QString gameType = rules.containsWord("dm")?  tr("Deathmatch")    :
                               rules.containsWord("dm2")? tr("Deathmatch II") :
                                                          tr("Co-op");
            d->status->setGameState(
                    convert(rec["mode"].value().asText()),
                    gameType,
                    convert(rec["mapId"].value().asText()),
                    convert(rec["mapTitle"].value().asText()));

            d->updateStatusBarWithGameState(rec);
            d->options->updateWithGameState(rec);
            break; }

        case shell::Protocol::MapOutline:
            d->status->setMapOutline(*static_cast<MapOutlinePacket *>(packet.data()));
            break;

        case shell::Protocol::PlayerInfo:
            d->status->setPlayerInfo(*static_cast<PlayerInfoPacket *>(packet.data()));
            break;

        default:
            break;
        }
    }
}

void LinkWindow::sendCommandToServer(const de::String &command)
{
    if (d->link)
    {
        // Echo the command locally.
        LogEntry *e = new LogEntry(LogEntry::Generic | LogEntry::Note, "", 0, ">",
                                   LogEntry::Args() << LogEntry::Arg::newFromPool(command));
        d->logBuffer.add(e);

        QScopedPointer<Packet> packet(d->link->protocol().newCommand(command));
        *d->link << *packet;
    }
}

void LinkWindow::sendCommandsToServer(QStringList commands)
{
    foreach (QString c, commands)
    {
        sendCommandToServer(convert(c));
    }
}

void LinkWindow::addressResolved()
{
    d->console->root().setOverlaidMessage(tr("Connecting..."));
    statusBar()->showMessage(tr("Connecting..."));
    d->updateCurrentHost();
    d->updateStyle();
}

void LinkWindow::connected()
{
    // Once successfully connected, we don't want to show error log any more.
    d->errorLog = "";

    if (d->linkName.isEmpty()) d->linkName = d->link->address().asText();
    setTitle(convert(d->linkName));
    d->updateCurrentHost();
    d->console->root().setOverlaidMessage("");
    d->status->linkConnected(d->link);
    statusBar()->clearMessage();
    updateWhenConnected();
    d->stopAction->setEnabled(true);
#ifdef MENU_IN_LINK_WINDOW
    d->disconnectAction->setEnabled(true);
#endif
    d->checkCurrentTab(true);

    emit linkOpened(this);
}

void LinkWindow::disconnected()
{
    if (!d->link) return;

    // The link was disconnected.
    d->link->audienceForPacketsReady() += [this](){ handleIncomingPackets(); };

    trash(d->link);
    d->link = 0;

    d->disconnected();

    emit linkClosed(this);
}

void LinkWindow::askForPassword()
{
    QInputDialog dlg(this);
    dlg.setWindowTitle(tr("Password Required"));
#ifdef WIN32
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
#endif
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setInputMode(QInputDialog::TextInput);
    dlg.setTextEchoMode(QLineEdit::Password);
    dlg.setLabelText(tr("Server password:"));

    if (dlg.exec() == QDialog::Accepted)
    {
        if (d->link)
        {
            *d->link << d->link->protocol().passwordResponse(dlg.textValue());
        }
        return;
    }

    QTimer::singleShot(1, this, SLOT(closeConnection()));
}

void LinkWindow::localServerStopped(int port)
{
    if (d->waitingForLocalPort == port)
    {
        if (!d->errorLog.isEmpty())
        {
            if (d->checkForErrors())
            {
                d->showErrorLog();
            }
        }
        closeConnection();
    }
}

void LinkWindow::updateConsoleFontFromPreferences()
{
    d->console->root().setFont(Preferences::consoleFont());
    d->console->update();
}

void LinkWindow::checkFoundServers()
{
    if (!d->waitingForLocalPort) return;

    auto const &finder = GuiShellApp::app().serverFinder();
    foreach (Address const &addr, finder.foundServers())
    {
        if (addr.isLocal() && addr.port() == d->waitingForLocalPort)
        {
            // This is the one!
            QTimer::singleShot(100, [this, addr] () { openConnection(new Link(addr)); });
            d->waitingForLocalPort = 0;
        }
    }
}
