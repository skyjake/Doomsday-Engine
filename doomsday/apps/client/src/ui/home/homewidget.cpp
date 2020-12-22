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

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>

#include <de/app.h>
#include <de/fadetoblackwidget.h>
#include <de/labelwidget.h>
#include <de/loop.h>
#include <de/packageloader.h>
#include <de/persistentstate.h>
#include <de/popupmenuwidget.h>
#include <de/sequentiallayout.h>
#include <de/styleproceduralimage.h>
#include <de/tabwidget.h>
#include <de/timer.h>

using namespace de;

static constexpr TimeSpan SCROLL_SPAN  = 0.5_s;
static constexpr TimeSpan DISMISS_SPAN = 1.5_s;

DE_GUI_PIMPL(HomeWidget)
, DE_OBSERVES(App,                  StartupComplete)
, DE_OBSERVES(Games,                Readiness)
, DE_OBSERVES(DoomsdayApp,          GameLoad)
, DE_OBSERVES(DoomsdayApp,          GameChange)
, DE_OBSERVES(Variable,             Change)
, DE_OBSERVES(ButtonWidget,         StateChange)
, DE_OBSERVES(ColumnWidget,         Activity)
, DE_OBSERVES(PackagesColumnWidget, AvailableCount)
{
    struct Column {
        ColumnWidget *widget;
        SafePtr<Variable> configVar;
        Column(ColumnWidget *w, Variable *var) : widget(w), configVar(var) {}
    };

    Dispatch dispatch;
    SaveListData savedItems; ///< All the available save games as items.

    dsize                visibleColumnCount = 2;
    List<Column>         allColumns; // not owned
    List<ColumnWidget *> columns;    // Only the visible ones (not owned).
    IndirectRule *       columnWidth;
    LabelWidget *        tabsBackground;
    TabWidget *          tabs;
    int                  currentOffsetTab = 0;
    AnimationRule *      scrollOffset;
    AnimationRule *      dismissOffset;
    ButtonWidget *       moveLeft;
    ButtonWidget *       moveRight;
    Timer                moveShowTimer;
    ButtonWidget *       taskBarHintButton;
    bool                 dismissing   = false;
    bool                 havePackages = false;

    SafeWidgetPtr<FadeToBlackWidget> blanker;

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
        if (style().isBlurringAllowed())
        {
            tabsBackground->set(Background(Vec4f(1), Background::Blurred));
        }
        else
        {
            tabsBackground->set(Background(Vec4f(0, 0, 0, 1)));
        }

        // Create the column navigation buttons.
        moveLeft  = new ButtonWidget;
        moveRight = new ButtonWidget;
        //moveLeft ->setImage(new StyleProceduralImage("fold", *moveLeft, 90));
        //moveRight->setImage(new StyleProceduralImage("fold", *moveLeft, -90));
        moveLeft ->setActionFn([this]() { tabs->setCurrent(visibleTabRange().start - 1); });
        moveRight->setActionFn([this]() { tabs->setCurrent(visibleTabRange().end); });
        configureEdgeNavigationButton(*moveLeft);
        configureEdgeNavigationButton(*moveRight);

        // Hint/shortcut button for opening the task bar.
        taskBarHintButton = new ButtonWidget;
        taskBarHintButton->setBackgroundColor("");
        taskBarHintButton->setSizePolicy(ui::Expand, ui::Expand);
        taskBarHintButton->margins().set("dialog.gap");
        taskBarHintButton->setText(_E(b) "ESC" _E(.) " Task Bar");
//        taskBarHintButton->setTextColor("text");
        taskBarHintButton->setFont("small");
        taskBarHintButton->setOpacity(.66f);
        taskBarHintButton->rule()
                .setInput(Rule::Right,  self().rule().right()  - rule("dialog.gap"))
                .setInput(Rule::Bottom, self().rule().bottom() - rule("dialog.gap") + *dismissOffset);
        taskBarHintButton->setActionFn([] () {
            ClientWindow::main().taskBar().open();
        });

        // The task bar is created later.
        dispatch += [this]() {
            ClientWindow::main().taskBar().audienceForOpen() += [this]() {
                taskBarHintButton->disable();
                taskBarHintButton->setOpacity(0, 0.25);
            };
            ClientWindow::main().taskBar().audienceForClose() += [this]() {
                taskBarHintButton->enable();
                taskBarHintButton->setOpacity(.66f, 0.5);
            };
        };

        // The navigation buttons should be hidden with a delay or otherwise the user
        // may inadvertely click on them right after they're gone.
        moveShowTimer.setSingleShot(true);
        moveShowTimer.setInterval(SCROLL_SPAN);
        moveShowTimer += [this]() {
            moveLeft ->show(tabs->current() != 0);
            moveRight->show(tabs->current() < tabs->items().size() - 1);
        };
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
        //button.setImageColor(style().colors().colorf("inverted.text") + Vec4f(0, 0, 0, 2));
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

    void updateVisibleColumnsAndTabHeadings()
    {
        const bool gotGames = DoomsdayApp::games().numPlayable() > 0;

        // Show columns depending on whether there are playable games.
        allColumns.at(0).widget->show(!gotGames);
        for (int i = 1; i < allColumns.sizei(); ++i)
        {
            const Column &col = allColumns.at(i);
            if (col.configVar)
            {
                col.widget->show(gotGames && col.configVar->value().isTrue());
            }
            if (is<PackagesColumnWidget>(col.widget))
            {
                col.widget->show(gotGames || havePackages);
            }
        }

        // Tab headings for visible columns.
        struct TabSpec {
            const Column *col;
            int           visibleIndex;
        };
        List<TabSpec> specs;
        int visibleIndex = 0;
        for (int index = 0; index < allColumns.sizei(); ++index)
        {
            const Column &col = allColumns.at(index);
            if (!col.widget->behavior().testFlag(Widget::Hidden))
            {
                TabSpec ts;
                ts.col = &col;
                ts.visibleIndex = visibleIndex++;
                specs << ts;
            }
        }
        // Is this different than what is currently there?
        if (tabs->items().size() == dsize(specs.size()))
        {
            bool differenceFound = false;
            for (dsize pos = 0; pos < tabs->items().size(); ++pos)
            {
                const auto &item = tabs->items().at(pos);
                if (item.label() != specs.at(pos).col->widget->tabHeading() ||
                    item.data().asInt() != specs.at(pos).visibleIndex)
                {
                    differenceFound = true;
                    break;
                }
            }
            if (!differenceFound) return;
        }
        // Create new items.
        tabs->clearItems();
        for (const TabSpec &ts : specs)
        {
            auto *tabItem = new TabItem(ts.col->widget->tabHeading(), NumberValue(ts.visibleIndex));
            tabItem->setShortcutKey(ts.col->widget->tabShortcut());
            tabs->items() << tabItem;
        }
    }

    void updateVisibleTabsAndLayout()
    {
        updateVisibleColumnsAndTabHeadings();
        calculateColumnCount();
        updateLayout();
    }

    void appStartupCompleted()
    {
        blanker->start(0.25);
    }

    void gameReadinessUpdated()
    {
        self().root().window().glActivate();

        updateVisibleTabsAndLayout();

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

    void aboutToLoadGame(const Game &gameBeingLoaded)
    {
        self().root().clearFocusStack();
        self().root().setFocus(nullptr);

        if (gameBeingLoaded.isNull())
        {
            moveOnscreen();
        }
        else
        {
            TimeSpan span = DISMISS_SPAN;
            auto &win = self().root().window().as<ClientWindow>();
            if (win.isGameMinimized())
            {
                win.busy().clearTransitionFrameToBlack();
                span = 1.0;
            }
            moveOffscreen();
        }
    }

    void currentGameChanged(const Game &newGame)
    {
        if (!newGame.isNull())
        {
            ClientWindow::main().fadeContent(ClientWindow::FadeFromBlack, 1.0);
            self().root().clearFocusStack();
            self().root().setFocus(nullptr);
        }
    }

    void variableValueChanged(Variable &, const Value &)
    {
        updateVisibleTabsAndLayout();
    }

    void moveOffscreen(TimeSpan span = DISMISS_SPAN)
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

    void moveOnscreen(TimeSpan span = DISMISS_SPAN)
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
        col->audienceForActivity() += this;

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
        for (GuiWidget *widget : self().childWidgets())
        {
            if (!widget->behavior().testFlag(Widget::Hidden))
            {
                if (ColumnWidget *column = maybeAs<ColumnWidget>(widget))
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
        if (visibleTabRange().end >= columns.sizei())
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
            const Rangei vis = visibleTabRange();
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
    void scrollToTab(int pos, TimeSpan span)
    {
        pos = de::clamp(0, pos, columns.sizei() - 1);

        if (currentOffsetTab + int(visibleColumnCount) > columns.sizei())
        {
            // Don't let the visible range extend outside the view.
            currentOffsetTab = columns.size() - visibleColumnCount;
            span = 0.0;
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

    void setScrollOffset(int tab, TimeSpan span)
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
        const int newHighlightPos = tabs->currentItem().data().asInt();
        for (int pos = 0; pos < columns.sizei(); ++pos)
        {
            if (pos != newHighlightPos && columns[pos]->isHighlighted())
            {
                columns[pos]->setHighlighted(false);
            }
        }
        // Set new highlight.
        columns[newHighlightPos]->setHighlighted(true);

        moveShowTimer.start();
    }

    int highlightedTab() const
    {
        for (int pos = 0; pos < columns.sizei(); ++pos)
        {
            if (columns[pos]->isHighlighted())
            {
                return pos;
            }
        }
        return -1;
    }

    void mouseActivity(const ColumnWidget *columnWidget) override
    {
        for (dsize i = 0; i < columns.size(); ++i)
        {
            if (columns.at(i) == columnWidget)
            {
                tabs->setCurrent(i);
                break;
            }
        }
    }

    void availablePackageCountChanged(int count) override
    {
        havePackages = count > 0;
        updateVisibleTabsAndLayout();
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

    {
        auto *packagesColumn = new PackagesColumnWidget;
        d->addColumn(packagesColumn);
        packagesColumn->audienceForAvailableCount() += d;
    }

    d->updateVisibleColumnsAndTabHeadings();
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
    const Rule &gap = rule("gap");
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
    d->tabs->audienceForTab() += [this]() { tabChanged(); };
}

void HomeWidget::viewResized()
{
    d->calculateColumnCount();
    d->updateLayout();
}

bool HomeWidget::handleEvent(const Event &event)
{
    if (event.isKeyDown())
    {
        const auto &key = event.as<KeyEvent>();

        // Keyboard navigation between tabs.
        if (key.ddKey() == DDKEY_LEFTARROW)
        {
            d->switchTab(-1);
            return true;
        }
        else if (key.ddKey() == DDKEY_RIGHTARROW)
        {
            d->switchTab(+1);
            return true;
        }
        else if (key.ddKey() == DDKEY_HOME)
        {
            d->tabs->setCurrent(0);
            return true;
        }
        else if (key.ddKey() == DDKEY_END)
        {
            d->tabs->setCurrent(d->tabs->items().size() - 1);
            return true;
        }
    }
    return false;
}

void HomeWidget::moveOnscreen(TimeSpan span)
{
    d->moveOnscreen(span);
}

void HomeWidget::moveOffscreen(TimeSpan span)
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
            << new ui::Item(ui::Item::Separator, "Library")
            << new ui::VariableToggleItem("Show Descriptions", App::config("home.showColumnDescription"))
            << new ui::VariableToggleItem("Show Unplayable",   App::config("home.showUnplayableGames"))
            << new ui::Item(ui::Item::Separator)
            << new ui::Item(ui::Item::Separator, "Columns")
            << new ui::VariableToggleItem("Doom",            App::config("home.columns.doom"))
            << new ui::VariableToggleItem("Heretic",         App::config("home.columns.heretic"))
            << new ui::VariableToggleItem("Hexen",           App::config("home.columns.hexen"))
            << new ui::VariableToggleItem("Other Games",     App::config("home.columns.otherGames"))
            << new ui::VariableToggleItem("Multiplayer",     App::config("home.columns.multiplayer"));
    return menu;
}

bool HomeWidget::dispatchEvent(const Event &event, bool (Widget::*memberFunc)(const Event &))
{
    if (event.type() == Event::MouseWheel)
    {
        if (event.isMouse())
        {
#if 0
            const MouseEvent &mouse = event.as<MouseEvent>();
            if (event.type() == Event::MouseWheel &&
               mouse.wheelMotion() == MouseEvent::Steps)
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
    d->scrollToTab(d->tabs->currentItem().data().asInt(), SCROLL_SPAN);
    d->updateHighlightedTab();
}

void HomeWidget::updateStyle()
{
    GuiWidget::updateStyle();
    viewResized();
}

void HomeWidget::operator >> (PersistentState &toState) const
{
    Record &rec = toState.objectNamespace();
    rec.set(name().concatenateMember("firstTab"),  d->currentOffsetTab);
    rec.set(name().concatenateMember("activeTab"), d->highlightedTab());
}

void HomeWidget::operator << (const PersistentState &fromState)
{
    const Record &rec = fromState.objectNamespace();
    d->restoredOffsetTab = rec.geti(name().concatenateMember("firstTab"), -1);
    d->restoredActiveTab = rec.geti(name().concatenateMember("activeTab"), -1);
}
