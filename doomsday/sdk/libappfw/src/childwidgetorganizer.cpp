/** @file childwidgetorganizer.cpp Organizes widgets according to a UI context.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/ChildWidgetOrganizer"
#include "de/LabelWidget"
#include "de/ui/Item"

#include <de/App>
#include <QMap>

namespace de {

namespace internal
{
    enum AddBehavior
    {
        AlwaysAppend    = 0x1,
        AlwaysPrepend   = 0x2,
        IgnoreFilter    = 0x4,
        DefaultBehavior = 0,
    };

    Q_DECLARE_FLAGS(AddBehaviors, AddBehavior)
    Q_DECLARE_OPERATORS_FOR_FLAGS(AddBehaviors)
}

using namespace internal;

static DefaultWidgetFactory defaultWidgetFactory;

struct ChildWidgetOrganizer::Instance
    : public de::Private<ChildWidgetOrganizer>
    , DENG2_OBSERVES(Widget,   Deletion   )
    , DENG2_OBSERVES(ui::Data, Addition   )
    , DENG2_OBSERVES(ui::Data, Removal    )
    , DENG2_OBSERVES(ui::Data, OrderChange)
    , DENG2_OBSERVES(ui::Item, Change     )
{
    ui::Data const *dataItems = nullptr;
    IFilter const *filter = nullptr;
    GuiWidget *container;
    IWidgetFactory *factory;
    ui::DataPos firstAcceptedPos = 0;

    typedef QMap<ui::Item const *, GuiWidget *> Mapping;
    typedef QMutableMapIterator<ui::Item const *, GuiWidget *> MutableMappingIterator;
    Mapping mapping; ///< Maps items to corresponding widgets.

    bool virtualEnabled = false;
    Rule const *virtualMin = nullptr;
    Rule const *virtualMax = nullptr;
    ConstantRule *virtualStrut = nullptr;
    ConstantRule *estimatedHeight = nullptr;
    int averageItemHeight = 0;
    dsize virtualItemCount = 0;
    Ranges virtualPvsRange;
    QSet<GuiWidget *> pendingStrutAdjust;

    Instance(Public *i, GuiWidget *c)
        : Base(i)
        , container(c)
        , factory(&defaultWidgetFactory)
    {}

    ~Instance()
    {
        releaseRef(virtualMin);
        releaseRef(virtualMax);
        releaseRef(virtualStrut);
        releaseRef(estimatedHeight);
    }

    void set(ui::Data const *ctx)
    {
        if (dataItems)
        {
            dataItems->audienceForAddition() -= this;
            dataItems->audienceForRemoval() -= this;
            dataItems->audienceForOrderChange() -= this;

            clearWidgets();
            dataItems = 0;
        }

        dataItems = ctx;

        if (dataItems)
        {
            makeWidgets();

            dataItems->audienceForAddition() += this;
            dataItems->audienceForRemoval() += this;
            dataItems->audienceForOrderChange() += this;
        }
    }

    Ranges itemRange() const
    {
        Ranges range(0, dataItems->size());
        if (virtualEnabled) range = range.intersection(virtualPvsRange);
        return range;
    }

    GuiWidget *addItemWidget(ui::Data::Pos pos, AddBehaviors behavior = DefaultBehavior)
    {
        DENG2_ASSERT_IN_MAIN_THREAD(); // widgets should only be manipulated in UI thread
        DENG2_ASSERT(factory != 0);

        if (!itemRange().contains(pos))
        {
            // Outside the current potentially visible range.
            return nullptr;
        }

        ui::Item const &item = dataItems->at(pos);

        if (filter && !behavior.testFlag(IgnoreFilter))
        {
            if (!filter->isItemAccepted(self, *dataItems, item))
            {
                // Skip this one.
                return nullptr;
            }
        }

        GuiWidget *w = factory->makeItemWidget(item, container);
        if (!w) return nullptr; // Unpresentable.

        // Update the widget immediately.
        mapping.insert(&item, w);
        itemChanged(item);

        if (behavior.testFlag(AlwaysAppend) || pos == dataItems->size() - 1)
        {
            container->addLast(w);
        }
        else if (behavior.testFlag(AlwaysPrepend) || pos == 0)
        {
            container->addFirst(w);
        }
        else
        {
            if (GuiWidget *nextWidget = findNextWidget(pos))
            {
                container->insertBefore(w, *nextWidget);
            }
            else
            {
                container->add(w);
            }
        }

        // Others may alter the widget in some way.
        DENG2_FOR_PUBLIC_AUDIENCE2(WidgetCreation, i)
        {
            i->widgetCreatedForItem(*w, item);
        }

        // Observe.
        w->audienceForDeletion() += this; // in case it's manually deleted
        item.audienceForChange() += this;

        return w;
    }

    void removeItemWidget(ui::DataPos pos)
    {
        ui::Item const *item = &dataItems->at(pos);
        auto found = mapping.find(item);
        if (found != mapping.end())
        {
            GuiWidget *w = found.value();
            mapping.erase(found);
            deleteWidget(w);
            item->audienceForChange() -= this;
        }
    }

    GuiWidget *findNextWidget(ui::DataPos afterPos) const
    {
        // Some items may not be represented as widgets, so continue looking
        // until the next widget is found.
        while (++afterPos < dataItems->size())
        {
            auto found = mapping.constFind(&dataItems->at(afterPos));
            if (found != mapping.constEnd())
            {
                return found.value();
            }
        }
        return nullptr;
    }

    void makeWidgets()
    {
        DENG2_ASSERT(dataItems != 0);
        DENG2_ASSERT(container != 0);

        for (ui::Data::Pos i = 0; i < dataItems->size(); ++i)
        {
            addItemWidget(i, AlwaysAppend);
        }
    }

    void deleteWidget(GuiWidget *w)
    {
        pendingStrutAdjust.remove(w);

        w->audienceForDeletion() -= this;
        GuiWidget::destroy(w);
    }

    void clearWidgets()
    {
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            i.key()->audienceForChange() -= this;
            deleteWidget(i.value());
        }
        mapping.clear();
    }

    void widgetBeingDeleted(Widget &widget)
    {
        MutableMappingIterator iter(mapping);
        while (iter.hasNext())
        {
            iter.next();
            if (iter.value() == &widget)
            {
                iter.remove();
                break;
            }
        }
    }

    void dataItemAdded(ui::Data::Pos pos, ui::Item const &item)
    {
        addItemWidget(pos);

        if (!filter)
        {
            virtualItemCount = dataItems->size();
            updateVirtualHeight();
        }
        else
        {
            if (filter->isItemAccepted(self, *dataItems, item))
            {
                ++virtualItemCount;
                updateVirtualHeight();
            }
        }
    }

    void dataItemRemoved(ui::Data::Pos pos, ui::Item &item)
    {
        if (pos < firstAcceptedPos)
        {
            --firstAcceptedPos;
        }
        if (!filter || filter->isItemAccepted(self, *dataItems, item))
        {
            --virtualItemCount;
            updateVirtualHeight();
        }

        Mapping::iterator found = mapping.find(&item);
        if (found != mapping.end())
        {
            found.key()->audienceForChange() -= this;
            deleteWidget(found.value());
            mapping.erase(found);
        }
    }

    void dataItemOrderChanged()
    {
        // Remove all widgets and put them back in the correct order.
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            container->remove(*i.value());
        }
        for (ui::Data::Pos i = 0; i < dataItems->size(); ++i)
        {
            if (mapping.contains(&dataItems->at(i)))
            {
                container->add(mapping[&dataItems->at(i)]);
            }
        }
    }

    void itemChanged(ui::Item const &item)
    {
        if (!mapping.contains(&item))
        {
            // Not represented as a child widget.
            return;
        }

        GuiWidget &w = *mapping[&item];
        factory->updateItemWidget(w, item);

        // Notify.
        DENG2_FOR_PUBLIC_AUDIENCE2(WidgetUpdate, i)
        {
            i->widgetUpdatedForItem(w, item);
        }
    }

    GuiWidget *find(ui::Item const &item) const
    {
        Mapping::const_iterator found = mapping.constFind(&item);
        if (found == mapping.constEnd()) return 0;
        return found.value();
    }

    GuiWidget *findByLabel(String const &label) const
    {
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            if (i.key()->label() == label)
            {
                return i.value();
            }
        }
        return 0;
    }

    ui::Item const *findByWidget(GuiWidget const &widget) const
    {
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            if (i.value() == &widget)
            {
                return i.key();
            }
        }
        return 0;
    }

    void refilter()
    {
        if (!filter)
        {
            firstAcceptedPos = 0;
            return;
        }

        firstAcceptedPos = ui::Data::InvalidPos;

        if (virtualEnabled)
        {
            virtualPvsRange = Ranges();
            virtualItemCount = 0;
            virtualStrut->set(0);
            clearWidgets();
        }

        for (ui::DataPos i = 0; i < dataItems->size(); ++i)
        {
            ui::Item const *item = &dataItems->at(i);
            bool const accepted = filter->isItemAccepted(self, *dataItems, *item);

            if (!virtualEnabled)
            {
                if (!accepted && mapping.contains(item))
                {
                    // This widget needs to be removed.
                    removeItemWidget(i);
                }
                else if (accepted && !mapping.contains(item))
                {
                    // This widget may need to be created.
                    addItemWidget(i, IgnoreFilter);
                }
            }

            if (accepted)
            {
                ++virtualItemCount;

                if (firstAcceptedPos == ui::Data::InvalidPos)
                {
                    firstAcceptedPos = i;
                }
            }
        }

        updateVirtualHeight();
    }

//- Child Widget Virtualization ---------------------------------------------------------

    void updateVirtualHeight()
    {
        if (virtualEnabled)
        {
            estimatedHeight->set(virtualItemCount * float(averageItemHeight));
        }
    }

    GuiWidget *firstChild() const
    {
        return &container->childWidgets().first()->as<GuiWidget>();
    }

    GuiWidget *lastChild() const
    {
        return &container->childWidgets().last()->as<GuiWidget>();
    }

    float virtualItemHeight(GuiWidget const *widget) const
    {
        if (float hgt = widget->rule().height().value())
        {
            return hgt;
        }
        return averageItemHeight;
    }

    duint maxVisibleItems() const
    {
        if (!virtualMin) return 0;
        return duint(std::ceil((virtualMax->value() - virtualMin->value()) /
                                float(averageItemHeight)));
    }

    void updateVirtualization()
    {
        if (!virtualEnabled || !virtualMin || !virtualMax ||
            virtualMin->valuei() >= virtualMax->valuei())
        {
            return;
        }

        // Apply the pending strut reductions, if the widget heights are now known.
        // TODO: Changes in item heights should be observed and immediately applied...
        {
            QMutableSetIterator<GuiWidget *> iter(pendingStrutAdjust);
            while (iter.hasNext())
            {
                iter.next();
                if (iter.value()->rule().height().value() > 0)
                {
                    // Adjust based on the difference to the average height.
                    virtualStrut->set(de::max(0.f, virtualStrut->value() -
                                              (iter.value()->rule().height().value() -
                                               averageItemHeight)));
                    iter.remove();
                }
            }
        }

        Rangef estimatedExtents; // Range covered by items (estimated).
        if (container->childCount() > 0)
        {
            estimatedExtents = Rangef(firstChild()->rule().top()   .value(),
                                       lastChild()->rule().bottom().value());
        }
        else
        {
            estimatedExtents = Rangef(virtualMin->value(), virtualMin->value());
        }

        duint const maxVisible = maxVisibleItems();
        bool changed = true;

        while (changed)
        {
            changed = false;

            // Can we remove widgets?
            while (container->childCount() > 1 &&
                   lastChild()->rule().top().value() > virtualMax->value())
            {
                // Remove from the bottom.
                ui::DataPos const pos = virtualPvsRange.end - 1;
                if (reduceVirtualPvs(ui::Down))
                {
                    estimatedExtents.end -= virtualItemHeight(lastChild());
                    removeItemWidget(pos);
                    changed = true;
                }
                else break;
            }

            while (container->childCount() > 1 &&
                   firstChild()->rule().bottom().value() < virtualMin->value())
            {
                // Remove from the top.
                ui::DataPos const pos = virtualPvsRange.start;
                if (reduceVirtualPvs(ui::Up))
                {
                    float const itemHeight = virtualItemHeight(firstChild());
                    estimatedExtents.start += itemHeight;
                    virtualStrut->set(de::max(0.f, virtualStrut->value() + itemHeight));
                    removeItemWidget(pos);
                    changed = true;
                }
                else break;
            }

            // Can we add more widgets in the bottom?
            while (container->childCount() < virtualItemCount &&
                   estimatedExtents.end < virtualMax->value() &&
                   container->childCount() < maxVisible)
            {
                ui::DataPos pos = extendVirtualPvs(ui::Down);
                if (pos != ui::Data::InvalidPos)
                {
                    estimatedExtents.end += averageItemHeight;
                    addItemWidget(pos, AlwaysAppend | IgnoreFilter);
                    changed = true;
                }
                else break;
            }

            // Add widgets to the top?
            while (container->childCount() < virtualItemCount &&
                   estimatedExtents.start > virtualMin->value() &&
                   container->childCount() < maxVisible)
            {
                ui::DataPos pos = extendVirtualPvs(ui::Up);
                if (pos != ui::Data::InvalidPos)
                {
                    GuiWidget *w = addItemWidget(pos, AlwaysPrepend | IgnoreFilter);
                    pendingStrutAdjust.insert(w);
                    estimatedExtents.start -= averageItemHeight;
                    virtualStrut->set(de::max(0.f, virtualStrut->value() - averageItemHeight));
                    changed = true;
                }
                else break;
            }
        }

        if (virtualPvsRange.start == firstAcceptedPos)
        {
            virtualStrut->set(0);
            pendingStrutAdjust.clear();
        }
    }

    /**
     * Extends the potentially visible range by one item.
     * @param dir  Direction for expansion.
     * @return Position of the newly added item.
     */
    ui::DataPos extendVirtualPvs(ui::Direction dir)
    {
        ui::DataPos pos;

        if (dir == ui::Down)
        {
            pos = virtualPvsRange.end;
            do
            {
                if (pos == dataItems->size())
                {
                    return ui::Data::InvalidPos; // Out of items.
                }
                ++pos;
            }
            while (!filter || !filter->isItemAccepted(self, *dataItems, dataItems->at(pos - 1)));

            virtualPvsRange.end = pos;
            //qDebug() << "PVS extended (down):" << virtualPvsRange.asText();
            return pos - 1;
        }
        else
        {
            pos = virtualPvsRange.start;
            do
            {
                if (pos == 0)
                {
                    return ui::Data::InvalidPos; // Out of items.
                }
                --pos;
            }
            while (!filter || !filter->isItemAccepted(self, *dataItems, dataItems->at(pos)));

            virtualPvsRange.start = pos;
            //qDebug() << "PVS extended (up):" << virtualPvsRange.asText();
            return virtualPvsRange.start;
        }
    }

    bool reduceVirtualPvs(ui::Direction dir)
    {
        if (virtualPvsRange.isEmpty()) return false;

        if (dir == ui::Down)
        {
            do
            {
                --virtualPvsRange.end;
            }
            while (!virtualPvsRange.isEmpty() &&
                   (!filter ||
                    !filter->isItemAccepted(self, *dataItems,
                                            dataItems->at(virtualPvsRange.end - 1))));
            //qDebug() << "PVS reduced (down):" << virtualPvsRange.asText();
        }
        else if (dir == ui::Up)
        {
            do
            {
                ++virtualPvsRange.start;
            }
            while (!virtualPvsRange.isEmpty() &&
                   (!filter ||
                    !filter->isItemAccepted(self, *dataItems,
                                            dataItems->at(virtualPvsRange.start))));
            //qDebug() << "PVS reduced (up):" << virtualPvsRange.asText();
        }
        return true;
    }

    DENG2_PIMPL_AUDIENCE(WidgetCreation)
    DENG2_PIMPL_AUDIENCE(WidgetUpdate)
};

