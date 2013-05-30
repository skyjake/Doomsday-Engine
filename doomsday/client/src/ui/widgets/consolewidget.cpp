/** @file consolewidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/consolewidget.h"
#include "ui/widgets/guirootwidget.h"
#include "ui/widgets/buttonwidget.h"
#include "ui/widgets/consolecommandwidget.h"
#include "ui/widgets/logwidget.h"

using namespace de;

DENG2_PIMPL(ConsoleWidget)
{
    ConsoleCommandWidget *cmdLine;
    LogWidget *log;

    Instance(Public *i)
        : Base(i),
          cmdLine(0),
          log(0)
    {
    }

    void glInit()
    {
    }

    void glDeinit()
    {
    }
};

ConsoleWidget::ConsoleWidget() : GuiWidget("taskbar"), d(new Instance(this))
{
    Rule const &gap = style().rules().rule("gap");

    ButtonWidget *consoleButton = new ButtonWidget;
    consoleButton->setText(DENG2_STR_ESCAPE("b") ">");
    consoleButton->rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Height, style().fonts().font("default").height() + gap * 2)
            .setInput(Rule::Width,  consoleButton->rule().height());
    add(consoleButton);

    // The task bar has a number of child widgets.
    d->cmdLine = new ConsoleCommandWidget("commandline");
    d->cmdLine->rule()
            .setInput(Rule::Left,   consoleButton->rule().right())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Right,  rule().right());
    add(d->cmdLine);

    // Keep the button at the top of the expanding command line.
    consoleButton->rule().setInput(Rule::Top, d->cmdLine->rule().top());

    // Log.
    d->log = new LogWidget("log");
    d->log->rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, d->cmdLine->rule().top())
            .setInput(Rule::Top,    rule().top());
    add(d->log);

    // Width of the console is defined by the style.
    rule().setInput(Rule::Width, style().rules().rule("console.width"));
}

ConsoleCommandWidget &ConsoleWidget::commandLine()
{
    return *d->cmdLine;
}

LogWidget &ConsoleWidget::log()
{
    return *d->log;
}

void ConsoleWidget::glInit()
{
    LOG_AS("ConsoleWidget");
    d->glInit();
}

void ConsoleWidget::glDeinit()
{
    d->glDeinit();
}

bool ConsoleWidget::handleEvent(const Event &event)
{
    return false;
}
