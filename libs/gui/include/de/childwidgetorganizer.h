/** @file childwidgetorganizer.h Organizes widgets according to a UI context.
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

#ifndef LIBAPPFW_CHILDWIDGETORGANIZER_H
#define LIBAPPFW_CHILDWIDGETORGANIZER_H

#include "libgui.h"
#include "de/ui/data.h"
#include "de/guiwidget.h"

namespace de {

/**
 * Utility class that observes changes in a ui::Context and updates a parent
 * widget's children to reflect the UI context's contents. This involves
 * creating the corresponding widgets, updating them when the context items
 * change, and reordering them when the items' order changes.
 *
 * The concrete task of creating widgets is done by an object that implements
 * the ChildWidgetOrganizer::IWidgetFactory interface. Also, third parties
 * may observe widget creation and updates and alter the widget as they choose.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC ChildWidgetOrganizer
{
public:
    /**
     * Constructs widgets for the organizer.
     */
    class IWidgetFactory
    {
    public:
        virtual ~IWidgetFactory() = default;

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
        virtual GuiWidget *makeItemWidget(const ui::Item &item, const GuiWidget *parent) = 0;

        /**
         * Called whenever the item's content changes and this should be reflected
         * in the widget.
         *
         * @param widget  Widget that represents the item.
         * @param item    Item that has the content.
         */
        virtual void updateItemWidget(GuiWidget &widget, const ui::Item &item) = 0;
    };

public:
    ChildWidgetOrganizer(GuiWidget &container);

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
    void setContext(const ui::Data &context);

    void unsetContext();

    const ui::Data &context() const;

    GuiWidget *itemWidget(ui::Data::Pos pos) const;
    GuiWidget *itemWidget(const ui::Item &item) const;
    GuiWidget *itemWidget(const String &label) const;

    const ui::Item *findItemForWidget(const GuiWidget &widget) const;

//- Child Widget Virtualization ---------------------------------------------------------

    /**
     * Enables or disables child widget virtualization. When enabled, widgets are
     * only created for items that are potentially visible.
     *
     * Virtualization is necessary when the set of data items is very large.
     *
     * If enabled, you must also provide the top and bottom rules for the visible
     * area, and the approximated average child widget height.
     *
     * @param enabled  Enable or disable child virtualization.
     */
    void setVirtualizationEnabled(bool enabled);

    /**
     * Enables or disables child recycling. Deleted children will be put up for
     * recycling instead of being deleted, and new children will first be taken
     * from the set of old recycled widgets.
     *
     * It is only possible to use this when all the items being managed have the
     * same kind of widget representing them.
     */
    void setRecyclingEnabled(bool enabled);

    void setVirtualTopEdge(const Rule &topEdge);

    void setVisibleArea(const Rule &minimum, const Rule &maximum);

    bool virtualizationEnabled() const;

    /**
     * Returns the rule that defines the height of all the currently nonexistent widgets
     * above the first visible child. This will be automatically updated as children are
     * recycled and the top child changes.
     *
     * The initial value is zero.
     */
    const Rule &virtualStrut() const;

    /**
     * The average child height is used when estimating the maximum number of widgets
     * that can be created.
     *
     * @param height  Average child height.
     */
    void setAverageChildHeight(int height);

    int averageChildHeight() const;

    const Rule &estimatedTotalHeight() const;

    /**
     * After child widgets have been moved around, this must be called to update the
     * potentially visible item range and to recycle any widgets that are outside the
     * range. Items entering the range will be given widgets.
     */
    void updateVirtualization();

public:
    /**
     * Notified when the organizer creates a widget for a context item. Allows
     * third parties to customize the widget as needed.
     */
    DE_AUDIENCE(WidgetCreation,
                           void widgetCreatedForItem(GuiWidget &widget,
                                                     const ui::Item &item))

    /**
     * Notified when the organizer updates a widget for a changed context item.
     * Allows third parties to customize the widget as needed.
     */
    DE_AUDIENCE(WidgetUpdate,
                           void widgetUpdatedForItem(GuiWidget &widget,
                                                     const ui::Item &item))

private:
    DE_PRIVATE(d)
};

/**
 * Simple widget factory that creates label widgets with their default
 * settings, using the label from the ui::Item.
 */
class DefaultWidgetFactory : public ChildWidgetOrganizer::IWidgetFactory
{
public:
    GuiWidget *makeItemWidget(const ui::Item &item, const GuiWidget *parent);
    void updateItemWidget(GuiWidget &widget, const ui::Item &item);
};

} // namespace de

#endif // LIBAPPFW_CHILDWIDGETORGANIZER_H
