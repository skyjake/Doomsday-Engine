/** @file widgets/menuwidget.cpp
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

#include "de/menuwidget.h"

#include "de/childwidgetorganizer.h"
#include "de/focuswidget.h"
#include "de/gridlayout.h"
#include "de/popupbuttonwidget.h"
#include "de/popupmenuwidget.h"
#include "de/styleproceduralimage.h"
#include "de/variabletogglewidget.h"
#include "de/ui/actionitem.h"
#include "de/ui/listdata.h"
#include "de/ui/subwidgetitem.h"
#include "de/ui/variantactionitem.h"

namespace de {

using namespace ui;

DE_PIMPL(MenuWidget)
, DE_OBSERVES(Data, Addition)        // for layout update
, DE_OBSERVES(Data, Removal)         // for layout update
, DE_OBSERVES(Data, OrderChange)     // for layout update
, DE_OBSERVES(Widget, ChildAddition) // for layout update
, DE_OBSERVES(Widget, ChildRemoval)  // for layout update
, DE_OBSERVES(PopupWidget, Close)
, DE_OBSERVES(Widget, Deletion)
, DE_OBSERVES(ButtonWidget, Press) // child button triggers
, public ChildWidgetOrganizer::IWidgetFactory
{
    /**
     * Base class for sub-widget actions. Handles ownership/openness tracking.
     */
    class SubAction : public de::Action
    {
    public:
        SubAction(MenuWidget::Impl *inst, const ui::Item &parentItem)
            : d(inst)
            , _parentItem(parentItem)
            , _dir(ui::Right)
        {}

        void setWidget(PopupWidget *w, ui::Direction openingDirection)
        {
            _widget.reset(w);

            // Popups need a parent.
            //d->self().add(_widget);

            _dir = openingDirection;
        }

        bool isTriggered() const
        {
            return bool(_widget);
        }

        GuiWidget &parent() const
        {
            auto *p = d->organizer.itemWidget(_parentItem);
            DE_ASSERT(p != nullptr);
            return *p;
        }

        void trigger()
        {
            DE_ASSERT(_widget);
            DE_ASSERT(d->self().hasRoot());

            if (_widget->isOpeningOrClosing()) return;

            if (!_widget->parentWidget())
            {
                d->self().root().add(_widget);
            }

            Action::trigger();

            if (auto *subMenu = maybeAs<PopupMenuWidget>(_widget))
            {
                // Parent is the anchor button, owned by a MenuWidget, possibly owned a
                // the popup menu.
                if (auto *parentMenu = parent().parentWidget())
                {
                    subMenu->setParentPopup(maybeAs<PopupWidget>(parentMenu->parentWidget()));
                }
            }
            _widget->setAnchorAndOpeningDirection(parent().hitRule(), _dir);

            d->keepTrackOfSubWidget(_widget);
            _widget->open();
        }

    protected:
        MenuWidget::Impl *d;
        const ui::Item &_parentItem;
        ui::Direction _dir;
        SafeWidgetPtr<PopupWidget> _widget;
    };

    /**
     * Action owned by the button that represents a SubmenuItem.
     */
    class SubmenuAction : public SubAction
    {
    public:
        SubmenuAction(MenuWidget::Impl *inst, const ui::SubmenuItem &parentItem)
            : SubAction(inst, parentItem)
        {
            _sub.reset(new PopupMenuWidget);
            setWidget(_sub, parentItem.openingDirection());

            // Use the items from the submenu.
            _sub->menu().setItems(parentItem.items());
        }

        ~SubmenuAction()
        {
            GuiWidget::destroy(_sub);
        }

    private:
        SafeWidgetPtr<PopupMenuWidget> _sub; // owned
    };

    /**
     * Action owned by the button that represents a SubwidgetItem.
     */
    class SubwidgetAction : public SubAction, DE_OBSERVES(PanelWidget, Close)
    {
    public:
        SubwidgetAction(MenuWidget::Impl *inst, const ui::SubwidgetItem &parentItem)
            : SubAction(inst, parentItem)
            , _item(parentItem)
        {}

        void trigger() override
        {
            if (isTriggered()) return; // Already open, cannot retrigger.

            // The widget is created only at this point, when the action is triggered.
            setWidget(_item.makeWidget(), _item.openingDirection());
            _widget->setDeleteAfterDismissed(true);

            if (_item.semantics().testFlag(Item::ClosesParentPopup))
            {
                _widget->audienceForClose() += this;
            }

            SubAction::trigger();
        }

        void panelBeingClosed(PanelWidget &) override
        {
            if (auto *selfPopup = maybeAs<PopupMenuWidget>(d->self().parentGuiWidget()))
            {
                selfPopup->close();
            }
        }

    private:
        const ui::SubwidgetItem &_item;
    };

    AssetGroup assets;
    bool needLayout = false;
    bool variantsEnabled = false;
    GridLayout layout;
    ListData defaultItems;
    const Data *items = nullptr;
    ChildWidgetOrganizer organizer;
    Set<PanelWidget *> openSubs;
    IndirectRule *outContentHeight;

    SizePolicy colPolicy = Fixed;
    SizePolicy rowPolicy = Fixed;

    Impl(Public *i)
        : Base(i)
        , organizer(*i)
    {
        outContentHeight = new IndirectRule;

        // We will create widgets ourselves.
        organizer.setWidgetFactory(*this);

        // The default context is empty.
        setContext(&defaultItems);

        self().audienceForChildAddition() += this;
        self().audienceForChildRemoval()  += this;
    }

    ~Impl()
    {
        releaseRef(outContentHeight);

        // Clear the data model first, so possible sub-widgets are deleted at the right time.
        // Note that we can't clear an external data model.
        defaultItems.clear();
    }

    void setContext(const Data *ctx)
    {
        if (items)
        {
            // Get rid of the old context.
            items->audienceForAddition()    -= this;
            items->audienceForRemoval()     -= this;
            items->audienceForOrderChange() -= this;
            organizer.unsetContext();
        }

        items = ctx;

        // Take new context into use.
        items->audienceForAddition()    += this;
        items->audienceForRemoval()     += this;
        items->audienceForOrderChange() += this;
        organizer.setContext(*items); // recreates widgets
    }

    void dataItemAdded(Data::Pos, const Item &)
    {
        // Make sure we determine the layout for the new item.
        needLayout = true;
    }

    void dataItemRemoved(Data::Pos, Item &)
    {
        // Make sure we determine the layout after this item is gone.
        needLayout = true;
    }

    void dataItemOrderChanged()
    {
        // Make sure we determine the layout for the new order.
        needLayout = true;
    }

    void widgetChildAdded(Widget &child)
    {
        // Make sure we redo the layout with the new child. This occurs
        // when filtered items are accepted again as widgets.
        needLayout = true;

        if (IAssetGroup *asset = maybeAs<IAssetGroup>(child))
        {
            assets += *asset; // part of the asset group (observes for deletion)
        }
        if (ButtonWidget *button = maybeAs<ButtonWidget>(child))
        {
            button->audienceForPress() += this;
        }
    }

    void widgetChildRemoved(Widget &child)
    {
        // Make sure we redo the layout without this child. This occurs
        // when filtered items are removed from the menu.
        needLayout = true;

        if (IAssetGroup *asset = maybeAs<IAssetGroup>(child))
        {
            assets -= *asset; // no longer part of the asset group
        }
        if (ButtonWidget *button = maybeAs<ButtonWidget>(child))
        {
            button->audienceForPress() -= this;
        }
    }

    static void setFoldIndicatorForDirection(LabelWidget &label, ui::Direction dir)
    {
        if (dir == ui::Right || dir == ui::Left)
        {
            label.setImage(new StyleProceduralImage("fold", label, dir == ui::Right? -90 : 90));
            label.setTextAlignment(dir == ui::Right? ui::AlignLeft : ui::AlignRight);
        }
    }

    /*
     * Menu items are represented as buttons and labels.
     */
    GuiWidget *makeItemWidget(const Item &item, const GuiWidget *)
    {
        if (item.semantics().testFlag(Item::ShownAsButton))
        {
            // Normal clickable button.
            ButtonWidget *b = (item.semantics().testFlag(Item::ShownAsPopupButton)?
                                   new PopupButtonWidget : new ButtonWidget);
            b->setTextAlignment(ui::AlignRight);
            if (is<SubmenuItem>(item))
            {
                const auto &subItem = item.as<SubmenuItem>();
                b->setAction(new SubmenuAction(this, subItem));
                setFoldIndicatorForDirection(*b, subItem.openingDirection());
            }
            else if (is<SubwidgetItem>(item))
            {
                const auto &subItem = item.as<SubwidgetItem>();
                b->setAction(new SubwidgetAction(this, subItem));
                setFoldIndicatorForDirection(*b, subItem.openingDirection());
                if (subItem.image().isNull())
                {
                    setFoldIndicatorForDirection(*b, subItem.openingDirection());
                }
            }
            return b;
        }
        else if (item.semantics().testFlag(Item::Separator))
        {
            LabelWidget *lab = new LabelWidget;
            lab->setAlignment(ui::AlignLeft);
            lab->setTextLineAlignment(ui::AlignLeft);
            lab->setSizePolicy(ui::Expand, ui::Expand);
            return lab;
        }
        else if (item.semantics().testFlag(Item::ShownAsLabel))
        {
            LabelWidget *lab = new LabelWidget;
            lab->setTextAlignment(ui::AlignRight);
            lab->setTextLineAlignment(ui::AlignLeft);
            lab->setSizePolicy(ui::Expand, ui::Expand);
            return lab;
        }
        else if (item.semantics().testFlag(Item::ShownAsToggle))
        {
            // We know how to present variable toggles.
            if (const VariableToggleItem *varTog = maybeAs<VariableToggleItem>(item))
            {
                return new VariableToggleWidget(varTog->variable());
            }
            else
            {
                // A regular toggle.
                return new ToggleWidget;
            }
        }
        return nullptr;
    }

    void updateItemWidget(GuiWidget &widget, const Item &item)
    {
        // Image items apply their image to all label-based widgets.
        if (const ImageItem *img = maybeAs<ImageItem>(item))
        {
            if (LabelWidget *label = maybeAs<LabelWidget>(widget))
            {
                if (!img->image().isNull())
                {
                    label->setImage(img->image());
                }
            }
        }

        if (const ActionItem *act = maybeAs<ActionItem>(item))
        {
            if (item.semantics().testFlag(Item::ShownAsButton))
            {
                ButtonWidget &b = widget.as<ButtonWidget>();
                b.setText(act->label());
                if (act->action())
                {
                    b.setAction(*act->action());
                }
            }
            else if (item.semantics().testFlag(Item::ShownAsLabel))
            {
                widget.as<LabelWidget>().setText(item.label());
            }
            else if (item.semantics().testFlag(Item::ShownAsToggle))
            {
                ToggleWidget &t = widget.as<ToggleWidget>();
                t.setText(act->label());
                if (act->action())
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
        }
    }

    void panelBeingClosed(PanelWidget &popup)
    {
        popup.audienceForClose()    -= this;
        popup.audienceForDeletion() -= this;
        openSubs.remove(&popup);
    }

    void widgetBeingDeleted(Widget &widget)
    {
        openSubs.remove(static_cast<PanelWidget *>(&widget));
    }

    void keepTrackOfSubWidget(PanelWidget *w)
    {
        DE_ASSERT(is<PanelWidget>(w));

        openSubs.insert(w);

        w->audienceForClose()    += this;
        w->audienceForDeletion() += this;

        DE_NOTIFY_PUBLIC(SubWidgetOpened, i)
        {
            i->subWidgetOpened(self(), w);
        }

        // Automatically close other subwidgets when one is opened.
        for (auto *panel : openSubs)
        {
            if (panel != w) panel->close();
        }
    }

    bool isVisibleItem(const GuiWidget *child) const
    {
        if (child)
        {
            return !child->behavior().testFlag(Widget::Hidden);
        }
        return false;
    }

    int countVisible() const
    {
        int num = 0;
        for (GuiWidget *i : self().childWidgets())
        {
            if (isVisibleItem(i)) ++num;
        }
        return num;
    }

    void relayout()
    {
        layout.clear();

        if (organizer.virtualizationEnabled())
        {
            layout.setLeftTop(self().contentRule().left(),
                              self().contentRule().top() + organizer.virtualStrut());
        }

        for (GuiWidget *child : self().childWidgets())
        {
            if (isVisibleItem(child))
            {
                layout << *child;
            }
        }
    }

    const Rule &contentHeight() const
    {
        if (organizer.virtualizationEnabled())
        {
            return OperatorRule::maximum(organizer.estimatedTotalHeight(),
                                         organizer.virtualStrut() + layout.height());
        }
        return layout.height();
    }

    void buttonPressed(ButtonWidget &button)
    {
        DE_NOTIFY_PUBLIC(ItemTriggered, i)
        {
            if (auto *item = organizer.findItemForWidget(button))
            {
                i->menuItemTriggered(*item);
            }
        }
    }

    DE_PIMPL_AUDIENCES(ItemTriggered, SubWidgetOpened)
};

