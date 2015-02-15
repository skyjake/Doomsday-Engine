/** @file gamefilterwidget.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/gamefilterwidget.h"

#include <de/ButtonWidget>
#include <de/ChoiceWidget>
#include <de/SequentialLayout>
#include <de/DialogContentStylist>
#include <de/TabWidget>
#include <de/PersistentState>

using namespace de;

static TimeDelta const BACKGROUND_FADE_SPAN = 0.25;
static float const BACKGROUND_FILL_OPACITY = 0.8f;

DENG_GUI_PIMPL(GameFilterWidget)
{
    LabelWidget *background = nullptr;
    Rule const *bgOpacityRule = nullptr;
    Animation bgOpacity { 0, Animation::Linear };
    bool animatingOpacity = false;

    TabWidget *tabs;
    LabelWidget *sortLabel;
    ChoiceWidget *sortBy;
    DialogContentStylist stylist;
    FilterMode filterMode = UserChangeable;

    Instance(Public *i) : Base(i)
    {
        stylist.setContainer(self);

        // Optional background.
        self.add(background = new LabelWidget);
        background->set(Background(Vector4f(style().colors().colorf("text"), 0),
                                   Background::BlurredWithSolidFill));
        background->setOpacity(0);
        background->setAttribute(IndependentOpacity);
        background->hide(); // hidden by default

        // Create widgets.
        self.add(tabs = new TabWidget);
        sortLabel = LabelWidget::newWithText(tr("Sort By:"), &self);
        self.add(sortBy = new ChoiceWidget);

        tabs->items()
                << new TabItem(tr("Singleplayer"), Singleplayer)
                << new TabItem(tr("Multiplayer"),  Multiplayer)
                << new TabItem(tr("All Games"),    AllGames);

        sortLabel->setFont("small");
        sortLabel->margins().setLeft("");
        sortBy->setFont("tab.label");
        sortBy->setOpeningDirection(ui::Down);
        sortBy->items()
                << new ChoiceItem(tr("Title"),        SortByTitle)
                << new ChoiceItem(tr("Identity key"), SortByIdentityKey);

        SequentialLayout layout(self.rule().left(), self.rule().top(), ui::Right);
        layout << *sortLabel << *sortBy;

        tabs->rule()
                .setInput(Rule::Width, self.rule().width())
                .setInput(Rule::Left,  self.rule().left())
                .setInput(Rule::Top,   self.rule().top());
    }

    ~Instance()
    {
        releaseRef(bgOpacityRule);
    }

    String persistId(String const &name) const
    {
        return self.name() + "." + name;
    }

    void updateBackgroundOpacity()
    {
        float opacity = (bgOpacityRule->value() > 1? 1 : 0);
        if(!fequal(bgOpacity.target(), opacity))
        {
            bgOpacity.setValue(opacity, BACKGROUND_FADE_SPAN);
            background->setOpacity(opacity, BACKGROUND_FADE_SPAN);
            animatingOpacity = true;
        }

        if(animatingOpacity)
        {
            Background bg = background->background();
            bg.solidFill.w = bgOpacity * BACKGROUND_FILL_OPACITY;
            background->set(bg);

            if(bgOpacity.done()) animatingOpacity = false;
        }
    }
};

GameFilterWidget::GameFilterWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    connect(d->tabs, SIGNAL(currentTabChanged()), this, SIGNAL(filterChanged()));
    connect(d->sortBy, SIGNAL(selectionChanged(uint)), this, SIGNAL(sortOrderChanged()));

    rule().setInput(Rule::Height, d->tabs->rule().height());
}

void GameFilterWidget::useInvertedStyle()
{
    d->tabs->useInvertedStyle();
    d->sortLabel->setTextColor("inverted.text");
    d->sortBy->useInfoStyle();
}

void GameFilterWidget::setFilter(Filter flt, FilterMode mode)
{
    ui::DataPos pos = d->tabs->items().findData(duint(flt));
    if(pos != ui::Data::InvalidPos)
    {
        d->tabs->setCurrent(pos);
    }
    d->filterMode = mode;

    if(d->filterMode == Permanent)
    {
        d->tabs->disable();
    }
}

void GameFilterWidget::enableBackground(Rule const &scrollPositionRule)
{
    DENG2_ASSERT(hasRoot());

    d->bgOpacityRule = holdRef(scrollPositionRule);

    d->background->rule()
            .setInput(Rule::Left,   root().viewLeft())
            .setInput(Rule::Right,  root().viewRight())
            .setInput(Rule::Top,    d->tabs->rule().top()    - style().rules().rule("gap"))
            .setInput(Rule::Bottom, d->tabs->rule().bottom() + style().rules().rule("gap"));

    d->background->show();
}

GameFilterWidget::Filter GameFilterWidget::filter() const
{
    return Filter(d->tabs->currentItem().data().toUInt());
}

GameFilterWidget::SortOrder GameFilterWidget::sortOrder() const
{
    return SortOrder(d->sortBy->selectedItem().data().toInt());
}

void GameFilterWidget::update()
{
    GuiWidget::update();

    if(d->background->isVisible())
    {
        d->updateBackgroundOpacity();
    }
}

void GameFilterWidget::operator >> (PersistentState &toState) const
{
    Record &st = toState.names();
    if(d->filterMode != Permanent)
    {
        st.set(d->persistId("filter"), dint(filter()));
    }
    st.set(d->persistId("order"),  dint(sortOrder()));
}

void GameFilterWidget::operator << (PersistentState const &fromState)
{
    Record const &st = fromState.names();
    if(d->filterMode != Permanent)
    {
        d->tabs->setCurrent(d->tabs->items().findData(int(st[d->persistId("filter")])));
    }
    d->sortBy->setSelected(d->sortBy->items().findData(int(st[d->persistId("order" )])));
}
