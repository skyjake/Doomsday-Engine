#ifndef CONSOLEPAGE_H
#define CONSOLEPAGE_H

#include <QWidget>
#include <QScrollBar>
#include <QWheelEvent>
#include "qtrootwidget.h"
#include <de/shell/LogWidget>
#include <de/shell/CommandLineWidget>

class ConsolePage
    : public QWidget
    , DE_OBSERVES(de::shell::LogWidget, Scroll)
    , DE_OBSERVES(de::shell::LogWidget, Maximum)
{
    Q_OBJECT

public:
    explicit ConsolePage(QWidget *parent = 0);

    QtRootWidget &root();
    de::shell::LogWidget &log();
    de::shell::CommandLineWidget &cli();

    // Qt events.
    void wheelEvent(QWheelEvent *) override;

    void scrollPositionChanged(int pos) override;
    void scrollMaxChanged(int maximum) override;

signals:

public slots:
    void scrollLogHistory(int pos);

private:
    de::shell::LogWidget *_log;
    de::shell::CommandLineWidget *_cli;
    QtRootWidget *_root;
    QScrollBar *_logScrollBar;
    int _wheelAccum;
};

#endif // CONSOLEPAGE_H