DE_AUDIENCE_METHODS(MenuWidget, ItemTriggered, SubWidgetOpened)

MenuWidget::MenuWidget(const String &name)
    : ScrollAreaWidget(name), d(new Impl(this))
{
    setBehavior(ChildVisibilityClipping, UnsetFlags);
}

AssetGroup &MenuWidget::assets()
{
    return d->assets;
}

void MenuWidget::setGridSize(int columns, ui::SizePolicy columnPolicy,
                             int rows, ui::SizePolicy rowPolicy,
                             GridLayout::Mode layoutMode)
{
    d->layout.clear();
    d->layout.setModeAndGridSize(layoutMode, columns, rows);
    d->layout.setLeftTop(contentRule().left(), contentRule().top());

    d->colPolicy = columnPolicy;
    d->rowPolicy = rowPolicy;

    if (d->colPolicy == ui::Filled)
    {
        DE_ASSERT(columns > 0);
        d->layout.setOverrideWidth((rule().width() - margins().width() -
                                    (columns - 1) * d->layout.columnPadding()) / float(columns));
    }

    if (d->rowPolicy == ui::Filled)
    {
        DE_ASSERT(rows > 0);
        d->layout.setOverrideHeight((rule().height() - margins().height() -
                                     (rows - 1) * d->layout.rowPadding()) / float(rows));
    }

    d->needLayout = true;
}

