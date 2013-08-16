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
#include "ui/widgets/contextwidgetorganizer.h"
#include "ui/widgets/listcontext.h"
#include "ui/widgets/actionitem.h"
#include "ui/widgets/variabletoggleitem.h"
#include "ui/widgets/variabletogglewidget.h"

using namespace de;
using namespace ui;

DENG2_PIMPL(MenuWidget),
DENG2_OBSERVES(Context, Addition),
DENG2_OBSERVES(Context, Removal),
DENG2_OBSERVES(Context, OrderChange),
public ContextWidgetOrganizer::IWidgetFactory
{
    bool needLayout;
    ListContext items;
    ContextWidgetOrganizer organizer;

    SizePolicy colPolicy;
    SizePolicy rowPolicy;

    Rule const *margin;
    //Rule const *padding;

    ConstantRule *cols;
    ConstantRule *rows;

    OperatorRule *colWidth;
    OperatorRule *rowHeight;

    Instance(Public *i)
        : Base(i),
          needLayout(false),
          organizer(self),
          colPolicy(Fixed),
          rowPolicy(Fixed)
    {
        cols = new ConstantRule(1);
        rows = new ConstantRule(1);

        margin = &self.style().rules().rule("gap");
        //padding = &self.style().rules().rule("unit");

        colWidth  = holdRef((self.rule().width() - *margin * 2) / *cols);
        rowHeight = holdRef((self.rule().height() - *margin * 2) / *rows);

        items.audienceForAddition += this;
        items.audienceForRemoval += this;
        items.audienceForOrderChange += this;

        organizer.setWidgetFactory(*this);
        organizer.setContext(items);
    }

    ~Instance()
    {
        releaseRef(cols);
        releaseRef(rows);
        releaseRef(colWidth);
        releaseRef(rowHeight);
    }

    void contextItemAdded(Context::Pos id, Item const &)
    {
        // Make sure we determine the layout for the new item.
        needLayout = true;
    }

    void contextItemBeingRemoved(Context::Pos id, Item const &item)
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
    GuiWidget *makeWidgetForItem(Item const &item, GuiWidget const *parent)
    {
        if(item.semantic() == Item::Action || item.semantic() == Item::SubmenuAction)
        {
            // Normal clickable button.
            ButtonWidget *b = new ButtonWidget;
            b->setTextAlignment(ui::AlignRight);
            return b;
        }
        else if(item.semantic() == Item::Separator)
        {
            LabelWidget *lab = new LabelWidget;
            lab->setAlignment(ui::AlignLeft);
            lab->setTextLineAlignment(ui::AlignLeft);
            lab->setSizePolicy(ui::Expand, ui::Expand);
            return lab;
        }
        else if(item.semantic() == Item::Toggle)
        {
            // We know how to present variable toggles.
            VariableToggleItem const *varTog = dynamic_cast<VariableToggleItem const *>(&item);
            if(varTog)
            {
                return new VariableToggleWidget(varTog->variable());
            }
        }
        return 0;
    }

    void updateWidgetForItem(GuiWidget &widget, Item const &item)
    {
        if(item.semantic() == Item::Action ||
           item.semantic() == Item::SubmenuAction)
        {
            ButtonWidget &b = widget.as<ButtonWidget>();
            ActionItem const &act = item.as<ActionItem>();

            b.setImage(act.image());
            b.setText(act.label());
            b.setAction(act.action()->duplicate());
        }
        else if(item.semantic() == Item::Separator)
        {
            widget.as<LabelWidget>().setText(item.label());
        }
    }

    Vector2i ordinalToGridPos(int ordinal) const
    {
        Vector2i pos;
        if(colPolicy != Expand)
        {
            pos.x = ordinal % cols->valuei();
            pos.y = ordinal / cols->valuei();
        }
        else if(rows->valuei() > 0)
        {
            DENG2_ASSERT(rowPolicy != Expand);
            pos.x = ordinal % rows->valuei();
            pos.y = ordinal / rows->valuei();
        }
        else
        {
            DENG2_ASSERT(cols->valuei() > 0);
            pos.x = ordinal % cols->valuei();
            pos.y = ordinal / cols->valuei();
        }
        return pos;
    }

    bool isVisibleItem(Widget const *child) const
    {
        GuiWidget const *widget = dynamic_cast<GuiWidget const *>(child);
        return widget && widget->isVisible();
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

    Vector2i countGrid() const
    {
        Vector2i size;
        int ord = 0;

        //DENG2_ASSERT(sortedChildren.size() == int(self.childCount()));

        foreach(Widget *i, self.childWidgets())
        {
            if(isVisibleItem(i))
            {
                size = size.max(ordinalToGridPos(ord++) + Vector2i(1, 1));
            }
        }

        return size;
    }

    /*
    // Functor for quicksort comparisons.
    struct Sorter {
        Instance &d;
        Sorter(Instance *inst) : d(*inst) {}
        bool operator () (Widget const *a, Widget const *b) const {
            DENG2_ASSERT(!d.sorting.isNull());
            return d.sorting->compareMenuItemsForSorting(*a, *b) < 0;
        }
    };
    */

#if 0
    void prepareSortedChildren()
    {
        items.sort(ListContext::Ascending);

        /*
        sortedChildren = self.Widget::children();
        if(!sorting.isNull())
        {
            qSort(sortedChildren.begin(), sortedChildren.end(), Sorter(this));
        }
        */
    }
#endif

    GuiWidget *findItem(int col, int row) const
    {
        int ord = 0;
        foreach(Widget *i, self.childWidgets())
        {
            if(isVisibleItem(i))
            {
                if(ordinalToGridPos(ord) == Vector2i(col, row))
                {
                    return static_cast<GuiWidget *>(i);
                }
                ord++;
            }
        }
        return 0;
    }

    Rule const *fullColumnWidth(int col) const
    {
        Vector2i const size = countGrid();
        Rule const *total = 0;

        for(int i = 0; i < size.y; ++i)
        {
            GuiWidget *item = findItem(col, i);
            if(!total)
            {
                total = holdRef(item->rule().width());
            }
            else
            {
                changeRef(total, OperatorRule::maximum(*total, item->rule().width()));
            }
        }
        if(!total) return new ConstantRule(0);
        return refless(total);
    }

    Rule const *fullRowHeight(int row) const
    {
        Vector2i const size = countGrid();
        Rule const *total = 0;

        for(int i = 0; i < size.x; ++i)
        {
            GuiWidget *item = findItem(i, row);
            if(!item) continue;

            if(!total)
            {
                total = holdRef(item->rule().height());
            }
            else
            {
                changeRef(total, OperatorRule::maximum(*total, item->rule().height()));
            }
        }
        if(!total) return new ConstantRule(0);
        return refless(total);
    }

    Rule const *totalWidth() const
    {
        Vector2i const size = countGrid();
        Rule const *total = 0;

        for(int i = 0; i < size.x; ++i)
        {
            if(!total)
            {
                total = holdRef(fullColumnWidth(i));
            }
            else
            {
                changeRef(total, *total + *fullColumnWidth(i));
            }
        }
        if(!total) return new ConstantRule(0);
        return refless(total);
    }

    Rule const *totalHeight() const
    {
        Vector2i const size = countGrid();
        Rule const *total = 0;

        for(int i = 0; i < size.y; ++i)
        {
            if(!total)
            {
                total = holdRef(fullRowHeight(i));
            }
            else
            {
                changeRef(total, *total + *fullRowHeight(i));
            }
        }
        if(!total) return new ConstantRule(0);
        return refless(total);
    }
};

MenuWidget::MenuWidget(String const &name)
    : ScrollAreaWidget(name), d(new Instance(this))
{}

void MenuWidget::setGridSize(int columns, ui::SizePolicy columnPolicy,
                             int rows, ui::SizePolicy rowPolicy)
{
    d->cols->set(columns);
    d->rows->set(rows);

    d->colPolicy = columnPolicy;
    d->rowPolicy = rowPolicy;

    d->needLayout = true;
}

Context &MenuWidget::items()
{
    return d->items;
}

Context const &MenuWidget::items() const
{
    return d->items;
}

/*
void MenuWidget::setLayoutSortOrder(ISortOrder *sorting)
{
    d->sorting.reset(sorting);
    d->needLayout = true;
}
*/

/*
GuiWidget *MenuWidget::addItem(GuiWidget *anyWidget)
{
    if(!anyWidget) return 0;

    add(anyWidget);
    d->needLayout = true;
    return anyWidget;
}
*/

#if 0
ButtonWidget *MenuWidget::addItem(String const &styledText, Action *action)
{
    return addItem(Image(), styledText, action);
}

ButtonWidget *MenuWidget::addItem(Image const &image, String const &styledText, Action *action)
{    
    ButtonWidget *b = new ButtonWidget;

    b->setImage(image);
    b->setText(styledText);
    b->setTextAlignment(ui::AlignRight);
    b->setAction(action);

    addItem(b);

    return b;
}

GuiWidget *MenuWidget::addSeparator(String const &labelText)
{
    LabelWidget *lab = new LabelWidget;
    lab->setText(labelText);
    lab->setAlignment(ui::AlignLeft);
    lab->setTextLineAlignment(ui::AlignLeft);
    lab->setSizePolicy(ui::Expand, ui::Expand);
    addItem(lab);
    return lab;
}

void MenuWidget::removeItem(GuiWidget *child)
{
    d->needLayout = true;
}
#endif

int MenuWidget::count() const
{
    return d->countVisible();
}

void MenuWidget::updateLayout()
{
    // Sort children again.
    //d->prepareSortedChildren();

    Rule const *baseVert = holdRef(&contentRule().top());

    Vector2i gridSize = d->countGrid();

    for(int row = 0; row < gridSize.y; ++row)
    {
        Rule const *baseHoriz = &contentRule().left();
        GuiWidget *previous = 0;
        Rule const *rowBottom = 0;

        for(int col = 0; col < gridSize.x; ++col)
        {
            GuiWidget *widget = d->findItem(col, row);
            if(!widget) continue;

            if(d->colPolicy == Filled)
            {
                widget->rule()
                        .setInput(Rule::Left, previous? previous->rule().right() : *baseHoriz)
                        .setInput(Rule::Width, *d->colWidth);
            }            
            else //if(d->colPolicy == Fixed)
            {
                if(col > 0)
                {
                    widget->rule().setInput(Rule::Left, contentRule().left() + *d->colWidth * Const(col));
                }
                else
                {
                    widget->rule().setInput(Rule::Left, contentRule().left());
                }
            }

            if(d->rowPolicy == Filled)
            {
                // The top/left corner is determined by the grid.
                widget->rule()
                        .setInput(Rule::Top, *baseVert)
                        .setInput(Rule::Height, *d->rowHeight);
            }
            else if(d->rowPolicy == Expand)
            {
                widget->rule().setInput(Rule::Top, *baseVert);
            }
            else if(d->rowPolicy == Fixed)
            {
                widget->rule().setInput(Rule::Top, contentRule().top() + *d->rowHeight * Const(row));
            }

            // Form operator rule for maximum bottom coordinate.
            if(!rowBottom)
            {
                rowBottom = holdRef(widget->rule().bottom());
            }
            else
            {
                changeRef(rowBottom, OperatorRule::maximum(widget->rule().bottom(), *rowBottom));
            }

            previous = widget;
        }

        releaseRef(baseVert);
        baseVert = rowBottom;
    }

    // Determine the width and height of the scrollable content area.
    if(d->colPolicy != Expand)
    {
        setContentWidth(*d->colWidth * Const(gridSize.x));
    }
    else
    {
        Rule const *width = d->totalWidth();
        setContentWidth(*width);
        rule().setInput(Rule::Width, *width + *d->margin * 2);
    }

    if(d->rowPolicy != Expand)
    {
        setContentHeight(*d->rowHeight * Const(gridSize.y));
    }
    else
    {
        Rule const *height = d->totalHeight();
        setContentHeight(*height);
        rule().setInput(Rule::Height, *height + *d->margin * 2);
    }

    // Hold kept references.
    releaseRef(baseVert);

    d->needLayout = false;
}

Rule const *MenuWidget::newColumnWidthRule(int column) const
{
    if(d->colPolicy != Filled)
    {
        return holdRef(d->fullColumnWidth(column));
    }
    return holdRef(d->colWidth);
}

ContextWidgetOrganizer const &MenuWidget::organizer() const
{
    return d->organizer;
}

void MenuWidget::update()
{
    if(isHidden()) return;

    ScrollAreaWidget::update();

    if(d->needLayout)
    {
        updateLayout();
    }
}
