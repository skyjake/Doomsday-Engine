/** @file variablearraywidget.cpp  Widget for editing Variables with array values.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/variablearraywidget.h"
#include <de/textvalue.h>

namespace de {

DE_GUI_PIMPL(VariableArrayWidget)
, DE_OBSERVES(Variable, Deletion)
, DE_OBSERVES(Variable, Change  )
, DE_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
{
    Variable *    var = nullptr;
    IndirectRule *maxWidth;
    MenuWidget *  menu;
    ButtonWidget *addButton;
    ButtonWidget *deleteButton;
    ui::DataPos   hoverItem      = ui::Data::InvalidPos;
    bool          mouseWasInside = false;

    /// Notifies the widget when the mouse is over one of the items.
    struct HoverHandler : public GuiWidget::IEventHandler
    {
        VariableArrayWidget &owner;

        HoverHandler(VariableArrayWidget &owner) : owner(owner) {}
        bool handleEvent(GuiWidget &widget, const Event &event) {
            if (event.isMouse() && widget.hitTest(event)) {
                owner.d->setHoverItem(widget);
            }
            return false;
        }
    };

    Impl(Public *i, Variable &var) : Base(i), var(&var)
    {
        maxWidth = new IndirectRule;
        maxWidth->setSource(rule("list.width"));

        menu         = new MenuWidget;
        addButton    = new ButtonWidget;
        deleteButton = new ButtonWidget;

        menu->organizer().audienceForWidgetCreation() += this;
        menu->organizer().audienceForWidgetUpdate()   += this;
        menu->setGridSize(1, ui::Expand, 0, ui::Expand);
        menu->layout().setRowPadding(2 * rule("unit"));

        updateFromVariable();
        var.audienceForDeletion() += this;
        var.audienceForChange()   += this;
    }

    ~Impl() override
    {
        releaseRef(maxWidth);
    }

    void setHoverItem(GuiWidget &widget)
    {
        hoverItem = menu->findItem(widget);

        deleteButton->show();
        deleteButton->rule().setMidAnchorY(widget.rule().midY());
    }

    void widgetCreatedForItem(GuiWidget &widget, const ui::Item &item) override
    {
        auto &label = widget.as<LabelWidget>();
        label.setSizePolicy(ui::Expand, ui::Expand);
        label.setMaximumTextWidth(*maxWidth);
        widget.margins().setRight("").setTopBottom("");
        widget.margins().setLeft("");//deleteButton->rule().width());
        widget.addEventHandler(new HoverHandler(self()));

        self().elementCreated(label, item);
    }

    void widgetUpdatedForItem(GuiWidget &widget, const ui::Item &item) override
    {
        widget.as<LabelWidget>().setText(item.label());
    }

    void updateFromVariable()
    {
        if (!var) return;

        menu->items().clear();

        if (const auto *array = maybeAs<ArrayValue>(var->value()))
        {
            for (const Value *value : array->elements())
            {
                menu->items() << self().makeItem(*value);
            }
        }
        else
        {
            const String value = var->value().asText();
            if (!value.isEmpty())
            {
                menu->items() << self().makeItem(var->value());
            }
        }
    }

    void setVariableFromWidget()
    {
        if (!var) return;

        var->audienceForChange() -= this;

        if (menu->items().isEmpty())
        {
            var->set(new TextValue());
        }
        else if (menu->items().size() == 1)
        {
            var->set(new TextValue(menu->items().at(0).data().asText()));
        }
        else
        {
            auto *array = new ArrayValue;
            for (ui::DataPos i = 0; i < menu->items().size(); ++i)
            {
                array->add(new TextValue(menu->items().at(i).data().asText()));
            }
            var->set(array);
        }
        var->audienceForChange() += this;
    }

    void variableValueChanged(Variable &, const Value &) override
    {
        updateFromVariable();
    }

    void variableBeingDeleted(Variable &) override
    {
        var = nullptr;
        self().disable();
    }

    DE_PIMPL_AUDIENCE(Change)
};

DE_AUDIENCE_METHOD(VariableArrayWidget, Change)

VariableArrayWidget::VariableArrayWidget(Variable &variable, const String &name)
    : GuiWidget(name)
    , d(new Impl(this, variable))
{
    d->deleteButton->setSizePolicy(ui::Expand, ui::Expand);
    d->deleteButton->setStyleImage("close.ring", "default");
    d->deleteButton->margins().setLeft(RuleBank::UNIT).setRight("dialog.gap");
    d->deleteButton->setBehavior(Focusable, UnsetFlags);
    d->deleteButton->set(Background());
    d->deleteButton->hide();

    d->menu->margins()
            .setLeft(d->deleteButton->rule().width())
            .setBottom("dialog.gap");

    d->menu->enableScrolling(false);
    d->menu->enablePageKeys(false);
    d->menu->rule()
            .setLeftTop(/*margins().left() + */rule().left(),
                        margins().top()  + rule().top())
            .setInput(Rule::Right, rule().right() - 2 * rule("gap"));

    d->addButton->setFont("small");
    d->addButton->setStyleImage("create", d->addButton->fontId());
    d->addButton->setTextAlignment(ui::AlignRight);
    d->addButton->setSizePolicy(ui::Expand, ui::Expand);

    AutoRef<Rule> totalWidth(OperatorRule::maximum(d->menu->rule().width(),
                                                   d->deleteButton->rule().width() +
                                                   d->addButton->contentWidth()));

    d->addButton->rule()
            //.setInput(Rule::Width, totalWidth)
            .setLeftTop(d->deleteButton->rule().width() + d->menu->rule().left(),
                        d->menu->rule().bottom());

    d->deleteButton->rule().setInput(Rule::Left, d->menu->rule().left());
    d->deleteButton->setActionFn([this] ()
    {
        d->deleteButton->hide();
        d->menu->items().remove(d->hoverItem);
        setVariableFromWidget();
    });

    rule().setSize(totalWidth + margins().width(),
                   d->menu->rule().height() + d->addButton->rule().height() + margins().height());

    add(d->menu);
    add(d->deleteButton);
    add(d->addButton);
    
    d->menu->updateLayout();
}