Data &MenuWidget::items()
{
    return *const_cast<Data *>(d->items);
}

const Data &MenuWidget::items() const
{
    return *d->items;
}

void MenuWidget::setItems(const Data &items)
{
    d->setContext(&items);
}

void MenuWidget::useDefaultItems()
{
    d->setContext(&d->defaultItems);
}

bool MenuWidget::isUsingDefaultItems() const
{
    return d->items == &d->defaultItems;
}

int MenuWidget::count() const
{
    return d->countVisible();
}

bool MenuWidget::isWidgetPartOfMenu(const GuiWidget &widget) const
{
    if (widget.parentWidget() != this) return false;
    return d->isVisibleItem(&widget);
}

void MenuWidget::updateLayout()
{
    d->relayout();
    d->outContentHeight->setSource(d->contentHeight());

    setContentSize(d->layout.width(), *d->outContentHeight);

    // Expanding policy causes the size of the menu widget to change.
    if (d->colPolicy == Expand)
    {
        rule().setInput(Rule::Width, d->layout.width() + margins().width());
    }
    if (d->rowPolicy == Expand)
    {
        rule().setInput(Rule::Height, *d->outContentHeight + margins().height());
    }

    d->needLayout = false;
}

const GridLayout &MenuWidget::layout() const
{
    return d->layout;
}

