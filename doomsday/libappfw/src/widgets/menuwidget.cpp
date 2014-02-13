/** @file menuwidget.cpp
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

#include "de/MenuWidget"
#include "de/PopupMenuWidget"
#include "de/VariableToggleWidget"
#include "de/ChildWidgetOrganizer"
#include "de/GridLayout"
#include "de/ui/ListData"
#include "de/ui/ActionItem"
#include "de/ui/SubwidgetItem"

namespace de {

using namespace ui;

DENG2_PIMPL(MenuWidget)
, DENG2_OBSERVES(Data, Addition)    // for layout update
, DENG2_OBSERVES(Data, Removal)     // for layout update
, DENG2_OBSERVES(Data, OrderChange) // for layout update
, DENG2_OBSERVES(PopupWidget, Close)
, DENG2_OBSERVES(Widget, Deletion)
, public ChildWidgetOrganizer::IWidgetFactory
{
    /**
     * Base class for sub-widget actions. Handles ownership/openness tracking.
     */
    class SubAction : public de::Action, DENG2_OBSERVES(Widget, Deletion)
    {
    public:
        SubAction(Instance *inst, ui::Item const &parentItem)
            : d(inst)
            , _parentItem(parentItem)
            , _dir(ui::Right)
            , _widget(0)
        {}

        ~SubAction()
        {
            if(_widget)
            {
                delete _widget;
            }
        }

        void setWidget(PopupWidget *w, ui::Direction openingDirection)
        {
            _widget = w;

            // Popups need a parent.
            d->self.add(_widget);

            _widget->audienceForDeletion += this;
            _dir = openingDirection;
        }

        void widgetBeingDeleted(Widget &)
        {
            _widget = 0;
        }

        bool isTriggered() const
        {
            return _widget != 0;
        }

        void trigger()
        {
            Action::trigger();

            DENG2_ASSERT(_widget != 0);

            GuiWidget *parent = d->organizer.itemWidget(_parentItem);
            DENG2_ASSERT(parent != 0);

            _widget->setAnchorAndOpeningDirection(parent->hitRule(), _dir);

            d->keepTrackOfSubWidget(_widget);
            _widget->open();
        }

    protected:
        Instance *d;
        ui::Item const &_parentItem;
        ui::Direction _dir;
        PopupWidget *_widget;
    };

    /**
     * Action owned by the button that represents a SubmenuItem.
     */
    class SubmenuAction : public SubAction
    {
    public:
        SubmenuAction(Instance *inst, ui::SubmenuItem const &parentItem)
            : SubAction(inst, parentItem)
        {
            PopupMenuWidget *sub = new PopupMenuWidget;
            setWidget(sub, parentItem.openingDirection());

            // Use the items from the submenu.
            sub->menu().setItems(parentItem.items());
        }
    };

    /**
     * Action owned by the button that represents a SubwidgetItem.
     */
    class SubwidgetAction : public SubAction
    {
    public:
        SubwidgetAction(Instance *inst, ui::SubwidgetItem const &parentItem)
            : SubAction(inst, parentItem)
            , _item(parentItem)
        {}

        void trigger()
        {
            if(isTriggered()) return; // Already open, cannot retrigger.

            // The widget is created only at this point.
            setWidget(_item.makeWidget(), _item.openingDirection());
            _widget->setDeleteAfterDismissed(true);

            SubAction::trigger();
        }

    private:
        ui::SubwidgetItem const &_item;
    };

    bool needLayout;
    GridLayout layout;
    ListData defaultItems;
    Data const *items;
    ChildWidgetOrganizer organizer;
    QSet<PanelWidget *> openSubs;

    SizePolicy colPolicy;
    SizePolicy rowPolicy;

    Instance(Public *i)
        : Base(i),
          needLayout(false),
          items(0),
          organizer(self),
          colPolicy(Fixed),
          rowPolicy(Fixed)
    {
        // We will create widgets ourselves.
        organizer.setWidgetFactory(*this);

        // The default context is empty.        
        setContext(&defaultItems);
    }

    ~Instance()
    {
        // Clear the data model first, so possible sub-widgets are deleted at the right time.
        // Note that we can't clear an external data model.
        defaultItems.clear();
    }

    void setContext(Data const *ctx)
    {
        if(items)
        {
            // Get rid of the old context.
            items->audienceForAddition -= this;
            items->audienceForRemoval -= this;
            items->audienceForOrderChange -= this;
            organizer.unsetContext();
        }

        items = ctx;

        // Take new context into use.
        items->audienceForAddition += this;
        items->audienceForRemoval += this;
        items->audienceForOrderChange += this;
        organizer.setContext(*items); // recreates widgets
    }

    void contextItemAdded(Data::Pos, Item const &)
    {
        // Make sure we determine the layout for the new item.
        needLayout = true;
    }

    void contextItemRemoved(Data::Pos, Item &)
    {
        // Make sure we determine the layout after this item is gone.
        needLayout = true;
    }

    void contextItemOrderChanged()
    {
        // Make sure we determine the layout for the new order.
        needLayout = true;
    }

    /*
     * Menu items are represented as buttons and labels.
     */
    GuiWidget *makeItemWidget(Item const &item, GuiWidget const *)
    {
        if(item.semantics().testFlag(Item::ShownAsButton))
        {
            // Normal clickable button.
            ButtonWidget *b = new ButtonWidget;
            b->setTextAlignment(ui::AlignRight);

            if(item.is<SubmenuItem>())
            {
                b->setAction(new SubmenuAction(this, item.as<SubmenuItem>()));
            }
            else if(item.is<SubwidgetItem>())
            {
                b->setAction(new SubwidgetAction(this, item.as<SubwidgetItem>()));
            }
            return b;
        }
        else if(item.semantics().testFlag(Item::Separator))
        {
            LabelWidget *lab = new LabelWidget;
            lab->setAlignment(ui::AlignLeft);
            lab->setTextLineAlignment(ui::AlignLeft);
            lab->setSizePolicy(ui::Expand, ui::Expand);
            return lab;
        }
        else if(item.semantics().testFlag(Item::ShownAsLabel))
        {
            LabelWidget *lab = new LabelWidget;
            lab->setTextAlignment(ui::AlignRight);
            lab->setTextLineAlignment(ui::AlignLeft);
            lab->setSizePolicy(ui::Expand, ui::Expand);
            return lab;
        }
        else if(item.semantics().testFlag(Item::ShownAsToggle))
        {
            // We know how to present variable toggles.
            if(VariableToggleItem const *varTog = item.maybeAs<VariableToggleItem>())
            {
                return new VariableToggleWidget(varTog->variable());
            }
            else
            {
                // A regular toggle.
                return new ToggleWidget;
            }
        }
        return 0;
    }

    void updateItemWidget(GuiWidget &widget, Item const &item)
    {
        if(ActionItem const *act = item.maybeAs<ActionItem>())
        {
            if(item.semantics().testFlag(Item::ShownAsButton))
            {
                ButtonWidget &b = widget.as<ButtonWidget>();
                b.setImage(act->image());
                b.setText(act->label());
                if(act->action())
                {
                    b.setAction(*act->action());
                }
            }
            else if(item.semantics().testFlag(Item::ShownAsLabel))
            {
                widget.as<LabelWidget>().setText(item.label());
            }
            else if(item.semantics().testFlag(Item::ShownAsToggle))
            {
                ToggleWidget &t = widget.as<ToggleWidget>();
                t.setText(act->label());
                if(act->action())
                {
                    t.setAction(*act->action());
                }
            }
        }
        else
        {
            // Other kinds of items are represented as labels or
            // label-derived widgets.
            widget.as<LabelWidget>().setText(item.label());

            if(SubwidgetItem const *sub = item.maybeAs<SubwidgetItem>())
            {
                widget.as<LabelWidget>().setImage(sub->image());
            }
        }
    }

    void panelBeingClosed(PanelWidget &popup)
    {        
        openSubs.remove(&popup);
    }

    void widgetBeingDeleted(Widget &widget)
    {
        openSubs.remove(static_cast<PanelWidget *>(&widget));
    }

    void keepTrackOfSubWidget(PanelWidget *w)
    {
        DENG2_ASSERT(w->is<PanelWidget>());

        openSubs.insert(w);

        w->audienceForClose += this;
        w->audienceForDeletion += this;
    }

    bool isVisibleItem(Widget const *child) const
    {
        if(GuiWidget const *widget = child->maybeAs<GuiWidget>())
        {
            return !widget->behavior().testFlag(Widget::Hidden);
        }
        return false;
    }

    int countVisible() const
    {
        int num = 0;
        foreach(Widget *i, self.childWidgets())
        {
            if(isVisibleItem(i)) ++num;
        }
        return num;
    }

    void relayout()
    {
        layout.clear();

        foreach(Widget *child, self.childWidgets())
        {
            GuiWidget *w = child->maybeAs<GuiWidget>();
            if(!isVisibleItem(w)) continue;

            layout << *w;
        }
    }
};

