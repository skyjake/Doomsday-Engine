/** @file widgets/choicewidget.cpp  Widget for choosing from a set of alternatives.
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

#include "de/choicewidget.h"
#include "de/popupmenuwidget.h"
#include "de/textvalue.h"
#include "de/numbervalue.h"

namespace de {

using namespace ui;

DE_GUI_PIMPL(ChoiceWidget)
, DE_OBSERVES(Data, Addition)
, DE_OBSERVES(Data, Removal)
, DE_OBSERVES(Data, OrderChange)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
{
    /**
     * Items in the choice's popup uses this as action to change the selected
     * item.
     */
    struct SelectAction : public de::Action
    {
        ChoiceWidget::Impl *wd;
        const ui::Item &selItem;

        SelectAction(ChoiceWidget::Impl *inst, const ui::Item &item)
            : wd(inst), selItem(item) {}

        void trigger()
        {
            Action::trigger();
            wd->selected = wd->items().find(selItem);
            wd->updateButtonWithSelection();
            wd->updateItemHighlight();
            wd->choices->dismiss();

            DE_FOR_OBSERVERS(i, wd->self().audienceForUserSelection())
            {
                i->selectionChangedByUser(wd->self(), wd->selected);
            }
        }
    };

    PopupMenuWidget *choices;
    IndirectRule *   maxWidth;
    Data::Pos        selected; ///< One item is always selected.
    String           noSelectionHint;

    Impl(Public *i) : Base(i), selected(Data::InvalidPos)
    {
        maxWidth = new IndirectRule;

        self().setMaximumTextWidth(rule("choice.item.width.max"));
        self().setTextLineAlignment(ui::AlignLeft);
        self().setFont("choice.selected");

        choices = new PopupMenuWidget;
        choices->items().audienceForAddition() += this;
        choices->items().audienceForRemoval() += this;
        choices->items().audienceForOrderChange() += this;
        choices->menu().organizer().audienceForWidgetCreation() += this;
        choices->menu().organizer().audienceForWidgetUpdate() += this;
        self().add(choices);

        self().setPopup(*choices, ui::Right);

        choices->audienceForOpen() += [this]()
        {
            // Focus the selected item when the popup menu is opened.
            if (auto *w = choices->menu().itemWidget<GuiWidget>(selected))
            {
                root().setFocus(w);
            }
        };
    }

    ~Impl()
    {
        releaseRef(maxWidth);
    }

    void updateStyle()
    {
        // Popup background color.
        choices->set(choices->background().withSolidFill(style().colors().colorf("choice.popup")));
    }

    void widgetCreatedForItem(GuiWidget &widget, const ui::Item &item)
    {
        if (auto *label = maybeAs<LabelWidget>(widget))
        {
            label->setMaximumTextWidth(rule("choice.item.width.max"));
        }
        if (auto *but = maybeAs<ButtonWidget>(widget))
        {
            // Make sure the created buttons have an action that updates the
            // selected item.
            but->setAction(new SelectAction(this, item));
        }
    }

    void widgetUpdatedForItem(GuiWidget &, const ui::Item &item)
    {
        if (isValidSelection() && &item == &self().selectedItem())
        {
            // Make sure the button is up to date, too.
            updateButtonWithItem(self().selectedItem());
        }
    }

    void updateMaximumWidth()
    {
        // We'll need to calculate this manually because the fonts keep changing due to
        // selection and thus we can't just check the current layout.
        const Font &font = self().font();
        int widest = 0;
        for (uint i = 0; i < items().size(); ++i)
        {
            EscapeParser esc;
            esc.parse(items().at(i).label());
            widest = de::max(widest, font.advanceWidth(esc.plainText()));
        }
        maxWidth->setSource(OperatorRule::minimum(rule("choice.item.width.max"),
                                                  Const(widest) + self().margins().width()));
    }

    const Data &items() const
    {
        return choices->items();
    }

    bool isValidSelection() const
    {
        return selected < items().size();
    }

    void dataItemAdded(Data::Pos id, const ui::Item &)
    {
        updateMaximumWidth();

        if (selected >= items().size())
        {
            // If the previous selection was invalid, make a valid one now.
            selected = 0;

            updateButtonWithSelection();
            return;
        }

        if (id <= selected)
        {
            // New item added before/at the selection.
            selected++;
        }
    }

    void dataItemRemoved(Data::Pos id, ui::Item &)
    {
        if (id <= selected && selected > 0)
        {
            selected--;
        }

        updateButtonWithSelection();
        updateMaximumWidth();
    }

    void dataItemOrderChanged()
    {
        updateButtonWithSelection();
    }

    void updateItemHighlight()
    {
        // Highlight the currently selected item.
        for (Data::Pos i = 0; i < items().size(); ++i)
        {
            if (GuiWidget *w = choices->menu().organizer().itemWidget(i))
            {
                w->setFont(i == selected? "choice.selected" : "default");
            }
        }
    }

    void updateButtonWithItem(const ui::Item &item)
    {
        self().setText(item.label());

        const ActionItem *act = dynamic_cast<const ActionItem *>(&item);
        if (act)
        {
            self().setImage(act->image());
        }
    }

    void updateButtonWithSelection()
    {
        // Update the main button.
        if (isValidSelection())
        {
            updateButtonWithItem(items().at(selected));
        }
        else
        {
            // No valid selection.
            self().setText(noSelectionHint);
            self().setImage(Image());
        }

        DE_NOTIFY_PUBLIC(Selection, i)
        {
            i->selectionChanged(self(), selected);
        }
    }

    DE_PIMPL_AUDIENCES(Selection, UserSelection)
};

