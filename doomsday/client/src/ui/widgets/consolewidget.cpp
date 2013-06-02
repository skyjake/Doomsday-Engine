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
#include "ui/clientwindow.h"

#include <de/ScalarRule>
#include <de/KeyEvent>
#include <de/MouseEvent>
#include <QCursor>

using namespace de;

DENG2_PIMPL(ConsoleWidget)
{
    ConsoleCommandWidget *cmdLine;
    LogWidget *log;
    ScalarRule *horizShift;
    ScalarRule *height;
    ScalarRule *width;

    bool opened;
    bool grabHover;
    int grabWidth;
    bool grabbed;

    Instance(Public *i)
        : Base(i),
          cmdLine(0),
          log(0),
          opened(true),
          grabHover(false),
          grabWidth(0),
          grabbed(false)
    {
        horizShift = new ScalarRule(0);
        width      = new ScalarRule(self.style().rules().rule("console.width").valuei());
        height     = new ScalarRule(0);

        grabWidth  = self.style().rules().rule("unit").valuei();
    }

    ~Instance()
    {
        releaseRef(horizShift);
        releaseRef(width);
        releaseRef(height);
    }

    void glInit()
    {
    }

    void glDeinit()
    {
    }

    void expandLog(int delta, bool useOffsetAnimation)
    {
        if(height->scalar().target() == 0)
        {
            // On the first expansion make sure the margin is taken into account.
            delta += log->topMargin();
        }
        height->set(height->scalar().target() + delta, .25f);

        if(self.rule().top().valuei() <= 0)
        {
            // The expansion has reached the top of the screen, so we
            // can enable PageUp/Dn keys for the log.
            log->enablePageKeys(true);
        }

        if(useOffsetAnimation)
        {
            // Sync the log content with the height animation.
            log->setContentYOffset(Animation::range(Animation::EaseIn, delta, 0, .25f));
        }
    }
};

ConsoleWidget::ConsoleWidget() : GuiWidget("Console"), d(new Instance(this))
{
    Rule const &gap = style().rules().rule("gap");

    ButtonWidget *consoleButton = new ButtonWidget;
    consoleButton->setText(DENG2_ESC("b") ">");
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
            .setInput(Rule::Top,    OperatorRule::maximum(rule().top(), Const(0)));
    add(d->log);

    connect(d->log, SIGNAL(contentHeightIncreased(int)), this, SLOT(logContentHeightIncreased(int)));

    // Width of the console is defined by the style.
    rule()
        .setInput(Rule::Width, OperatorRule::minimum(ClientWindow::main().root().viewWidth(),
                                                     OperatorRule::maximum(*d->width, Const(320))))
        .setInput(Rule::Height, d->cmdLine->rule().height() + *d->height);

    close();
}

ConsoleCommandWidget &ConsoleWidget::commandLine()
{
    return *d->cmdLine;
}

LogWidget &ConsoleWidget::log()
{
    return *d->log;
}

Rule const &ConsoleWidget::shift()
{
    return *d->horizShift;
}

bool ConsoleWidget::isOpen() const
{
    return d->opened;
}

void ConsoleWidget::viewResized()
{
    if(!d->opened)
    {
        // Make sure it stays shifted out of the view.
        d->horizShift->set(-rule().width().valuei() - 1);
    }
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

bool ConsoleWidget::handleEvent(Event const &event)
{
    if(!d->opened)
    {
        // Just check for the opening event.
        if(event.isKeyDown())
        {
            KeyEvent const &key = event.as<KeyEvent>();

            if(key.qtKey() == Qt::Key_Escape &&
               key.modifiers().testFlag(KeyEvent::Shift))
            {
                open();
                return true;
            }
        }
        return false;
    }

    // Hovering over the right edge shows the <-> cursor.
    if(event.type() == Event::MousePosition)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(d->grabbed)
        {
            // Adjust width.
            d->width->set(mouse.pos().x - rule().left().valuei(), .1f);
            return true;
        }

        Rectanglei pos = rule().recti();
        pos.topLeft.x = pos.bottomRight.x - d->grabWidth;
        if(pos.contains(mouse.pos()))
        {
            if(!d->grabHover)
            {
                d->grabHover = true;
                root().window().canvas().setCursor(Qt::SizeHorCursor);
            }
        }
        else if(d->grabHover)
        {
            d->grabHover = false;
            root().window().canvas().setCursor(Qt::ArrowCursor);
        }
    }

    // Dragging the right edge resizes the widget.
    if(d->grabHover && event.type() == Event::MouseButton)
    {
        switch(handleMouseClick(event))
        {
        case MouseClickStarted:
            d->grabbed = true;
            return true;

        case MouseClickAborted:
        case MouseClickFinished:
            d->grabbed = false;
            return true;

        default:
            break;
        }
    }

    if(event.type() == Event::KeyPress)
    {
        KeyEvent const &key = event.as<KeyEvent>();

        if(!d->grabbed && key.qtKey() == Qt::Key_Escape)
        {
            close();
            return true;
        }

        if(key.qtKey() == Qt::Key_PageUp ||
           key.qtKey() == Qt::Key_PageDown)
        {
            d->log->enablePageKeys(true);
            d->expandLog(rule().top().valuei(), false);
            return true;
        }

        if(key.qtKey() == Qt::Key_F5) // Clear history.
        {
            clearLog();
            return true;
        }
    }
    return false;
}

void ConsoleWidget::open()
{
    if(d->opened) return;

    d->opened = true;
    d->horizShift->set(0, .3f);

    if(hasRoot())
    {
        root().setFocus(d->cmdLine);
    }
}

void ConsoleWidget::close()
{
    if(!d->opened) return;

    d->opened = false;
    d->horizShift->set(-rule().width().valuei() - 1, .3f);

    if(hasRoot())
    {
        root().setFocus(0);
    }
}

void ConsoleWidget::clearLog()
{
    d->height->set(0);
    d->log->scrollToBottom();
    d->log->enablePageKeys(false);
}

void ConsoleWidget::logContentHeightIncreased(int delta)
{
    d->expandLog(delta, true);
}