GridLayout &MenuWidget::layout()
{
    return d->layout;
}

const Rule &MenuWidget::contentHeight() const
{
    return *d->outContentHeight;
}

void MenuWidget::offerFocus()
{
    for (GuiWidget *widget : childWidgets())
    {
        if (!widget->behavior().testFlag(Hidden) && widget->behavior().testFlag(Focusable))
        {
            root().setFocus(widget);
            return;
        }
    }
}

ChildWidgetOrganizer &MenuWidget::organizer()
{
    return d->organizer;
}

const ChildWidgetOrganizer &MenuWidget::organizer() const
{
    return d->organizer;
}

void MenuWidget::setVirtualizationEnabled(bool enabled, int averageItemHeight)
{
    d->organizer.setVirtualizationEnabled(enabled);
    d->organizer.setAverageChildHeight(averageItemHeight);
    d->organizer.setVirtualTopEdge(contentRule().top());
    d->organizer.setVisibleArea(rule().top(), rule().bottom());
    d->needLayout = true;
}

void MenuWidget::setVariantItemsEnabled(bool variantsEnabled)
{
    if (d->variantsEnabled != variantsEnabled)
    {
        d->variantsEnabled = variantsEnabled;

        items().forAll([] (const ui::Item &item)
        {
            if (is<ui::VariantActionItem>(item))
            {
                item.notifyChange();
            }
            return LoopContinue;
        });
    }
}

