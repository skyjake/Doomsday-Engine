/** @file tabwidget.cpp  Tab widget.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/TabWidget"
#include "de/ui/ListData"
#include "de/MenuWidget"

Q_DECLARE_METATYPE(de::ui::Item const *)

namespace de {

DENG2_PIMPL(TabWidget)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DENG2_OBSERVES(ui::Data,             Addition)
, DENG2_OBSERVES(ui::Data,             OrderChange)
, DENG2_OBSERVES(ButtonWidget,         Press)
{
    ui::Data::Pos current;
    MenuWidget *buttons;
    bool needUpdate;
    bool invertedStyle;

    Instance(Public *i)
        : Base(i)
        , current(0)
        , needUpdate(false)
        , invertedStyle(false)
    {
        self.add(buttons = new MenuWidget);
        buttons->enableScrolling(false);
        buttons->margins().set("");
        buttons->setGridSize(0, ui::Expand, 1, ui::Expand, GridLayout::ColumnFirst);

        buttons->organizer().audienceForWidgetCreation() += this;
        buttons->items().audienceForAddition() += this;
        buttons->items().audienceForOrderChange() += this;

        // Center the buttons inside the widget.
        buttons->rule()
                .setInput(Rule::AnchorX, self.rule().left() + self.rule().width() / 2)
                .setInput(Rule::Top, self.rule().top())
                .setAnchorPoint(Vector2f(.5f, 0));
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &)
    {
        // Set the font and style.
        ButtonWidget &btn = widget.as<ButtonWidget>();
        btn.setSizePolicy(ui::Expand, ui::Expand);
        btn.setFont("tab.label");
        btn.margins().set("dialog.gap");

        if(invertedStyle)
        {
            btn.useInfoStyle();
        }

        btn.audienceForPress() += this;
    }

    void buttonPressed(ButtonWidget &button)
    {        
        self.setCurrent(buttons->items().find(*buttons->organizer().findItemForWidget(button)));
    }

    void dataItemAdded(ui::Data::Pos, ui::Item const &)
    {
        needUpdate = true;
    }

    void dataItemOrderChanged()
    {
        needUpdate = true;
    }

    void setCurrent(ui::Data::Pos pos)
    {
        if(current != pos && pos < buttons->items().size())
        {
            current = pos;
            updateSelected();
            emit self.currentTabChanged();
        }
    }

    void updateSelected()
    {
        for(ui::Data::Pos i = 0; i < buttons->items().size(); ++i)
        {
            bool const sel = (i == current);
            ButtonWidget &w = buttons->itemWidget<ButtonWidget>(buttons->items().at(i));
            w.setFont(sel? "tab.selected" : "tab.label");
            if(!invertedStyle)
            {
                w.setTextColor(sel? "tab.selected" : "text");
                w.setHoverTextColor(sel? "tab.selected" : "text");
            }
            else
            {
                w.setTextColor(sel? "tab.inverted.selected" : "inverted.text");
                w.setHoverTextColor(sel? "tab.inverted.selected" : "inverted.text");
            }
        }
    }
};

TabWidget::TabWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    rule().setInput(Rule::Height, d->buttons->rule().height());
}

void TabWidget::useInvertedStyle()
{
    d->invertedStyle = true;
    foreach(Widget *w, d->buttons->childWidgets())
    {
        // Restyle each existing button.
        w->as<ButtonWidget>().useInfoStyle();
    }
}

ui::Data &TabWidget::items()
{
    return d->buttons->items();
}

ui::Data::Pos TabWidget::current() const
{
    return d->current;
}

TabItem &TabWidget::currentItem()
{
    DENG2_ASSERT(d->current < items().size());
    return items().at(d->current).as<TabItem>();
}

void TabWidget::setCurrent(ui::Data::Pos itemPos)
{
    d->setCurrent(itemPos);
}

void TabWidget::update()
{
    GuiWidget::update();

    if(d->needUpdate)
    {
        d->updateSelected();
        d->needUpdate = false;
    }
}

} // namespace de
