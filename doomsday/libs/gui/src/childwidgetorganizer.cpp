/** @file childwidgetorganizer.cpp Organizes widgets according to a UI context.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/childwidgetorganizer.h"
#include "de/labelwidget.h"
#include "de/ui/item.h"

#include <de/app.h>
#include <de/keymap.h>

namespace de {

namespace internal
{
    enum AddBehavior
    {
        AlwaysAppend    = 0x1,
        AlwaysPrepend   = 0x2,
        DefaultBehavior = 0,
    };
    using AddBehaviors = Flags;
}

using namespace internal;

static DefaultWidgetFactory defaultWidgetFactory;

DE_PIMPL(ChildWidgetOrganizer)
    , DE_OBSERVES(Widget,   Deletion   )
    , DE_OBSERVES(ui::Data, Addition   )
    , DE_OBSERVES(ui::Data, Removal    )
    , DE_OBSERVES(ui::Data, OrderChange)
    , DE_OBSERVES(ui::Item, Change     )
{
    typedef Rangei PvsRange;

    const ui::Data *dataItems = nullptr;
    GuiWidget *container;
    IWidgetFactory *factory;

    typedef KeyMap<const ui::Item *, GuiWidget *> Mapping;
    Mapping mapping; ///< Maps items to corresponding widgets.

    bool virtualEnabled = false;
    const Rule *virtualTop = nullptr;
    const Rule *virtualMin = nullptr;
    const Rule *virtualMax = nullptr;
    ConstantRule *virtualStrut = nullptr;
    ConstantRule *estimatedHeight = nullptr;
    int averageItemHeight = 0;
    PvsRange virtualPvs;
    float lastTop = 0;
    float totalCorrection = 0;
    float correctionPerUnit = 0;

    bool recyclingEnabled = false;
    List<GuiWidget *> recycledWidgets; // Not GL-deinitialized to facilitate fast reuse.

    Impl(Public *i, GuiWidget *c)
        : Base(i)
        , container(c)
        , factory(&defaultWidgetFactory)
    {}

    ~Impl()
    {
        for (GuiWidget *recycled : recycledWidgets)
        {
            GuiWidget::destroy(recycled);
        }

        releaseRef(virtualTop);
        releaseRef(virtualMin);
        releaseRef(virtualMax);
        releaseRef(virtualStrut);
        releaseRef(estimatedHeight);
    }

    void set(const ui::Data *ctx)
    {
        if (dataItems)
        {
            dataItems->audienceForAddition() -= this;
            dataItems->audienceForRemoval() -= this;
            dataItems->audienceForOrderChange() -= this;

            clearWidgets();
            dataItems = nullptr;
        }

        dataItems = ctx;
        virtualPvs = {}; // force full update

        if (dataItems)
        {
            updateVirtualHeight();
            makeWidgets();

            dataItems->audienceForAddition() += this;
            dataItems->audienceForRemoval() += this;
            dataItems->audienceForOrderChange() += this;
        }
    }

    PvsRange itemRange() const
    {
        PvsRange range(0, int(dataItems->size()));
        if (virtualEnabled) range = range.intersection(virtualPvs);
        return range;
    }

    GuiWidget *addItemWidget(ui::Data::Pos pos, AddBehaviors behavior = DefaultBehavior)
    {
        DE_ASSERT_IN_MAIN_THREAD(); // widgets should only be manipulated in UI thread
        DE_ASSERT(factory != nullptr);

        if (!itemRange().contains(int(pos)))
        {
            // Outside the current potentially visible range.
            return nullptr;
        }

        const ui::Item &item = dataItems->at(pos);

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

        mapping.insert(&item, w);

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
        DE_NOTIFY_PUBLIC(WidgetCreation, i)
        {
            i->widgetCreatedForItem(*w, item);
        }

        // Update the widget immediately.
        itemChanged(item);

        // Observe.
        w->audienceForDeletion() += this; // in case it's manually deleted
        item.audienceForChange() += this;

        return w;
    }

    void removeItemWidget(ui::DataPos pos)
    {
        const ui::Item *item = &dataItems->at(pos);
        auto found = mapping.find(item);
        if (found != mapping.end())
        {
            GuiWidget *w = found->second;
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
            auto found = mapping.find(&dataItems->at(afterPos));
            if (found != mapping.end())
            {
                return found->second;
            }
        }
        return nullptr;
    }

    void makeWidgets()
    {
        DE_ASSERT(dataItems != nullptr);
        DE_ASSERT(container != nullptr);

        if (virtualEnabled)
        {
            updateVirtualization();
        }
        else
        {
            // Create a widget for each item.
            for (ui::Data::Pos i = 0; i < dataItems->size(); ++i)
            {
                addItemWidget(i, AlwaysAppend);
            }
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
        DE_FOR_EACH_CONST(Mapping, i, mapping)
        {
            i->first->audienceForChange() -= this;
            deleteWidget(i->second);
        }
        mapping.clear();
    }

    void widgetBeingDeleted(Widget &widget)
    {
        for (auto i = mapping.begin(); i != mapping.end(); ++i)
        {
            if (i->second == &widget)
            {
                mapping.erase(i);
                break;
            }
        }
    }

    void dataItemAdded(ui::DataPos pos, const ui::Item &)
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
            found->first->audienceForChange() -= this;
            deleteWidget(found->second);
            mapping.erase(found);
        }

        if (virtualEnabled)
        {
            if (virtualPvs.contains(int(pos)))
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
        DE_FOR_EACH_CONST(Mapping, i, mapping)
        {
            container->remove(*i->second);
        }
        for (ui::Data::Pos i = 0; i < dataItems->size(); ++i)
        {
            if (mapping.contains(&dataItems->at(i)))
            {
                container->add(mapping[&dataItems->at(i)]);
            }
        }
    }

    void itemChanged(const ui::Item &item)
    {
        if (!mapping.contains(&item))
        {
            // Not represented as a child widget.
            return;
        }

        GuiWidget &w = *mapping[&item];
        factory->updateItemWidget(w, item);

        // Notify.
        DE_NOTIFY_PUBLIC(WidgetUpdate, i)
        {
            i->widgetUpdatedForItem(w, item);
        }
    }

    GuiWidget *find(const ui::Item &item) const
    {
        Mapping::const_iterator found = mapping.find(&item);
        if (found == mapping.end()) return nullptr;
        return found->second;
    }

    GuiWidget *findByLabel(const String &label) const
    {
        DE_FOR_EACH_CONST(Mapping, i, mapping)
        {
            if (i->first->label() == label)
            {
                return i->second;
            }
        }
        return nullptr;
    }

    const ui::Item *findByWidget(const GuiWidget &widget) const
    {
        DE_FOR_EACH_CONST(Mapping, i, mapping)
        {
            if (i->second == &widget)
            {
                return i->first;
            }
        }
        return nullptr;
    }

//- Child Widget Virtualization ---------------------------------------------------------

    void updateVirtualHeight()
    {
        if (virtualEnabled)
        {
            estimatedHeight->set(dataItems ? dataItems->size() * float(averageItemHeight) : 0.0f);
        }
    }

    GuiWidget *firstChild() const
    {
        return container->childWidgets().first();
    }

    GuiWidget *lastChild() const
    {
        return container->childWidgets().last();
    }

    float virtualItemHeight(const GuiWidget *widget) const
    {
        float hgt = widget->rule().height().value();
        if (hgt > 0)
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
        const PvsRange oldPvs = virtualPvs;

        // Calculate position delta to compared to last update.
        float delta = virtualTop->value() - lastTop;
        lastTop = virtualTop->value();

        // Estimate a new PVS range based on the average item height and the visible area.
        const float y1 = de::max(0.f, virtualMin->value() - virtualTop->value());
        const float y2 = de::max(0.f, virtualMax->value() - virtualTop->value());

        const int spareItems = 3;
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
            DE_ASSERT(virtualPvs.size() == int(container->childCount()));
        }
        else if (estimated.end > oldPvs.end) // Extend downward.
        {
            virtualPvs.end = estimated.end;
            for (auto pos = oldPvs.end; pos < virtualPvs.end; ++pos)
            {
                addItemWidget(pos, AlwaysAppend);
            }
            DE_ASSERT(virtualPvs.size() == int(container->childCount()));
        }
        else if (estimated.start < oldPvs.start) // Extend upward.
        {
            virtualPvs.start = estimated.start;
            for (auto pos = oldPvs.start - 1;
                 pos >= virtualPvs.start && pos < int(dataItems->size());
                 --pos)
            {
                GuiWidget *w = addItemWidget(pos, AlwaysPrepend);
                DE_ASSERT(w != nullptr);

                float height = bestEstimateOfWidgetHeight(*w);
                // Reduce strut length to make room for new items.
                virtualStrut->set(de::max(0.f, virtualStrut->value() - height));
            }
            DE_ASSERT(virtualPvs.size() == int(container->childCount()));
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
            DE_ASSERT(virtualPvs.size() == int(container->childCount()));

            // Remove excess widgets from the bottom.
            while (virtualPvs.end > estimated.end)
            {
                removeItemWidget(--virtualPvs.end);
            }
            DE_ASSERT(virtualPvs.size() == int(container->childCount()));
        }

        DE_ASSERT(virtualPvs.size() == int(container->childCount()));

        // Take note of how big a difference there is between the ideal distance and
        // the virtual top of the list.
        if (oldPvs.start != virtualPvs.start)
        {
            // Calculate a correction delta to be applied while the view is scrolling.
            // This will ensure that differences in item heights will not accumulate
            // and cause the estimated PVS to become too inaccurate.
            float error = virtualStrut->value() - estimated.start * averageItemHeight;
            correctionPerUnit = -error / GuiWidget::pointsToPixels(100);
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

    DE_PIMPL_AUDIENCE(WidgetCreation)
    DE_PIMPL_AUDIENCE(WidgetUpdate)
};

DE_AUDIENCE_METHOD(ChildWidgetOrganizer, WidgetCreation)
DE_AUDIENCE_METHOD(ChildWidgetOrganizer, WidgetUpdate)

ChildWidgetOrganizer::ChildWidgetOrganizer(GuiWidget &container)
    : d(new Impl(this, &container))
{}

void ChildWidgetOrganizer::setContext(const ui::Data &context)
{
    d->set(&context);
}

void ChildWidgetOrganizer::unsetContext()
{
    d->set(nullptr);
}

const ui::Data &ChildWidgetOrganizer::context() const
{
    DE_ASSERT(d->dataItems != nullptr);
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
    DE_ASSERT(d->factory != 0);
    return *d->factory;
}

GuiWidget *ChildWidgetOrganizer::itemWidget(const ui::Item &item) const
{
    return d->find(item);
}

GuiWidget *ChildWidgetOrganizer::itemWidget(const String &label) const
{
    return d->findByLabel(label);
}

const ui::Item *ChildWidgetOrganizer::findItemForWidget(const GuiWidget &widget) const
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

void ChildWidgetOrganizer::setVirtualTopEdge(const Rule &topEdge)
{
    changeRef(d->virtualTop, topEdge);
}

void ChildWidgetOrganizer::setVisibleArea(const Rule &minimum, const Rule &maximum)
{
    changeRef(d->virtualMin, minimum);
    changeRef(d->virtualMax, maximum);
}

bool ChildWidgetOrganizer::virtualizationEnabled() const
{
    return d->virtualEnabled;
}

const Rule &ChildWidgetOrganizer::virtualStrut() const
{
    DE_ASSERT(d->virtualEnabled);
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

const Rule &ChildWidgetOrganizer::estimatedTotalHeight() const
{
    return *d->estimatedHeight;
}

void ChildWidgetOrganizer::updateVirtualization()
{
    d->updateVirtualization();
}

GuiWidget *DefaultWidgetFactory::makeItemWidget(const ui::Item &, const GuiWidget *)
{
    return new LabelWidget;
}

void DefaultWidgetFactory::updateItemWidget(GuiWidget &widget, const ui::Item &item)
{
    widget.as<LabelWidget>().setText(item.label());
}

} // namespace de