Variable &VariableArrayWidget::variable() const
{
    return *d->var;
}

MenuWidget &VariableArrayWidget::elementsMenu()
{
    return *d->menu;
}

String VariableArrayWidget::labelForElement(const Value &value) const
{
    return value.asText();
}

void VariableArrayWidget::elementCreated(LabelWidget &, const ui::Item &)
{
    // Derived class may customize the label.
}

ButtonWidget &VariableArrayWidget::addButton()
{
    return *d->addButton;
}

ButtonWidget *VariableArrayWidget::detachAddButton(const Rule &contentWidth)
{
    d->addButton->setSizePolicy(ui::Expand, ui::Expand);
    d->addButton->orphan();

    d->maxWidth->setSource(contentWidth - margins().width());
    d->menu->setGridSize(1, ui::Fixed, 0, ui::Expand);
    d->menu->rule().setInput(Rule::Width, contentWidth - margins().width());
    rule().setSize(contentWidth,
                   d->menu->rule().height() + margins().height());

    return d->addButton;
}

ui::Item *VariableArrayWidget::makeItem(const Value &value)
{
    auto *item = new ui::Item(ui::Item::ShownAsLabel, labelForElement(value));
    item->setData(value);
    return item;
}

bool VariableArrayWidget::handleEvent(const Event &event)
{
    // Hide the delete button when mouse leaves the widget's bounds.
    if (event.isMouse())
    {
        const MouseEvent &mouse = event.as<MouseEvent>();
        const bool isInside = rule().recti().contains(mouse.pos());
        if (d->mouseWasInside && !isInside)
        {
            d->deleteButton->hide();
        }
        d->mouseWasInside = isInside;
    }
    return GuiWidget::handleEvent(event);
}

void VariableArrayWidget::updateFromVariable()
{
    d->updateFromVariable();
}

void VariableArrayWidget::setVariableFromWidget()
{
    d->setVariableFromWidget();
    DE_NOTIFY(Change, i)
    {
        i->variableArrayChanged(*this);
    }
}

} // namespace de