DENG2_AUDIENCE_METHOD(ChildWidgetOrganizer, WidgetCreation)
DENG2_AUDIENCE_METHOD(ChildWidgetOrganizer, WidgetUpdate)

ChildWidgetOrganizer::ChildWidgetOrganizer(GuiWidget &container)
    : d(new Instance(this, &container))
{}

void ChildWidgetOrganizer::setContext(ui::Data const &context)
{
    d->set(&context);
}

void ChildWidgetOrganizer::unsetContext()
{
    d->set(0);
}

ui::Data const &ChildWidgetOrganizer::context() const
{
    DENG2_ASSERT(d->dataItems != 0);
    return *d->dataItems;
}

GuiWidget *ChildWidgetOrganizer::itemWidget(ui::Data::Pos pos) const
{
    return itemWidget(context().at(pos));
}

void ChildWidgetOrganizer::setWidgetFactory(IWidgetFactory &factory)
{
    d->factory = &factory;
}

ChildWidgetOrganizer::IWidgetFactory &ChildWidgetOrganizer::widgetFactory() const
{
    DENG2_ASSERT(d->factory != 0);
    return *d->factory;
}

void ChildWidgetOrganizer::setFilter(IFilter const &filter)
{
    d->filter = &filter;
}

void ChildWidgetOrganizer::unsetFilter()
{
    d->filter = nullptr;
}

