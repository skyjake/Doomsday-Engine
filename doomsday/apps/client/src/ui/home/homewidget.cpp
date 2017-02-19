/** @file homewidget.cpp  Root widget for the Home UI.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/home/multiplayercolumnwidget.h"
#include "ui/home/packagescolumnwidget.h"
#include "ui/widgets/busywidget.h"
#include "ui/savelistdata.h"
#include "ui/clientwindow.h"
#include "ui/widgets/taskbarwidget.h"

#include <doomsday/DoomsdayApp>
#include <doomsday/Games>

#include <de/App>
#include <de/FadeToBlackWidget>
#include <de/LabelWidget>
#include <de/Loop>
#include <de/PersistentState>
#include <de/PopupMenuWidget>
#include <de/SequentialLayout>
#include <de/StyleProceduralImage>
#include <de/TabWidget>

#include <QTimer>

using namespace de;

static TimeDelta const SCROLL_SPAN = .5;
static TimeDelta const DISMISS_SPAN = 1.5;

DENG_GUI_PIMPL(HomeWidget)
, DENG2_OBSERVES(App,          StartupComplete)
, DENG2_OBSERVES(Games,        Readiness)
, DENG2_OBSERVES(DoomsdayApp,  GameLoad)
, DENG2_OBSERVES(DoomsdayApp,  GameChange)
, DENG2_OBSERVES(Variable,     Change)
, DENG2_OBSERVES(ButtonWidget, StateChange)
{
    struct Column {
        ColumnWidget *widget;
        SafePtr<Variable> configVar;
        Column(ColumnWidget *w, Variable *var) : widget(w), configVar(var) {}
    };

    LoopCallback mainCall;
    SaveListData savedItems; ///< All the available save games as items.

    dsize visibleColumnCount = 2;
    QList<Column> allColumns; // not owned
    QList<ColumnWidget *> columns; // Only the visible ones (not owned).
    IndirectRule *columnWidth;
    LabelWidget *tabsBackground;
    TabWidget *tabs;
    SafeWidgetPtr<FadeToBlackWidget> blanker;
    int currentOffsetTab = 0;
    AnimationRule *scrollOffset;
    AnimationRule *dismissOffset;
    ButtonWidget *moveLeft;
    ButtonWidget *moveRight;
    QTimer moveShowTimer;
    ButtonWidget *taskBarHintButton;
    bool dismissing = false;

    int restoredOffsetTab = -1;
    int restoredActiveTab = -1;

    Impl(Public *i) : Base(i)
    {
        App::app().audienceForStartupComplete()     += this;
        DoomsdayApp::games().audienceForReadiness() += this;
        DoomsdayApp::app().audienceForGameChange()  += this;
        DoomsdayApp::app().audienceForGameLoad()    += this;

        columnWidth   = new IndirectRule;
        scrollOffset  = new AnimationRule(0, Animation::EaseOut);
        dismissOffset = new AnimationRule(0, Animation::EaseBoth);

        tabs = new TabWidget;

        tabsBackground = new LabelWidget;
        tabsBackground->set(Background(Vector4f(1, 1, 1, 1), Background::Blurred));

        // Create the column navigation buttons.
        moveLeft  = new ButtonWidget;
        moveRight = new ButtonWidget;
        //moveLeft ->setImage(new StyleProceduralImage("fold", *moveLeft, 90));
        //moveRight->setImage(new StyleProceduralImage("fold", *moveLeft, -90));
        moveLeft ->setActionFn([this] () { tabs->setCurrent(visibleTabRange().start - 1); });
        moveRight->setActionFn([this] () { tabs->setCurrent(visibleTabRange().end); });
        configureEdgeNavigationButton(*moveLeft);
        configureEdgeNavigationButton(*moveRight);

        // Hint/shortcut button for opening the task bar.
        taskBarHintButton = new ButtonWidget;
        taskBarHintButton->setBackgroundColor("");
        taskBarHintButton->setSizePolicy(ui::Expand, ui::Expand);
        taskBarHintButton->margins().set("dialog.gap");
        taskBarHintButton->setText(_E(b) "ESC" _E(.) + tr(" Task Bar"));
        taskBarHintButton->setTextColor("altaccent");
        taskBarHintButton->setFont("small");
        taskBarHintButton->setOpacity(.66f);
        taskBarHintButton->rule()
                .setInput(Rule::Right,  self().rule().right()  - rule("dialog.gap"))
                .setInput(Rule::Bottom, self().rule().bottom() - rule("dialog.gap") + *dismissOffset);
        taskBarHintButton->setActionFn([this] () {
            ClientWindow::main().taskBar().open();
        });

        // The task bar is created later, so defer the signal connects.
        mainCall.enqueue([this] ()
        {
            QObject::connect(&ClientWindow::main().taskBar(), &TaskBarWidget::opened, [this] ()
            {
                taskBarHintButton->disable();
                taskBarHintButton->setOpacity(0, 0.25);
            });
            QObject::connect(&ClientWindow::main().taskBar(), &TaskBarWidget::closed, [this] ()
            {
                taskBarHintButton->enable();
                taskBarHintButton->setOpacity(.66f, 0.5);
            });
        });

        // The navigation buttons should be hidden with a delay or otherwise the user
        // may inadvertely click on them right after they're gone.
        moveShowTimer.setSingleShot(true);
        moveShowTimer.setInterval(SCROLL_SPAN.asMilliSeconds());
        QObject::connect(&moveShowTimer, &QTimer::timeout, [this] ()
        {
            moveLeft ->show(tabs->current() != 0);
            moveRight->show(tabs->current() < tabs->items().size() - 1);
        });
    }

    ~Impl()
    {
        releaseRef(columnWidth);
        releaseRef(scrollOffset);
        releaseRef(dismissOffset);
    }

    void configureEdgeNavigationButton(ButtonWidget &button)
    {
        // Edge navigation buttons are only visible when hoving on them.
        button.set(Background(style().colors().colorf("text")));
        //button.setImageColor(style().colors().colorf("inverted.text") + Vector4f(0, 0, 0, 2));
        //button.setOverrideImageSize(style().fonts().font("default").height().value() * 2);
        button.setOpacity(0);
        button.setBehavior(Widget::Focusable, UnsetFlags); // only for the mouse
        button.setAttribute(EatAllMouseEvents, SetFlags);
        button.audienceForStateChange() += this;

        button.rule()
                .setInput(Rule::Width,  rule(RuleBank::UNIT))
                .setInput(Rule::Bottom, self().rule().bottom())
                .setInput(Rule::Top,    tabs->rule().bottom());
    }

    void buttonStateChanged(ButtonWidget &button, ButtonWidget::State state)
    {
        // Hide navigation buttons when they are not being used.
        if (state == ButtonWidget::Down)
        {
            button.setOpacity(.3f);
        }
        else
        {
            button.setOpacity(state == ButtonWidget::Up? 0 : .1f, 0.25);
        }
    }

    void updateVisibleColumnsAndTabs()
    {
        bool const gotGames = DoomsdayApp::games().numPlayable() > 0;

        tabs->items().clear();

        // Show columns depending on whether there are playable games.
        allColumns.at(0).widget->show(!gotGames);
        for (int i = 1; i < allColumns.size(); ++i)
        {
            Column const &col = allColumns.at(i);
            if (col.configVar)
            {
                col.widget->show(gotGames && col.configVar->value().isTrue());
            }
        }

        // Tab headings for visible columns.
        int index = 0;
        for (Column const &col : allColumns)
        {
            if (!col.widget->behavior().testFlag(Widget::Hidden))
            {
                auto *tabItem = new TabItem(col.widget->tabHeading(), index++);
                tabItem->setShortcutKey(col.widget->tabShortcut());
                tabs->items() << tabItem;
            }
        }
    }

    void appStartupCompleted()
    {
        blanker->start(0.25);
    }

    void gameReadinessUpdated()
    {
        updateVisibleColumnsAndTabs();
        calculateColumnCount();
        updateLayout();

        // Restore previous state?
        if (restoredActiveTab >= 0)
        {
            currentOffsetTab = restoredOffsetTab;
            setScrollOffset(currentOffsetTab, 0.0);
            tabs->setCurrent(ui::DataPos(restoredActiveTab));

            restoredActiveTab = -1;
            restoredOffsetTab = -1;
        }
    }

    void aboutToLoadGame(Game const &gameBeingLoaded)
    {
        self().root().clearFocusStack();
        self().root().setFocus(nullptr);

        if (gameBeingLoaded.isNull())
        {
            moveOnscreen();
        }
        else
        {
            TimeDelta span = DISMISS_SPAN;
            auto &win = self().root().window().as<ClientWindow>();
            if (win.isGameMinimized())
            {
                win.busy().clearTransitionFrameToBlack();
                span = 1.0;
            }
            moveOffscreen();
        }
    }

    void currentGameChanged(Game const &newGame)
    {
        if (!newGame.isNull())
        {
            ClientWindow::main().fadeContent(ClientWindow::FadeFromBlack, 1.0);
            self().root().clearFocusStack();
            self().root().setFocus(nullptr);
        }
    }

    void variableValueChanged(Variable &, Value const &)
    {
        updateVisibleColumnsAndTabs();
        calculateColumnCount();
        updateLayout();
    }

    void moveOffscreen(TimeDelta span = DISMISS_SPAN)
    {
        self().disable();
        self().setBehavior(DisableEventDispatchToChildren);
        self().root().clearFocusStack();
        self().root().setFocus(nullptr);

        // Home is being moved offscreen, so the game can take over in full size.
        ClientWindow::main().setGameMinimized(false);

        if (fequal(dismissOffset->animation().target(), 0.f))
        {
            dismissOffset->set(-self().rule().height(), span);
            dismissing = true;
        }
    }

    void moveOnscreen(TimeDelta span = DISMISS_SPAN)
    {
        if (!fequal(dismissOffset->animation().target(), 0.f))
        {
            self().show();
            dismissOffset->set(0, span);
            self().enable();
            self().setBehavior(DisableEventDispatchToChildren, false);
        }
    }

    void checkDismissHiding()
    {
        if (dismissing && dismissOffset->animation().done())
        {
            self().hide();
            dismissing = false;
        }
    }

    void addColumn(ColumnWidget *col)
    {
        QObject::connect(col, SIGNAL(mouseActivity(QObject const *)),
                         thisPublic, SLOT(mouseActivityInColumn(QObject const *)));

        col->scrollArea().margins().setTop(tabsBackground->rule().height());
        col->rule()
                .setInput(Rule::Width,  *columnWidth)
                .setInput(Rule::Height, self().rule().height());
        self().add(col);

        Variable *conf = col->configVariable();
        if (conf)
        {
            conf->audienceForChange() += this;
        }
        allColumns << Column(col, conf);
    }

    void calculateColumnCount()
    {
        visibleColumnCount = de::min(de::max(dsize(1), dsize(self().rule().width().valuei() /
                                                             rule("home.column.width").valuei())),
                                     tabs->items().size());
    }

    void updateLayout()
    {
        columns.clear();
        columnWidth->setSource(self().rule().width() / visibleColumnCount);

        // Lay out the columns from left to right.
        SequentialLayout layout(self().rule().left() - *scrollOffset,
                                self().rule().top()  + *dismissOffset,
                                ui::Right);
        for (Widget *widget : self().childWidgets())
        {
            if (!widget->behavior().testFlag(Widget::Hidden))
            {
                if (ColumnWidget *column = widget->maybeAs<ColumnWidget>())
                {
                    layout << *column;
                    columns << column;
                }
            }
        }

        updateHighlightedTab();

        // Make sure we stay within the valid range.
        if (!visibleTabRange().contains(int(tabs->current())))
        {
            currentOffsetTab = int(tabs->current());
            setScrollOffset(currentOffsetTab, 0.0);
        }
        if (visibleTabRange().end >= columns.size())
        {
            currentOffsetTab = columns.size() - int(visibleColumnCount);
            setScrollOffset(currentOffsetTab, 0.0);
        }
    }

    Rangei visibleTabRange() const
    {
        return Rangei(currentOffsetTab, currentOffsetTab + visibleColumnCount);
    }

    void switchTab(int dir, bool scrollInstantly = false)
    {
        if (scrollInstantly && dir)
        {
            Rangei const vis = visibleTabRange();
            tabs->setCurrent(dir < 0? vis.start : vis.end - 1);
        }

        auto pos = tabs->current();
        if (dir < 0 && pos > 0)
        {
            tabs->setCurrent(pos - 1);
        }
        else if (dir > 0 && pos < tabs->items().size() - 1)
        {
            tabs->setCurrent(pos + 1);
        }
    }

    /**
     * Scrolls the view so that column @a pos is visible.
     * @param pos   Column/tab index.
     * @param span  Animation duration.
     */
    void scrollToTab(int pos, TimeDelta span)
    {
        pos = de::clamp(0, pos, columns.size() - 1);

        if (currentOffsetTab + int(visibleColumnCount) > columns.size())
        {
            // Don't let the visible range extend outside the view.
            currentOffsetTab = columns.size() - visibleColumnCount;
            span = 0;
        }
        else if (visibleTabRange().contains(pos))
        {
            // No need to scroll anywhere, the requested tab is already visible.
            return;
        }
        if (pos < currentOffsetTab)
        {
            currentOffsetTab = pos;
        }
        else
        {
            currentOffsetTab = pos - visibleColumnCount + 1;
        }
        setScrollOffset(currentOffsetTab, span);
    }

    void setScrollOffset(int tab, TimeDelta const &span)
    {
        scrollOffset->set(*columnWidth * tab, span);
    }

    void updateHighlightedTab()
    {
        if (columns.isEmpty()) return;

        // Are we still in a valid tab?
        if (tabs->current() >= tabs->items().size())
        {
            tabs->setCurrent(tabs->items().size() - 1); // calls back here via observer
            return;
        }

        // Remove the highlight.
        for (int pos = 0; pos < columns.size(); ++pos)
        {
            if (columns[pos]->isHighlighted())
            {
                columns[pos]->setHighlighted(false);
            }
        }

        // Set new highlight.
        columns[tabs->currentItem().data().toInt()]->setHighlighted(true);

        moveShowTimer.start();
    }

    int highlightedTab() const
    {
        for (int pos = 0; pos < columns.size(); ++pos)
        {
            if (columns[pos]->isHighlighted())
            {
                return pos;
            }
        }
        return -1;
    }
};

