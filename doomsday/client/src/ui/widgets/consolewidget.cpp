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
#include "ui/signalaction.h"

#include <de/ScalarRule>
#include <de/KeyEvent>
#include <de/MouseEvent>
#include <QCursor>

using namespace de;

DENG2_PIMPL(ConsoleWidget)
{
    ButtonWidget *button;
    ConsoleCommandWidget *cmdLine;
    LogWidget *log;
    ScalarRule *horizShift;
    ScalarRule *height;
    ScalarRule *width;

    enum GrabEdge {
        NotGrabbed = 0,
        RightEdge,
        TopEdge
    };

    bool opened;
    int grabWidth;
    GrabEdge grabHover;
    GrabEdge grabbed;

    Instance(Public *i)
        : Base(i),
          button(0),
          cmdLine(0),
          log(0),
          opened(true),
          grabWidth(0),
          grabHover(NotGrabbed),
          grabbed(NotGrabbed)
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
        // Cannot expand if the user is grabbing the top edge.
        if(grabbed == TopEdge) return;

        Animation::Style style = useOffsetAnimation? Animation::EaseOut : Animation::Linear;

        if(height->animation().target() == 0)
        {
            // On the first expansion make sure the margins are taken into account.
            delta += 2 * log->topMargin();
        }

        height->setStyle(style);
        height->set(height->animation().target() + delta, .25f);

        if(useOffsetAnimation)
        {
            // Sync the log content with the height animation.
            log->setContentYOffset(Animation::range(style,
                                                    log->contentYOffset().value() + delta, 0,
                                                    height->animation().remainingTime()));
        }
    }

    bool handleMousePositionEvent(MouseEvent const &mouse)
    {
        if(grabbed == RightEdge)
        {
            // Adjust width.
            width->set(mouse.pos().x - self.rule().left().valuei(), .1f);
            return true;
        }
        else if(grabbed == TopEdge)
        {
            if(mouse.pos().y < self.rule().bottom().valuei())
            {
                height->set(self.rule().bottom().valuei() - mouse.pos().y, .1f);
                log->enablePageKeys(false);
            }
            return true;
        }

        // Check for grab at the right edge.
        Rectanglei pos = self.rule().recti();
        pos.topLeft.x = pos.bottomRight.x - grabWidth;
        if(pos.contains(mouse.pos()))
        {
            if(grabHover != RightEdge)
            {
                grabHover = RightEdge;
                self.root().window().canvas().setCursor(Qt::SizeHorCursor);
            }
        }
        else
        {
            // Maybe a grab at the top edge, then.
            pos = self.rule().recti();
            pos.bottomRight.y = pos.topLeft.y + grabWidth;
            if(pos.contains(mouse.pos()))
            {
                if(grabHover != TopEdge)
                {
                    grabHover = TopEdge;
                    self.root().window().canvas().setCursor(Qt::SizeVerCursor);
                }
            }
            else if(grabHover != NotGrabbed)
            {
                grabHover = NotGrabbed;
                self.root().window().canvas().setCursor(Qt::ArrowCursor);
            }
        }

        return false;
    }
};

