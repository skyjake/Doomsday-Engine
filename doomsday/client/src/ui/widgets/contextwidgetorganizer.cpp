/** @file contextwidgetorganizer.cpp Organizes widgets according to a UI context.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/contextwidgetorganizer.h"
#include "ui/widgets/labelwidget.h"
#include "ui/widgets/item.h"

#include <QMap>

using namespace de;

static DefaultWidgetFactory defaultWidgetFactory;

DENG2_PIMPL(ContextWidgetOrganizer),
DENG2_OBSERVES(Widget,      Deletion   ),
DENG2_OBSERVES(ui::Context, Addition   ),
DENG2_OBSERVES(ui::Context, Removal    ),
DENG2_OBSERVES(ui::Context, OrderChange),
DENG2_OBSERVES(ui::Item,    Change     )
{
    GuiWidget *container;
    ui::Context const *context;
    IWidgetFactory *factory;

    typedef QMap<ui::Item const *, GuiWidget *> Mapping;
    typedef QMutableMapIterator<ui::Item const *, GuiWidget *> MutableMappingIterator;
    Mapping mapping; ///< Maps items to corresponding widgets.

    Instance(Public *i, GuiWidget *c)
        : Base(i),
          container(c),
          context(0),
          factory(&defaultWidgetFactory)
    {}

    ~Instance()
    {
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            i.value()->audienceForDeletion -= this;
        }
    }

    void set(ui::Context const *ctx)
    {
        if(context)
        {
            context->audienceForAddition -= this;
            context->audienceForRemoval -= this;
            context->audienceForOrderChange -= this;

            clearWidgets();
            context = 0;
        }

        context = ctx;

        if(context)
        {
            makeWidgets();

            context->audienceForAddition += this;
            context->audienceForRemoval += this;
            context->audienceForOrderChange += this;
        }
    }

    void addItemWidget(ui::Context::Pos pos, bool alwaysAppend = false)
    {
        DENG2_ASSERT(factory != 0);

        ui::Item const &item = context->at(pos);
        GuiWidget *w = factory->makeItemWidget(item, container);
        if(!w) return; // Unpresentable.

        // Others may alter the widget in some way.
        DENG2_FOR_PUBLIC_AUDIENCE(WidgetCreation, i)
        {
            i->widgetCreatedForItem(*w, item);
        }

        // Update the widget immediately.
        mapping.insert(&item, w);
        itemChanged(item);

        if(alwaysAppend || pos == context->size() - 1)
        {
            // This is the last item.
            container->add(w);
        }
        else
        {
            container->insertBefore(w, *mapping[&context->at(pos + 1)]);
        }

        // Observe.
        w->audienceForDeletion += this; // in case it's manually deleted
        item.audienceForChange += this;
    }

    void makeWidgets()
    {
        DENG2_ASSERT(context != 0);
        DENG2_ASSERT(container != 0);

        for(ui::Context::Pos i = 0; i < context->size(); ++i)
        {
            addItemWidget(i, true /*always append*/);
        }
    }

    void deleteWidget(GuiWidget *w)
    {
        w->audienceForDeletion -= this;
        GuiWidget::destroy(w);
    }

    void clearWidgets()
    {
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            i.key()->audienceForChange -= this;

            deleteWidget(i.value());
        }
        mapping.clear();
    }

    void widgetBeingDeleted(Widget &widget)
    {
        /*
         * Note: this should not occur normally, as the widgets created by the
         * Organizer are not usually manually deleted.
         */
        MutableMappingIterator iter(mapping);
        while(iter.hasNext())
        {
            iter.next();
            if(iter.value() == &widget)
            {
                iter.remove();
                break;
            }
        }
    }

    void contextItemAdded(ui::Context::Pos pos, ui::Item const &)
    {
        addItemWidget(pos);
    }

    void contextItemRemoved(ui::Context::Pos, ui::Item &item)
    {
        Mapping::const_iterator found = mapping.constFind(&item);
        if(found != mapping.constEnd())
        {
            found.key()->audienceForChange -= this;
            deleteWidget(found.value());
            mapping.remove(found.key());
        }
    }

    void contextItemOrderChanged()
    {
        // Remove all widgets and put them back in the correct order.
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            container->remove(*i.value());
        }
        for(ui::Context::Pos i = 0; i < context->size(); ++i)
        {
            DENG2_ASSERT(mapping.contains(&context->at(i)));
            container->add(mapping[&context->at(i)]);
        }
    }

    void itemChanged(ui::Item const &item)
    {
        DENG2_ASSERT(mapping.contains(&item));

        GuiWidget &w = *mapping[&item];
        factory->updateItemWidget(w, item);

        // Notify.
        DENG2_FOR_PUBLIC_AUDIENCE(WidgetUpdate, i)
        {
            i->widgetUpdatedForItem(w, item);
        }
    }

    GuiWidget *find(ui::Item const &item) const
    {
        Mapping::const_iterator found = mapping.constFind(&item);
        if(found == mapping.constEnd()) return 0;
        return found.value();
    }
};

ContextWidgetOrganizer::ContextWidgetOrganizer(GuiWidget &container)
    : d(new Instance(this, &container))
{}

void ContextWidgetOrganizer::setContext(ui::Context const &context)
{
    d->set(&context);
}

void ContextWidgetOrganizer::unsetContext()
{
    d->set(0);
}

ui::Context const &ContextWidgetOrganizer::context() const
{
    DENG2_ASSERT(d->context != 0);
    return *d->context;
}

GuiWidget *ContextWidgetOrganizer::itemWidget(ui::Context::Pos pos) const
{
    return itemWidget(context().at(pos));
}

void ContextWidgetOrganizer::setWidgetFactory(IWidgetFactory &factory)
{
    d->factory = &factory;
}

ContextWidgetOrganizer::IWidgetFactory &ContextWidgetOrganizer::widgetFactory() const
{
    DENG2_ASSERT(d->factory != 0);
    return *d->factory;
}

GuiWidget *ContextWidgetOrganizer::itemWidget(ui::Item const &item) const
{
    return d->find(item);
}

GuiWidget *DefaultWidgetFactory::makeItemWidget(ui::Item const &, GuiWidget const *)
{
    return new LabelWidget;
}

void DefaultWidgetFactory::updateItemWidget(GuiWidget &widget, ui::Item const &item)
{
    widget.as<LabelWidget>().setText(item.label());
}
