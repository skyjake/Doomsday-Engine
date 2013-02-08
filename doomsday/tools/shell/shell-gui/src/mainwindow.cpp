#include "mainwindow.h"
#include "qtrootwidget.h"
#include <de/LogBuffer>
#include <de/shell/LogWidget>
#include <de/shell/CommandLineWidget>
#include <de/shell/Link>
#include <QToolBar>
#include <QCloseEvent>
#include <QMessageBox>

using namespace de;
using namespace de::shell;

struct MainWindow::Instance
{
    LogBuffer logBuffer;
    LogWidget *log;
    CommandLineWidget *cli;
    Link *link;

    Instance() : link(0)
    {
        // Configure the log buffer.
        logBuffer.setMaxEntryCount(50); // buffered here rather than appBuffer
#ifdef _DEBUG
        logBuffer.enable(LogEntry::DEBUG);
#endif
        //buf.addSink(d->log->logSink());

        cli = new CommandLineWidget;
        log = new LogWidget;

        logBuffer.addSink(log->logSink());
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), d(new Instance)
{
    setUnifiedTitleAndToolBarOnMac(true);

    QtRootWidget *w = new QtRootWidget;
#ifdef MACOSX
    w->setFont(QFont("Monaco", 13));
#else
    w->setFont(QFont("Courier", 15));
#endif
    setCentralWidget(w);

    /*
    QToolBar *tools = addToolBar(tr("Connection"));
    tools->addAction("Connect");
    tools->addAction("Disconnect");*/

    TextRootWidget &root = w->rootWidget();

    // Set up the widgets.
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

    w->setFocus();

    resize(QSize(640, 480));

    setTitle(tr("Disconnected"));
}

MainWindow::~MainWindow()
{
    delete d;
}

void MainWindow::setTitle(const QString &title)
{
    setWindowTitle(title + " - " + tr("Doomsday Shell"));
}

bool MainWindow::isConnected() const
{
    return d->link && d->link->status() != Link::Disconnected;
}

void MainWindow::closeEvent(QCloseEvent *event)
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
}

void MainWindow::openConnection(QString address)
{
    closeConnection();

    qDebug() << "Opening connection to" << address;

    // Keep trying to connect to 30 seconds.
    d->link = new Link(address, 30);
    //d->status->setShellLink(d->link);

    connect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
    connect(d->link, SIGNAL(disconnected()), this, SLOT(disconnected()));

    setTitle(address);
}

void MainWindow::closeConnection()
{
    if(d->link)
    {
        qDebug() << "Closing existing connection to" << d->link->address().asText();

        // Get rid of the old connection.
        disconnect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
        disconnect(d->link, SIGNAL(disconnected()), this, SLOT(disconnected()));

        delete d->link;
        d->link = 0;
        //d->status->setShellLink(0);

        setTitle(tr("Disconnected"));
    }
}

void MainWindow::handleIncomingPackets()
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
            LogEntryPacket *entries = static_cast<LogEntryPacket *>(packet.data());
            foreach(LogEntry *e, entries->entries())
            {
                d->logBuffer.add(new LogEntry(*e));
            }
            break; }

        case shell::Protocol::ConsoleLexicon:
            // Terms for auto-completion.
            //d->cli->setLexicon(protocol.lexicon(*packet));
            break;

        default:
            break;
        }
    }
}

void MainWindow::disconnected()
{
    if(!d->link) return;

    // The link was disconnected.
    disconnect(d->link, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));

    d->link->deleteLater();
    d->link = 0;
    //d->status->setShellLink(0);

    setTitle(tr("Disconnected"));
}