ConsoleWidget::ConsoleWidget() : GuiWidget("console"), d(new Instance(this))
{
    Rule const &unit = style().rules().rule("unit");

    d->button = new ButtonWidget;
    d->button->setText(_E(b) ">");
    // Until we have a popup menu widget...
    d->button->setAction(new SignalAction(this, SLOT(focusOnCommandLine())));
    add(d->button);

    d->cmdLine = new ConsoleCommandWidget("commandline");
    d->cmdLine->setEmptyContentHint("Enter commands here" /*  " _E(r)_E(l)_E(t) "SHIFT-ESC" */);
    add(d->cmdLine);

    connect(d->cmdLine, SIGNAL(gotFocus()), this, SLOT(openLog()));

    // Keep the button at the bottom of the expanding command line.
    //consoleButton->rule().setInput(Rule::Bottom, d->cmdLine->rule().bottom());

    d->button->setOpacity(.75f);
    d->cmdLine->setOpacity(.75f);

    // The Log is attached to the top of the command line.
    d->log = new LogWidget("log");
    d->log->rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, rule().bottom())
            .setInput(Rule::Top,    OperatorRule::maximum(rule().top(), Const(0)));
    add(d->log);

    // Blur the log background.
    enableBlur();

    connect(d->log, SIGNAL(contentHeightIncreased(int)), this, SLOT(logContentHeightIncreased(int)));

    // Width of the console is defined by the style.
    rule()
        .setInput(Rule::Width, OperatorRule::minimum(ClientWindow::main().root().viewWidth(),
                                                     OperatorRule::maximum(*d->width, Const(320))))
        .setInput(Rule::Height, *d->height)
        .setInput(Rule::Bottom, d->cmdLine->rule().top() - unit);

    closeLog();

    connect(d->cmdLine, SIGNAL(gotFocus()), this, SLOT(setFullyOpaque()));
    connect(d->cmdLine, SIGNAL(lostFocus()), this, SLOT(setTranslucent()));
}

ButtonWidget &ConsoleWidget::button()
{
    return *d->button;
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

bool ConsoleWidget::isLogOpen() const
{
    return d->opened;
}

void ConsoleWidget::enableBlur(bool yes)
{
    Background logBg = d->log->background();
    if(yes)
    {
        logBg.type = Background::Blurred;
    }
    else
    {
        logBg.type = Background::None;
    }
    d->log->set(logBg);
}

void ConsoleWidget::viewResized()
{
    if(!d->opened)
    {
        // Make sure it stays shifted out of the view.
        d->horizShift->set(-rule().width().valuei() - 1);
    }
}

void ConsoleWidget::update()
{
    GuiWidget::update();

    if(rule().top().valuei() <= 0)
    {
        // The expansion has reached the top of the screen, so we
        // can enable PageUp/Dn keys for the log.
        d->log->enablePageKeys(true);
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
    // Hovering over the right edge shows the <-> cursor.
    if(event.type() == Event::MousePosition)
    {
        if(d->handleMousePositionEvent(event.as<MouseEvent>()))
        {
            return true;
        }
    }

    // Dragging an edge resizes the widget.
    if(d->grabHover != Instance::NotGrabbed && event.type() == Event::MouseButton)
    {
        switch(handleMouseClick(event))
        {
        case MouseClickStarted:
            d->grabbed = d->grabHover;
            return true;

        case MouseClickAborted:
        case MouseClickFinished:
            d->grabbed = Instance::NotGrabbed;
            return true;

        default:
            break;
        }
    }

    if(event.type() == Event::MouseButton && hitTest(event))
    {
        // Prevent clicks from leaking through.
        return true;
    }

    if(event.type() == Event::KeyPress)
    {
        KeyEvent const &key = event.as<KeyEvent>();

        if(key.qtKey() == Qt::Key_PageUp ||
           key.qtKey() == Qt::Key_PageDown)
        {
            showFullLog();
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

void ConsoleWidget::openLog()
{
    if(d->opened) return;

    d->opened = true;
    d->horizShift->set(0, .3f);
}

void ConsoleWidget::closeLog()
{
    if(!d->opened) return;

    d->opened = false;
    d->horizShift->set(-rule().width().valuei() - 1, .3f);
}

void ConsoleWidget::clearLog()
{
    d->height->set(0);
    d->log->scrollToBottom();
    d->log->enablePageKeys(false);
}

void ConsoleWidget::showFullLog()
{
    d->log->enablePageKeys(true);
    d->expandLog(rule().top().valuei(), false);
}

void ConsoleWidget::logContentHeightIncreased(int delta)
{
    d->expandLog(delta, true);
}

void ConsoleWidget::setFullyOpaque()
{
    d->button->setOpacity(1, .25f);
    d->cmdLine->setOpacity(1, .25f);
}

void ConsoleWidget::setTranslucent()
{
    d->button->setOpacity(.75f, .25f);
    d->cmdLine->setOpacity(.75f, .25f);
}

void ConsoleWidget::focusOnCommandLine()
{
    root().setFocus(d->cmdLine);
}
