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
        DefaultBehavior = 0,
    };

    Q_DECLARE_FLAGS(AddBehaviors, AddBehavior)
    Q_DECLARE_OPERATORS_FOR_FLAGS(AddBehaviors)
}

using namespace internal;

static DefaultWidgetFactory defaultWidgetFactory;

DENG2_PIMPL(ChildWidgetOrganizer)
    , DENG2_OBSERVES(Widget,   Deletion   )
    , DENG2_OBSERVES(ui::Data, Addition   )
    , DENG2_OBSERVES(ui::Data, Removal    )
    , DENG2_OBSERVES(ui::Data, OrderChange)
    , DENG2_OBSERVES(ui::Item, Change     )
{
    typedef Rangei PvsRange;

    ui::Data const *dataItems = nullptr;
    GuiWidget *container;
    IWidgetFactory *factory;

    typedef QMap<ui::Item const *, GuiWidget *> Mapping;
    typedef QMutableMapIterator<ui::Item const *, GuiWidget *> MutableMappingIterator;
    Mapping mapping; ///< Maps items to corresponding widgets.

    bool virtualEnabled = false;
    Rule const *virtualTop = nullptr;
    Rule const *virtualMin = nullptr;
    Rule const *virtualMax = nullptr;
    ConstantRule *virtualStrut = nullptr;
    ConstantRule *estimatedHeight = nullptr;
    int averageItemHeight = 0;
    PvsRange virtualPvs;
    float lastTop = 0;
    float totalCorrection = 0;
    float correctionPerUnit = 0;

    bool recyclingEnabled = false;
    QList<GuiWidget *> recycledWidgets; // Not GL-deinitialized to facilitate fast reuse.

    Impl(Public *i, GuiWidget *c)
        : Base(i)
        , container(c)
        , factory(&defaultWidgetFactory)
    {}

    ~Impl()
    {
        foreach (GuiWidget *recycled, recycledWidgets)
        {
            GuiWidget::destroy(recycled);
        }

        releaseRef(virtualTop);
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

    PvsRange itemRange() const
    {
        PvsRange range(0, dataItems->size());
        if (virtualEnabled) range = range.intersection(virtualPvs);
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

        GuiWidget *w = nullptr;
        if (recyclingEnabled && !recycledWidgets.isEmpty())
        {
            w = recycledWidgets.takeFirst();
        }
        else
        {
            w = factory->makeItemWidget(item, container);
        }
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
        //pendingStrutAdjust.remove(w);
        w->audienceForDeletion() -= this;

        if (recyclingEnabled)
        {
            w->orphan();
            recycledWidgets << w;
        }
        else
        {
            GuiWidget::destroy(w);
        }
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

    void dataItemAdded(ui::DataPos pos, ui::Item const &)
    {
        if (!virtualEnabled)
        {
            addItemWidget(pos);
        }
        else
        {
            // Items added below the PVS can be handled purely virtually (i.e., ignored).
            // Items above the PVS will cause the PVS range to be re-estimated.
            if (pos < ui::DataPos(virtualPvs.end))
            {
                clearWidgets();
                virtualPvs = PvsRange();
            }
            updateVirtualHeight();
        }
    }

    void dataItemRemoved(ui::DataPos pos, ui::Item &item)
    {
        Mapping::iterator found = mapping.find(&item);
        if (found != mapping.end())
        {
            found.key()->audienceForChange() -= this;
            deleteWidget(found.value());
            mapping.erase(found);
        }

        if (virtualEnabled)
        {
            if (virtualPvs.contains(pos))
            {
                clearWidgets();
                virtualPvs = PvsRange();
            }
            // Virtual total height changes even if the item was not represented by a widget.
            updateVirtualHeight();
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

//- Child Widget Virtualization ---------------------------------------------------------

    void updateVirtualHeight()
    {
        if (virtualEnabled)
        {
            estimatedHeight->set(dataItems->size() * float(averageItemHeight));
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

    float bestEstimateOfWidgetHeight(GuiWidget &w) const
    {
        float height = w.rule().height().value();
        if (fequal(height, 0.f))
        {
            // Actual height is not yet known, so use the average.
            height = w.estimatedHeight();
        }
        if (fequal(height, 0.f))
        {
            height = averageItemHeight;
        }
        return height;
    }

    void updateVirtualization()
    {
        if (!virtualEnabled || !virtualMin || !virtualMax || !virtualTop ||
            virtualMin->valuei() >= virtualMax->valuei())
        {
            return;
        }

        PvsRange const fullRange { 0, int(dataItems->size()) };
        PvsRange const oldPvs = virtualPvs;

        // Calculate position delta to compared to last update.
        float delta = virtualTop->value() - lastTop;
        lastTop = virtualTop->value();

        // Estimate a new PVS range based on the average item height and the visible area.
        float const y1 = de::max(0.f, virtualMin->value() - virtualTop->value());
        float const y2 = de::max(0.f, virtualMax->value() - virtualTop->value());

        int const spareItems = 3;
        PvsRange estimated = PvsRange(y1 / averageItemHeight - spareItems,
                                      y2 / averageItemHeight + spareItems)
                             .intersection(fullRange);

        // If this range is completely different than the current range, recreate
        // all visible widgets.
        if (oldPvs.isEmpty() ||
            estimated.start >= oldPvs.end ||
            estimated.end <= oldPvs.start)
        {
            clearWidgets();

            // Set up a fully estimated strut.
            virtualPvs = estimated;
            virtualStrut->set(averageItemHeight * virtualPvs.start);
            lastTop = virtualTop->value();
            delta = 0;
            totalCorrection = 0;

            for (auto pos = virtualPvs.start; pos < virtualPvs.end; ++pos)
            {
                addItemWidget(pos, AlwaysAppend);
            }
            DENG2_ASSERT(virtualPvs.size() == int(container->childCount()));
        }
        else if (estimated.end > oldPvs.end) // Extend downward.
        {
            virtualPvs.end = estimated.end;
            for (auto pos = oldPvs.end; pos < virtualPvs.end; ++pos)
            {
                addItemWidget(pos, AlwaysAppend);
            }
            DENG2_ASSERT(virtualPvs.size() == int(container->childCount()));
        }
        else if (estimated.start < oldPvs.start) // Extend upward.
        {
            virtualPvs.start = estimated.start;
            for (auto pos = oldPvs.start - 1;
                 pos >= virtualPvs.start && pos < int(dataItems->size());
                 --pos)
            {
                GuiWidget *w = addItemWidget(pos, AlwaysPrepend);
                DENG2_ASSERT(w != nullptr);

                float height = bestEstimateOfWidgetHeight(*w);
                // Reduce strut length to make room for new items.
                virtualStrut->set(de::max(0.f, virtualStrut->value() - height));
            }
            DENG2_ASSERT(virtualPvs.size() == int(container->childCount()));
        }

        if (container->childCount() > 0)
        {
            // Remove excess widgets from the top and extend the strut accordingly.
            while (virtualPvs.start < estimated.start)
            {
                float height = bestEstimateOfWidgetHeight(*firstChild());
                removeItemWidget(virtualPvs.start++);
                virtualStrut->set(virtualStrut->value() + height);
            }
            DENG2_ASSERT(virtualPvs.size() == int(container->childCount()));

            // Remove excess widgets from the bottom.
            while (virtualPvs.end > estimated.end)
            {
                removeItemWidget(--virtualPvs.end);
            }
            DENG2_ASSERT(virtualPvs.size() == int(container->childCount()));
        }

        DENG2_ASSERT(virtualPvs.size() == int(container->childCount()));

        // Take note of how big a difference there is between the ideal distance and
        // the virtual top of the list.
        if (oldPvs.start != virtualPvs.start)
        {
            // Calculate a correction delta to be applied while the view is scrolling.
            // This will ensure that differences in item heights will not accumulate
            // and cause the estimated PVS to become too inaccurate.
            float error = virtualStrut->value() - estimated.start * averageItemHeight;
            correctionPerUnit = -error / GuiWidget::toDevicePixels(100);
            totalCorrection = de::abs(error);
        }
        // Apply correction to the virtual strut.
        if (!fequal(delta, 0.f))
        {
            float applied = correctionPerUnit * de::abs(delta);
            if (de::abs(applied) > totalCorrection)
            {
                applied = de::sign(applied) * totalCorrection;
            }
            virtualStrut->set(virtualStrut->value() + applied);
        }
    }

    DENG2_PIMPL_AUDIENCE(WidgetCreation)
    DENG2_PIMPL_AUDIENCE(WidgetUpdate)
};

DENG2_AUDIENCE_METHOD(ChildWidgetOrganizer, WidgetCreation)
DENG2_AUDIENCE_METHOD(ChildWidgetOrganizer, WidgetUpdate)

ChildWidgetOrganizer::ChildWidgetOrganizer(GuiWidget &container)
    : d(new Impl(this, &container))
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

void ChildWidgetOrganizer::setVirtualizationEnabled(bool enabled)
{
    d->virtualEnabled = enabled;
    d->virtualPvs     = Impl::PvsRange();

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

void ChildWidgetOrganizer::setRecyclingEnabled(bool enabled)
{
    d->recyclingEnabled = enabled;
}

void ChildWidgetOrganizer::setVirtualTopEdge(Rule const &topEdge)
{
    changeRef(d->virtualTop, topEdge);
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

int ChildWidgetOrganizer::averageChildHeight() const
{
    return d->averageItemHeight;
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
