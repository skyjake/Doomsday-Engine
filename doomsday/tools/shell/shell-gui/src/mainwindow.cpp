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
    event->accept();
}
