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
#include "ui/home/gamecolumnwidget.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>

#include <de/LabelWidget>
#include <de/SequentialLayout>
#include <de/TabWidget>

using namespace de;

static TimeDelta const SCROLL_SPAN = .5;

DENG_GUI_PIMPL(HomeWidget)
, DENG2_OBSERVES(Games, Readiness)
{
    dsize visibleColumnCount = 3; ///< Target.
    QList<ColumnWidget *> allColumns; // not owned
    QList<ColumnWidget *> columns; // Only the visible ones (not owned).
    IndirectRule *columnWidth;
    LabelWidget *tabsBackground;
    TabWidget *tabs;
    int currentOffsetTab = 0;
    ScalarRule *scrollOffset;

    Instance(Public *i) : Base(i)
    {
        DoomsdayApp::games().audienceForReadiness() += this;

        columnWidth  = new IndirectRule;
        scrollOffset = new ScalarRule(0);
        scrollOffset->setStyle(Animation::EaseOut);

        tabs = new TabWidget;

        tabsBackground = new LabelWidget;
        tabsBackground->set(Background(Vector4f(1, 1, 1, 1), Background::Blurred));
    }

    ~Instance()
    {
        DoomsdayApp::games().audienceForReadiness() -= this;

        releaseRef(columnWidth);
        releaseRef(scrollOffset);
    }

    void updateTabItems()
    {
        int index = 0;
        bool const gotGames = DoomsdayApp::games().numPlayable() > 0;

        tabs->items().clear();

        // Show columns depending on whether there are playable games.
        allColumns.at(0)->show(!gotGames);
        for(int i = 1; i < 6; ++i)
        {
            allColumns.at(i)->show(gotGames);
        }

        if(gotGames)
        {
            tabs->items()
                << new TabItem(   "Doom"        , index)
                << new TabItem(   "Heretic"     , index + 1)
                << new TabItem(   "Hexen"       , index + 2)
                << new TabItem(tr("Other")      , index + 3)
                << new TabItem(tr("Multiplayer"), index + 4);
            index += 5;
        }
        else
        {
            tabs->items() << new TabItem(tr("Data Files?"), index++);
        }

        tabs->items() << new TabItem(tr("Packages"), index++);
    }

    void gameReadinessUpdated()
    {
        updateTabItems();
        updateLayout();
    }

    void addColumn(ColumnWidget *col)
    {
        QObject::connect(col, SIGNAL(mouseActivity(QObject const *)),
                         thisPublic, SLOT(mouseActivityInColumn(QObject const *)));

        col->scrollArea().margins().setTop(tabsBackground->rule().height());
        col->rule()
                .setInput(Rule::Width,  *columnWidth)
                .setInput(Rule::Height, self.rule().height());
        self.add(col);
        allColumns << col;
    }

    void updateLayout()
    {
        columns.clear();
        columnWidth->setSource(self.rule().width() / de::min(visibleColumnCount,
                                                             tabs->items().size()));

        // Lay out the columns from left to right.
        SequentialLayout layout(self.rule().left() - *scrollOffset,
                                self.rule().top(), ui::Right);
        for(Widget *widget : self.childWidgets())
        {
            if(widget->isHidden())
            {
                continue;
            }
            if(ColumnWidget *column = widget->maybeAs<ColumnWidget>())
            {
                layout << *column;
                columns << column;
            }
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
        pos = de::clamp(0, pos, columns.size() - 1);

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

    column = new GameColumnWidget("doom-column", "id Software", "DOOM",
                                  "logo.game.doom");
    d->addColumn(column);

    column = new GameColumnWidget("heretic-column", "Raven Software", "Heretic",
                                  "logo.game.heretic");
    d->addColumn(column);

    column = new GameColumnWidget("hexen-column", "Raven Software", "Hexen",
                                  "logo.game.hexen");
    d->addColumn(column);

    column = new GameColumnWidget("other-column", "", "Other Games", "");
    d->addColumn(column);

    column = new ColumnWidget("multiplayer-column");
    d->addColumn(column);

    column = new ColumnWidget("packages-column");
    d->addColumn(column);

    d->updateTabItems();
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
    d->updateHighlightedTab();

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
        // Keyboard navigation between tabs.
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

void HomeWidget::mouseActivityInColumn(QObject const *columnWidget)
{
    for(int i = 0; i < d->columns.size(); ++i)
    {
        if(d->columns.at(i) == columnWidget)
        {
            d->tabs->setCurrent(i);
        }
    }
}