HomeWidget::HomeWidget()
    : GuiWidget("home")
    , d(new Impl(this))
{
    setAttribute(ManualOpacity);

    // Create the columns.
    ColumnWidget *column;

    column = new NoGamesColumnWidget();
    d->addColumn(column);

    column = new GameColumnWidget("DOOM", d->savedItems);
    d->addColumn(column);

    column = new GameColumnWidget("Heretic", d->savedItems);
    d->addColumn(column);

    column = new GameColumnWidget("Hexen", d->savedItems);
    d->addColumn(column);

    column = new GameColumnWidget("", d->savedItems);
    d->addColumn(column);

    column = new MultiplayerColumnWidget();
    d->addColumn(column);

    column = new PackagesColumnWidget();
    d->addColumn(column);

    d->updateVisibleColumnsAndTabs();
    d->tabs->setCurrent(0);

    // Tabs on top.
    add(d->tabsBackground);
    add(d->tabs);

    // Hidden navigation buttons.
    add(d->moveLeft);
    add(d->moveRight);
    d->moveLeft ->rule().setInput(Rule::Left,  rule().left());
    d->moveRight->rule().setInput(Rule::Right, rule().right());

    add(d->taskBarHintButton);

    // Hide content until first update.
    d->blanker.reset(new FadeToBlackWidget);
    d->blanker->rule().setRect(rule());
    d->blanker->initFadeFromBlack(0.75);
    add(d->blanker);

    // Define widget layout.
    Rule const &gap = rule("gap");
    d->tabsBackground->rule()
            .setInput(Rule::Left,   rule().left())
            .setInput(Rule::Top,    rule().top() + *d->dismissOffset)
            .setInput(Rule::Width,  rule().width())
            .setInput(Rule::Height, d->tabs->rule().height() + gap*2);
    d->tabs->rule()
            .setInput(Rule::Width,  rule().width())
            .setInput(Rule::Top,    rule().top() + gap + *d->dismissOffset)
            .setInput(Rule::Left,   rule().left());

    d->updateLayout();

    // Connections.
    connect(d->tabs, SIGNAL(currentTabChanged()), this, SLOT(tabChanged()));
}