GuiWidget *ChildWidgetOrganizer::itemWidget(ui::Item const &item) const
{
    return d->find(item);
}

GuiWidget *ChildWidgetOrganizer::itemWidget(String const &label) const
{
    return d->findByLabel(label);
}

ui::Item const *ChildWidgetOrganizer::findItemForWidget(GuiWidget const &widget) const
{
    return d->findByWidget(widget);
}

void ChildWidgetOrganizer::refilter()
{
    d->refilter();
}

dsize ChildWidgetOrganizer::itemCount() const
{
    return d->virtualItemCount;
}

void ChildWidgetOrganizer::setVirtualizationEnabled(bool enabled)
{
    d->virtualEnabled = enabled;
    d->virtualPvsRange = Ranges(0, 0);

    if (d->virtualEnabled)
    {
        d->estimatedHeight = new ConstantRule(0);
        d->virtualStrut    = new ConstantRule(0);
    }
    else
    {
        releaseRef(d->estimatedHeight);
        releaseRef(d->virtualStrut);
    }
}

void ChildWidgetOrganizer::setVisibleArea(Rule const &minimum, Rule const &maximum)
{
    changeRef(d->virtualMin, minimum);
    changeRef(d->virtualMax, maximum);
}

bool ChildWidgetOrganizer::virtualizationEnabled() const
{
    return d->virtualEnabled;
}

Rule const &ChildWidgetOrganizer::virtualStrut() const
{
    DENG2_ASSERT(d->virtualEnabled);
    return *d->virtualStrut;
}

void ChildWidgetOrganizer::setAverageChildHeight(int height)
{
    d->averageItemHeight = height;
    d->updateVirtualHeight();
}

Rule const &ChildWidgetOrganizer::estimatedTotalHeight() const
{
    return *d->estimatedHeight;
}

void ChildWidgetOrganizer::updateVirtualization()
{
    d->updateVirtualization();
}

GuiWidget *DefaultWidgetFactory::makeItemWidget(ui::Item const &, GuiWidget const *)
{
    return new LabelWidget;
}

void DefaultWidgetFactory::updateItemWidget(GuiWidget &widget, ui::Item const &item)
{
    widget.as<LabelWidget>().setText(item.label());
}

} // namespace de
