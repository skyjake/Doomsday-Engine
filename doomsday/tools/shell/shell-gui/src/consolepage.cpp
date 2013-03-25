#include "consolepage.h"
#include "preferences.h"
#include <QWheelEvent>
#include <QHBoxLayout>
#include <QDebug>

using namespace de;
using namespace de::shell;

ConsolePage::ConsolePage(QWidget *parent)
    : QWidget(parent),
      _log(0),
      _cli(0),
      _root(0),
      _logScrollBar(0),
      _wheelAccum(0)
{
    QHBoxLayout *hb = new QHBoxLayout;
    hb->setContentsMargins(0, 0, 0, 0);
    hb->setSpacing(0);
    setLayout(hb);
    _root = new QtRootWidget;
    hb->addWidget(_root, 1);
    _logScrollBar = new QScrollBar(Qt::Vertical);
    _logScrollBar->setMaximum(0);
    _logScrollBar->setDisabled(true);
    hb->addWidget(_logScrollBar);

    _root->setFont(Preferences::consoleFont());

    _cli = new CommandLineWidget;
    _log = new LogWidget;
    _log->setScrollIndicatorVisible(false); // we have our own

    // Set up the widgets.
    TextRootWidget &root = _root->rootWidget();
    _cli->rule()
            .setInput(Rule::Left,   root.viewLeft())
            .setInput(Rule::Width,  root.viewWidth())
            .setInput(Rule::Bottom, root.viewBottom());
    _log->rule()
            .setInput(Rule::Top,    root.viewTop())
            .setInput(Rule::Left,   root.viewLeft())
            .setInput(Rule::Right,  root.viewRight())
            .setInput(Rule::Bottom, _cli->rule().top());

    root.add(_log);
    root.add(_cli);
    root.setFocus(_cli);

    connect(_log,          SIGNAL(scrollPositionChanged(int)), this, SLOT(updateScrollPosition(int)));
    connect(_log,          SIGNAL(scrollMaxChanged(int)),      this, SLOT(updateMaxScroll(int)));
    connect(_logScrollBar, SIGNAL(sliderMoved(int)),           this, SLOT(scrollLogHistory(int)));
}

QtRootWidget &ConsolePage::root()
{
    return *_root;
}

LogWidget &ConsolePage::log()
{
    return *_log;
}

CommandLineWidget &ConsolePage::cli()
{
    return *_cli;
}

void ConsolePage::wheelEvent(QWheelEvent *ev)
{
    if(ev->orientation() == Qt::Vertical)
    {
        ev->accept();

        _wheelAccum += ev->delta();

        int linesToScroll = 0;
#ifdef MACOSX
        int const lineStep = 40;
#else
        int const lineStep = 60;
#endif
        while(_wheelAccum < -lineStep)
        {
            _wheelAccum += lineStep;
            linesToScroll--;
        }
        while(_wheelAccum > lineStep)
        {
            _wheelAccum -= lineStep;
            linesToScroll++;
        }
        if(linesToScroll)
        {
            int newPos = _log->scrollPosition() + linesToScroll;
            _log->scroll(newPos);
            updateScrollPosition(newPos);
            update();
        }
#ifndef MACOSX
        _wheelAccum = 0;
#endif
    }
    else
    {
        ev->ignore();
    }
}

void ConsolePage::updateScrollPosition(int pos)
{
    _logScrollBar->setValue(_log->maximumScroll() - pos);
}

void ConsolePage::updateMaxScroll(int maximum)
{
    _logScrollBar->setMaximum(maximum);
    _logScrollBar->setEnabled(maximum > 0);
    _logScrollBar->setPageStep(_log->scrollPageSize());
    _logScrollBar->setValue(_log->maximumScroll() - _log->scrollPosition());
}

void ConsolePage::scrollLogHistory(int pos)
{
    _log->scroll(_log->maximumScroll() - pos);
    update();
}
