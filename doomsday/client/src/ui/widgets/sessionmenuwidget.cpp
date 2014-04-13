/** @file sessionmenuwidget.cpp
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

#include "ui/widgets/sessionmenuwidget.h"

#include <de/ButtonWidget>

using namespace de;

DENG_GUI_PIMPL(SessionMenuWidget)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DENG2_OBSERVES(ButtonWidget,         Press)
{
    GameFilterWidget *filter;

    Instance(Public *i)
        : Base(i)
        , filter(0)
    {}

    static bool sorter(ui::Item const &a, ui::Item const &b)
    {
        SessionItem const *x = a.maybeAs<SessionItem>();
        SessionItem const *y = b.maybeAs<SessionItem>();
        if(x && y)
        {
            int cmp = x->sortKey().compareWithoutCase(y->sortKey());
            if(cmp < 0) return true;
            if(cmp > 0) return false;

            // Secondarily by title.
            if(x->menu().filter().sortOrder() != GameFilterWidget::SortByTitle)
            {
                return x->title().compareWithoutCase(y->title()) < 0;
            }
        }
        return false;
    }

    void sortSessions()
    {
        self.items().stableSort(sorter);
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &)
    {
        // Observe the button for presses.
        widget.as<GameSessionWidget>().loadButton().audienceForPress() += this;
    }

    void buttonPressed(ButtonWidget &button)
    {
        ui::Item const *item = self.organizer().findItemForWidget(button.parentWidget()->as<GuiWidget>());
        DENG2_ASSERT(item != 0);

        emit self.sessionSelected(item);
    }
};

SessionMenuWidget::SessionMenuWidget(String const &name)
    : MenuWidget(name), d(new Instance(this))
{
    enableScrolling(false);
    setGridSize(1, ui::Filled, 0, ui::Expand);

    organizer().setWidgetFactory(*this);
    organizer().audienceForWidgetCreation() += d;
}

void SessionMenuWidget::setFilter(GameFilterWidget *filter)
{
    if(d->filter)
    {
        disconnect(d->filter, SIGNAL(sortOrderChanged()), this, SLOT(sort()));
    }

    d->filter = filter;

    if(filter)
    {
        connect(filter, SIGNAL(sortOrderChanged()), this, SLOT(sort()));
    }
}

GameFilterWidget &SessionMenuWidget::filter() const
{
    DENG2_ASSERT(d->filter != 0);
    return *d->filter;
}

void SessionMenuWidget::setColumns(int numberOfColumns)
{
    if(layout().maxGridSize().x != numberOfColumns)
    {
        setGridSize(numberOfColumns, ui::Filled, 0, ui::Expand);
    }
}

void SessionMenuWidget::sort()
{
    if(d->filter)
    {
        d->sortSessions();
    }
}

//--------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(SessionMenuWidget::SessionItem)
{
    SessionMenuWidget *owner;
};

SessionMenuWidget::SessionItem::SessionItem(SessionMenuWidget &owner)
    : d(new Instance)
{
    d->owner = &owner;
}

SessionMenuWidget::SessionItem::~SessionItem()
{}

SessionMenuWidget &SessionMenuWidget::SessionItem::menu() const
{
    return *d->owner;
}

String SessionMenuWidget::SessionItem::sortKey() const
{
    switch(menu().filter().sortOrder())
    {
    case GameFilterWidget::SortByTitle:
        return title();

    case GameFilterWidget::SortByIdentityKey:
        return gameIdentityKey();
    }
    return "";
}
