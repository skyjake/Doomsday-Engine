/** @file menuwidget.cpp
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

#include "ui/widgets/menuwidget.h"
#include "ui/widgets/popupmenuwidget.h"
#include "ui/widgets/variabletogglewidget.h"
#include "ChildWidgetOrganizer"
#include "ui/ListData"
#include "ui/ActionItem"
#include "GridLayout"

using namespace de;
using namespace ui;

DENG2_PIMPL(MenuWidget),
DENG2_OBSERVES(Data, Addition),    // for layout update
DENG2_OBSERVES(Data, Removal),     // for layout update
DENG2_OBSERVES(Data, OrderChange), // for layout update
DENG2_OBSERVES(PopupWidget, Close),
public ChildWidgetOrganizer::IWidgetFactory
{
    /**
     * Action owned by the button that represents a SubmenuItem.
     */
    class SubmenuAction : public de::Action, DENG2_OBSERVES(Widget, Deletion)
    {
    public:
        SubmenuAction(Instance *inst, ui::SubmenuItem const &parentItem)
            : d(inst), _submenu(parentItem)
        {
            _widget = new PopupMenuWidget;
            _widget->audienceForDeletion += this;

            // Use the items from the submenu.
            _widget->menu().setItems(parentItem.items());

            // Popups need a parent; hidden items are ignored by the menu.
            d->self.add(_widget);
        }

        ~SubmenuAction()
        {
            if(_widget)
            {
                delete _widget;
            }
        }

        void widgetBeingDeleted(Widget &)
        {
            _widget = 0;
        }

        void trigger()
        {
            Action::trigger();

            DENG2_ASSERT(_widget != 0);

            GuiWidget *parent = d->organizer.itemWidget(_submenu);
            DENG2_ASSERT(parent != 0);

            _widget->setAnchorAndOpeningDirection(parent->hitRule(), _submenu.openingDirection());

            d->openPopups.insert(_widget);
            _widget->audienceForClose += d;

            _widget->open();
        }

        Action *duplicate() const
        {
            DENG2_ASSERT(false); // not needed
            return 0;
        }

    private:
        Instance *d;
        ui::SubmenuItem const &_submenu;
        PopupMenuWidget *_widget;
    };

    bool needLayout;
    GridLayout layout;
    ListData defaultItems;
    Data const *items;
    ChildWidgetOrganizer organizer;
    QSet<PopupWidget *> openPopups;

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

    void contextItemAdded(Data::Pos id, Item const &)
    {
        // Make sure we determine the layout for the new item.
        needLayout = true;
    }

    void contextItemRemoved(Data::Pos id, Item &)
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
    GuiWidget *makeItemWidget(Item const &item, GuiWidget const *parent)
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
                    b.setAction(act->action()->duplicate());
                }
            }
            else if(item.semantics().testFlag(Item::ShownAsToggle))
            {
                ToggleWidget &t = widget.as<ToggleWidget>();
                t.setText(act->label());
                if(act->action())
                {
                    t.setAction(act->action()->duplicate());
                }
            }
        }
        else
        {
            // Other kinds of items are represented as labels or
            // label-derived widgets.
            widget.as<LabelWidget>().setText(item.label());
        }
    }

    void panelBeingClosed(PanelWidget &popup)
    {
        openPopups.remove(&popup.as<PopupWidget>());
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
        d->layout.setOverrideWidth((rule().width() - margins().width()) / float(columns));
    }

    if(d->rowPolicy == ui::Filled)
    {
        DENG2_ASSERT(rows > 0);
        d->layout.setOverrideHeight((rule().height() - margins().height()) / float(rows));
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
    //if(isHidden()) return;

    if(d->needLayout)
    {
        updateLayout();
    }

    ScrollAreaWidget::update();
}

bool MenuWidget::handleEvent(Event const &event)
{
#if 0
    if(event.type() == Event::MousePosition)
    {
        if(rule().recti().contains(event.as<MouseEvent>().pos()))
        {
            // Eat position events inside the menu area.
            return true;
        }
    }
#endif
    return ScrollAreaWidget::handleEvent(event);
}

void MenuWidget::dismissPopups()
{
    foreach(PopupWidget *pop, d->openPopups)
    {
        pop->close();
    }
}
