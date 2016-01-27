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
#include "ui/home/nogamescolumnwidget.h"

#include <de/LabelWidget>
#include <de/SequentialLayout>
#include <de/TabWidget>

using namespace de;

static TimeDelta const SCROLL_SPAN = .5;

DENG_GUI_PIMPL(HomeWidget)
{
    int visibleColumnCount = 3;
    int columnCount = 7;
    QList<ColumnWidget *> columns; // not owned
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

    void addColumn(ColumnWidget *col)
    {
        col->rule()
                .setInput(Rule::Width,  *columnWidth)
                .setInput(Rule::Height, self.rule().height());
        self.add(col);

        columns << col;
    }

    void updateLayout()
    {
        columnWidth->setSource(self.rule().width() / visibleColumnCount);

        // Lay out the columns from left to right.
        SequentialLayout layout(self.rule().left() - *scrollOffset,
                                self.rule().top(), ui::Right);
        for(ColumnWidget *column : columns)
        {
            if(column->isHidden()) continue;
            layout << *column;
        }
    }

    void switchTab(int dir)
    {
        auto pos = tabs->current();
        if(dir < 0 && pos > 0)
        {
            tabs->setCurrent(pos - 1);
        }
        else if(dir > 0 && pos < tabs->items().size() - 1)
        {
            tabs->setCurrent(pos + 1);
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

    void updateHighlightedTab()
    {
        for(int pos = 0; pos < columns.size(); ++pos)
        {
            columns[pos]->setHighlighted(tabs->currentItem().data().toInt() == pos);
        }
    }
};

HomeWidget::HomeWidget()
    : GuiWidget("home")
    , d(new Instance(this))
{
    ColumnWidget *column;

    // Create the columns.

    column = new NoGamesColumnWidget();
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
    d->updateHighlightedTab();

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
            d->switchTab(-1);
            return true;
        }
        else if(key.qtKey() == Qt::Key_Right)
        {
            d->switchTab(+1);
            return true;
        }
        else if(key.qtKey() == Qt::Key_Home)
        {
            d->tabs->setCurrent(0);
            return true;
        }
        else if(key.qtKey() == Qt::Key_End)
        {
            d->tabs->setCurrent(d->tabs->items().size() - 1);
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
    d->updateHighlightedTab();
}
