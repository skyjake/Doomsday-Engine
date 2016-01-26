/** @file homewidget.cpp  Root widget for the Home UI.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/home/homewidget.h"
#include "ui/home/columnwidget.h"

#include <de/LabelWidget>
#include <de/SequentialLayout>
#include <de/TabWidget>

using namespace de;

static TimeDelta const SCROLL_SPAN = .5;

DENG_GUI_PIMPL(HomeWidget)
{
    int visibleColumnCount = 2;
    int columnCount = 7;
    IndirectRule *columnWidth;
    LabelWidget *tabsBackground;
    TabWidget *tabs;
    int currentOffsetTab = 0;
    ScalarRule *scrollOffset;

    Instance(Public *i) : Base(i)
    {
        columnWidth  = new IndirectRule;
        scrollOffset = new ScalarRule(0);
        scrollOffset->setStyle(Animation::EaseBoth);

        tabs = new TabWidget;
        tabsBackground = new LabelWidget;

        tabsBackground->set(Background(Vector4f(1, 1, 1, 1), Background::Blurred));
    }

    ~Instance()
    {
        releaseRef(columnWidth);
        releaseRef(scrollOffset);
    }

    void addColumn(GuiWidget *col)
    {
        col->rule()
                .setInput(Rule::Width,  *columnWidth)
                .setInput(Rule::Height, self.rule().height());
        self.add(col);
    }

    void updateLayout()
    {
        columnWidth->setSource(self.rule().width() / visibleColumnCount);

        // Lay out the columns from left to right.
        SequentialLayout layout(self.rule().left() - *scrollOffset,
                                self.rule().top(), ui::Right);
        for(Widget *w : self.childWidgets())
        {
            if(w->isHidden()) continue;
            if(ColumnWidget *column = w->maybeAs<ColumnWidget>())
            {
                layout << *column;
            }
        }
    }

    void scrollToTab(int pos, TimeDelta const &span)
    {
        pos = de::clamp(0, pos, columnCount - 1);

        Rangei const visible(currentOffsetTab, currentOffsetTab + visibleColumnCount);
        if(visible.contains(pos))
        {
            // No need to scroll anywhere, the requested tab is already visible.
            return;
        }
        if(pos < currentOffsetTab)
        {
            currentOffsetTab = pos;
        }
        else
        {
            currentOffsetTab = pos - visibleColumnCount + 1;
        }
        scrollOffset->set(*columnWidth * currentOffsetTab, span);
    }
};

HomeWidget::HomeWidget()
    : GuiWidget("home")
    , d(new Instance(this))
{
    ColumnWidget *column;

    // Create the columns.

    column = new ColumnWidget("nogames-column");
    d->addColumn(column);

    column = new ColumnWidget("doom-column");
    d->addColumn(column);

    column = new ColumnWidget("heretic-column");
    d->addColumn(column);

    column = new ColumnWidget("hexen-column");
    d->addColumn(column);

    column = new ColumnWidget("other-column");
    d->addColumn(column);

    column = new ColumnWidget("multiplayer-column");
    d->addColumn(column);

    column = new ColumnWidget("packages-column");
    d->addColumn(column);

    d->tabs->items()
            << new TabItem(tr("Data Files?"), 0)
            << new TabItem(   "Doom"        , 1)
            << new TabItem(   "Heretic"     , 2)
            << new TabItem(   "Hexen"       , 3)
            << new TabItem(tr("Other")      , 4)
            << new TabItem(tr("Multiplayer"), 5)
            << new TabItem(tr("Packages")   , 6);
    d->tabs->setCurrent(0);

    // Tabs on top.
    add(d->tabsBackground);
    add(d->tabs);

    // Define widget layout.
    Rule const &gap = style().rules().rule("gap");
    d->tabsBackground->rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Top,    rule().top())
            .setInput(Rule::Width,  rule().width())
            .setInput(Rule::Height, d->tabs->rule().height() + gap*2);
    d->tabs->rule()
            .setInput(Rule::Width,  rule().width())
            .setInput(Rule::Top,    rule().top() + gap)
            .setInput(Rule::Left,   rule().left());

    d->updateLayout();

    // Connections.
    connect(d->tabs, SIGNAL(currentTabChanged()), this, SLOT(tabChanged()));
}

void HomeWidget::viewResized()
{
    // Calculate appropriate number of columns.

    d->updateLayout();
}

bool HomeWidget::handleEvent(Event const &event)
{
    if(event.isKeyDown())
    {
        KeyEvent const &key = event.as<KeyEvent>();
        if(key.qtKey() == Qt::Key_Left)
        {
            //d->scrollToTab(d->currentOffsetTab - 1, SCROLL_SPAN);
            return true;
        }
        else if(key.qtKey() == Qt::Key_Right)
        {
            //d->scrollToTab(d->currentOffsetTab + 1, SCROLL_SPAN);
            return true;
        }
    }

    if(event.isMouse())
    {
        return true;
    }
    return false;
}

void HomeWidget::tabChanged()
{
    d->scrollToTab(d->tabs->currentItem().data().toInt(), SCROLL_SPAN);
}
