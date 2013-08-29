/** @file contextwidgetorganizer.h Organizes widgets according to a UI context.
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

#ifndef DENG_CLIENT_CONTEXTWIDGETORGANIZER_H
#define DENG_CLIENT_CONTEXTWIDGETORGANIZER_H

#include "data.h"
#include "guiwidget.h"

/**
 * Utility class that observes changes in a ui::Context and updates a parent
 * widget's children to reflect the UI context's contents. This involves
 * creating the corresponding widgets, updating them when the context items
 * change, and reordering them when the items' order changes.
 *
 * The concrete task of creating widgets is done by an object that implements
 * the ContextWidgetOrganizer::IWidgetFactory interface. Also, third parties
 * may observe widget creation and updates and alter the widget as they choose.
 *
 * @todo Virtualization: it is not required that all the items of the context
 * are represented by widgets on screen at the same time. In contexts with
 * large numbers of items, virtualization should be applied to keep only a
 * subset/range of items present as widgets.
 */
class ContextWidgetOrganizer
{
public:
    /**
     * Constructs widgets for the organizer.
     */
    class IWidgetFactory
    {
    public:
        virtual ~IWidgetFactory() {}

        /**
         * Called when the organizer needs a widget for a context item. This allows
         * the specialized organizers to choose the widget type and customize it
         * appropriately.
         *
         * After construction, the widget is automatically updated with
         * updateItemWidget().
         *
         * @param item    Item that has the content.
         * @param parent  Future parent of the widget, if any (can be @c NULL).
         */
        virtual GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *parent) = 0;

        /**
         * Called whenever the item's content changes and this should be reflected
         * in the widget.
         *
         * @param widget  Widget that represents the item.
         * @param item    Item that has the content.
         */
        virtual void updateItemWidget(GuiWidget &widget, ui::Item const &item) = 0;
    };

    /**
     * Notified when the organizer creates a widget for a context item. Allows
     * third parties to customize the widget as needed.
     */
    DENG2_DEFINE_AUDIENCE(WidgetCreation,
                          void widgetCreatedForItem(GuiWidget &widget,
                                                    ui::Item const &item))

    /**
     * Notified when the organizer updates a widget for a changed context item.
     * Allows third parties to customize the widget as needed.
     */
    DENG2_DEFINE_AUDIENCE(WidgetUpdate,
                          void widgetUpdatedForItem(GuiWidget &widget,
                                                    ui::Item const &item))

public:
    ContextWidgetOrganizer(GuiWidget &container);

    /**
     * Sets the object responsible for creating widgets for this organizer. The
     * default factory creates labels with their default settings. The factory
     * should be set before calling setContext().
     *
     * @param factory  Widget factory.
     */
    void setWidgetFactory(IWidgetFactory &factory);

    IWidgetFactory &widgetFactory() const;

    /**
     * Sets the data context of the organizer. If there was a previous context,
     * all widgets created for it are deleted from the container. The widgets
     * are immediately constructed using the current factory.
     *
     * @param context  Context with items.
     */
    void setContext(ui::Data const &context);

    void unsetContext();

    ui::Data const &context() const;

    GuiWidget *itemWidget(ui::Data::Pos pos) const;
    GuiWidget *itemWidget(ui::Item const &item) const;
    GuiWidget *itemWidget(de::String const &label) const;

private:
    DENG2_PRIVATE(d)
};

/**
 * Simple widget factory that creates label widgets with their default
 * settings, using the label from the ui::Item.
 */
class DefaultWidgetFactory : public ContextWidgetOrganizer::IWidgetFactory
{
public:
    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *parent);
    void updateItemWidget(GuiWidget &widget, ui::Item const &item);
};

#endif // DENG_CLIENT_CONTEXTWIDGETORGANIZER_H