void HomeWidget::viewResized()
{
    d->calculateColumnCount();
    d->updateLayout();
}

bool HomeWidget::handleEvent(Event const &event)
{
    if (event.isKeyDown())
    {
        // Keyboard navigation between tabs.
        KeyEvent const &key = event.as<KeyEvent>();
        if (key.qtKey() == Qt::Key_Left)
        {
            d->switchTab(-1);
            return true;
        }
        else if (key.qtKey() == Qt::Key_Right)
        {
            d->switchTab(+1);
            return true;
        }
        else if (key.qtKey() == Qt::Key_Home)
        {
            d->tabs->setCurrent(0);
            return true;
        }
        else if (key.qtKey() == Qt::Key_End)
        {
            d->tabs->setCurrent(d->tabs->items().size() - 1);
            return true;
        }
    }
    return false;
}

void HomeWidget::moveOnscreen(TimeDelta span)
{
    d->moveOnscreen(span);
}

void HomeWidget::moveOffscreen(TimeDelta span)
{
    d->moveOffscreen(span);
}

void HomeWidget::update()
{
    GuiWidget::update();
    d->checkDismissHiding();
    if (d->blanker)
    {
        d->blanker->disposeIfDone();
    }
}

PopupWidget *HomeWidget::makeSettingsPopup()
{
    PopupMenuWidget *menu = new PopupMenuWidget;
    menu->items()
            << new ui::Item(ui::Item::Separator, tr("Library"))
            << new ui::VariableToggleItem(tr("Show Descriptions"), App::config("home.showColumnDescription"))
            << new ui::VariableToggleItem(tr("Show Unplayable"),   App::config("home.showUnplayableGames"))
            << new ui::Item(ui::Item::Separator)
            << new ui::Item(ui::Item::Separator, tr("Columns"))
            << new ui::VariableToggleItem(tr("Doom"),            App::config("home.columns.doom"))
            << new ui::VariableToggleItem(tr("Heretic"),         App::config("home.columns.heretic"))
            << new ui::VariableToggleItem(tr("Hexen"),           App::config("home.columns.hexen"))
            << new ui::VariableToggleItem(tr("Other Games"),     App::config("home.columns.otherGames"))
            << new ui::VariableToggleItem(tr("Multiplayer"),     App::config("home.columns.multiplayer"));
    return menu;
}