DE_AUDIENCE_METHODS(ChoiceWidget, Selection, UserSelection)

ChoiceWidget::ChoiceWidget(const String &name)
    : PopupButtonWidget(name), d(new Impl(this))
{
    d->updateButtonWithSelection();
    d->updateStyle();
    setOpeningDirection(ui::Right);
    d->choices->setAllowDirectionFlip(false);
}

PopupMenuWidget &ChoiceWidget::popup()
{
    return *d->choices;
}

void ChoiceWidget::setSelected(Data::Pos pos)
{
    d->selected = pos;
    d->updateButtonWithSelection();
    d->updateItemHighlight();
}

bool ChoiceWidget::isValidSelection() const
{
    return d->isValidSelection();
}

Data::Pos ChoiceWidget::selected() const
{
    return d->selected;
}

const Item &ChoiceWidget::selectedItem() const
{
    DE_ASSERT(d->isValidSelection());
    return d->items().at(d->selected);
}

const Rule &ChoiceWidget::maximumWidth() const
{
    return *d->maxWidth;
}

void ChoiceWidget::openPopup()
{
    d->updateItemHighlight();
    d->choices->open();
}

ui::Data &ChoiceWidget::items()
{
    return d->choices->items();
}

void ChoiceWidget::setItems(const Data &items)
{
    popup().menu().setItems(items);
    d->updateMaximumWidth();
}

void ChoiceWidget::setNoSelectionHint(const String &hint)
{
    d->noSelectionHint = hint;
}

void ChoiceWidget::useDefaultItems()
{
    popup().menu().useDefaultItems();
    d->updateMaximumWidth();
}

ChoiceWidget::Item::Item(const String &label, const String &userText, const Image &image)
    : ui::ActionItem(image, label)
{
    setData(TextValue(userText));
}

ChoiceWidget::Item::Item(const String &label, dint userNumber, const Image &image)
    : ui::ActionItem(image, label)
{
    setData(NumberValue(userNumber));
}

ChoiceWidget::Item::Item(const String &label, ddouble userNumber, const Image &image)
    : ui::ActionItem(image, label)
{
    setData(NumberValue(userNumber));
}

} // namespace de