bool MenuWidget::variantItemsEnabled() const
{
    return d->variantsEnabled;
}

ui::DataPos MenuWidget::findItem(const GuiWidget &widget) const
{
    if (const auto *item = organizer().findItemForWidget(widget))
    {
        return items().find(*item);
    }
    return ui::Data::InvalidPos;
}

void MenuWidget::update()
{
    if (d->organizer.virtualizationEnabled())
    {
        d->organizer.updateVirtualization();
    }

    if (d->needLayout)
    {
        updateLayout();
    }

    ScrollAreaWidget::update();
}

bool MenuWidget::handleEvent(const Event &event)
{
    // If a menu item has focus, arrow keys can be used to move the focus.
    if (event.isKeyDown() && root().focus() && root().focus()->parentWidget() == this)
    {
        const KeyEvent &key = event.as<KeyEvent>();
        if (key.ddKey() == DDKEY_UPARROW || key.ddKey() == DDKEY_DOWNARROW)
        {
            root().focusIndicator().fadeIn();

            const auto children = childWidgets();

            for (int ordinal = children.indexOf(root().focus());
                 ordinal >= 0 && ordinal < int(childCount());
                 ordinal += (key.ddKey() == DDKEY_UPARROW? -1 : +1))
            {
                auto *child = children.at(ordinal);
                if (child->hasFocus() || child->isDisabled()) continue;
                if (child->isVisible() && child->behavior().testFlag(Focusable))
                {
                    root().setFocus(child);
                    findTopmostScrollable().scrollToWidget(*child);
                    return true;
                }
            }
        }
    }

    return ScrollAreaWidget::handleEvent(event);
}

void MenuWidget::dismissPopups()
{
    for (PanelWidget *pop : d->openSubs)
    {
        pop->close();
    }
}

void MenuWidget::updateStyle()
{
    ScrollAreaWidget::updateStyle();
    updateLayout();
}

} // namespace de