bool HomeWidget::dispatchEvent(Event const &event, bool (Widget::*memberFunc)(const Event &))
{
    if (event.type() == Event::MouseWheel)
    {
        if (event.isMouse())
        {
#if 0
            MouseEvent const &mouse = event.as<MouseEvent>();
            if (event.type() == Event::MouseWheel &&
               mouse.wheelMotion() == MouseEvent::Step)
            {
                if (abs(mouse.wheel().x) > abs(mouse.wheel().y))
                {
                    d->switchTab(-de::sign(mouse.wheel().x), true /*instant*/);
                }
            }
#endif
        }
    }
    return GuiWidget::dispatchEvent(event, memberFunc);
}

void HomeWidget::tabChanged()
{
    d->scrollToTab(d->tabs->currentItem().data().toInt(), SCROLL_SPAN);
    d->updateHighlightedTab();
}

void HomeWidget::mouseActivityInColumn(QObject const *columnWidget)
{
    for (int i = 0; i < d->columns.size(); ++i)
    {
        if (d->columns.at(i) == columnWidget)
        {
            d->tabs->setCurrent(i);
        }
    }
}

void HomeWidget::operator >> (PersistentState &toState) const
{
    Record &rec = toState.objectNamespace();
    rec.set(name().concatenateMember("firstTab"),  d->currentOffsetTab);
    rec.set(name().concatenateMember("activeTab"), d->highlightedTab());
}

void HomeWidget::operator << (PersistentState const &fromState)
{
    Record const &rec = fromState.objectNamespace();
    d->restoredOffsetTab = rec.geti(name().concatenateMember("firstTab"), -1);
    d->restoredActiveTab = rec.geti(name().concatenateMember("activeTab"), -1);
}
