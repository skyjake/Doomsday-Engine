#include "mainwindow.h"
#include "qtrootwidget.h"
#include <de/shell/LogWidget>
#include <de/shell/CommandLineWidget>

using namespace de;
using namespace de::shell;

struct MainWindow::Instance
{
    LogWidget *log;
    CommandLineWidget *cli;

    Instance()
    {
        cli = new CommandLineWidget;
        log = new LogWidget;
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), d(new Instance)
{
    QtRootWidget *w = new QtRootWidget;
    w->setFont(QFont("Courier", 15));
    setCentralWidget(w);

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
}

MainWindow::~MainWindow()
{
    delete d;
}
