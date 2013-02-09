#include "mainwindow.h"
#include "qtrootwidget.h"
#include "qttextcanvas.h"
#include <de/LogBuffer>
#include <de/shell/LogWidget>
#include <de/shell/CommandLineWidget>
#include <de/shell/Link>
#include <QToolBar>
#include <QCloseEvent>
#include <QMessageBox>

using namespace de;
using namespace de::shell;

DENG2_PIMPL(MainWindow)
{
    QtRootWidget *root;
    LogBuffer logBuffer;
    LogWidget *log;
    CommandLineWidget *cli;
    Link *link;

    Instance(Public &i) : Private(i), root(0), link(0)
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
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), d(new Instance(*this))
{
    setUnifiedTitleAndToolBarOnMac(true);

    d->root = new QtRootWidget;
#ifdef MACOSX
    d->root->setFont(QFont("Menlo", 13));
#else
    d->root->setFont(QFont("Courier", 15));
#endif
    d->updateStyle();
    setCentralWidget(d->root);

    /*
    QToolBar *tools = addToolBar(tr("Connection"));
    tools->addAction("Connect");
    tools->addAction("Disconnect");*/

    TextRootWidget &root = d->root->rootWidget();

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

    d->root->setFocus();

    resize(QSize(640, 480));

    d->root->setOverlaidMessage(tr("Disconnected"));
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

    emit closed(this);

    QMainWindow::closeEvent(event);
}

void MainWindow::openConnection(QString address)
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
    d->updateStyle();
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
        d->root->setOverlaidMessage(tr("Disconnected"));
        d->updateStyle();
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
            d->cli->setLexicon(protocol.lexicon(*packet));
            break;

        default:
            break;
        }
    }
}

void MainWindow::sendCommandToServer(de::String command)
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

void MainWindow::addressResolved()
{
    d->root->setOverlaidMessage(tr("Connecting..."));
}

void MainWindow::connected()
{
    d->root->setOverlaidMessage("");
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
    d->root->setOverlaidMessage(tr("Disconnected"));
    d->updateStyle();
}