MenuWidget::MenuWidget(String const &name)
    : ScrollAreaWidget(name), d(new Instance(this))
{}

void MenuWidget::setGridSize(int columns, ui::SizePolicy columnPolicy,
                             int rows, ui::SizePolicy rowPolicy,
                             GridLayout::Mode layoutMode)
{
    d->layout.clear();
    d->layout.setModeAndGridSize(layoutMode, columns, rows);
    d->layout.setLeftTop(contentRule().left(), contentRule().top());

    d->colPolicy = columnPolicy;
    d->rowPolicy = rowPolicy;

    if(d->colPolicy == ui::Filled)
    {
        DENG2_ASSERT(columns > 0);
        d->layout.setOverrideWidth((rule().width() - margins().width() -
                                    (columns - 1) * d->layout.columnPadding()) / float(columns));
    }

    if(d->rowPolicy == ui::Filled)
    {
        DENG2_ASSERT(rows > 0);
        d->layout.setOverrideHeight((rule().height() - margins().height() -
                                     (rows - 1) * d->layout.rowPadding()) / float(rows));
    }

    d->needLayout = true;
}

Data &MenuWidget::items()
{
    return *const_cast<Data *>(d->items);
}

Data const &MenuWidget::items() const
{
    return *d->items;
}

void MenuWidget::setItems(Data const &items)
{
    d->setContext(&items);
}

void MenuWidget::useDefaultItems()
{
    d->setContext(&d->defaultItems);
}

int MenuWidget::count() const
{
    return d->countVisible();
}

bool MenuWidget::isWidgetPartOfMenu(Widget const &widget) const
{
    if(widget.parent() != this) return false;
    return d->isVisibleItem(&widget);
}

void MenuWidget::updateLayout()
{
    d->relayout();

    setContentSize(d->layout.width(), d->layout.height());

    // Expanding policy causes the size of the menu widget to change.
    if(d->colPolicy == Expand)
    {
        rule().setInput(Rule::Width, d->layout.width() + margins().width());
    }
    if(d->rowPolicy == Expand)
    {
        rule().setInput(Rule::Height, d->layout.height() + margins().height());
    }

    d->needLayout = false;
}

GridLayout const &MenuWidget::layout() const
{
    return d->layout;
}

GridLayout &MenuWidget::layout()
{
    return d->layout;
}

ChildWidgetOrganizer &MenuWidget::organizer()
{
    return d->organizer;
}

ChildWidgetOrganizer const &MenuWidget::organizer() const
{
    return d->organizer;
}

void MenuWidget::update()
{
    if(d->needLayout)
    {
        updateLayout();
    }

    ScrollAreaWidget::update();
}

bool MenuWidget::handleEvent(Event const &event)
{
    return ScrollAreaWidget::handleEvent(event);
}

void MenuWidget::dismissPopups()
{
    foreach(PanelWidget *pop, d->openSubs)
    {
        pop->close();
    }
}

} // namespace de
