/** @file tabwidget.cpp  Tab widget.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/tabwidget.h"
#include "de/ui/listdata.h"
#include "de/menuwidget.h"

#include <de/animationrule.h>

namespace de {

DE_GUI_PIMPL(TabWidget)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DE_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
, DE_OBSERVES(ui::Data,             Addition)
, DE_OBSERVES(ui::Data,             OrderChange)
, DE_OBSERVES(ButtonWidget,         Press)
{
    ui::Data::Pos  current       = 0;
    MenuWidget *   buttons       = nullptr;
    bool           needUpdate    = false;
    bool           invertedStyle = false;
    LabelWidget *  selected      = nullptr;
    AnimationRule *selLeft       = nullptr;
    AnimationRule *selWidth      = nullptr;
    DotPath        tabFontId     = "tab.label";
    DotPath        selTabFontId  = "tab.selected";

    Impl(Public *i) : Base(i)
    {
        self().add(buttons = new MenuWidget);
        buttons->enableScrolling(false);
        buttons->margins().set("");
        buttons->setGridSize(0, ui::Expand, 1, ui::Expand, GridLayout::ColumnFirst);

        buttons->organizer().audienceForWidgetCreation() += this;
        buttons->organizer().audienceForWidgetUpdate()   += this;
        buttons->items().audienceForAddition()    += this;
        buttons->items().audienceForOrderChange() += this;

        // Center the buttons inside the widget.
        buttons->rule()
                .setInput(Rule::AnchorX, self().rule().left() + self().rule().width() / 2)
                .setInput(Rule::Top, self().rule().top())
                .setAnchorPoint(Vec2f(.5f, 0));

        // Selection highlight.
        self().add(selected = new LabelWidget);
    }

    ~Impl()
    {
        releaseRef(selLeft);
        releaseRef(selWidth);
    }

    void widgetCreatedForItem(GuiWidget &widget, const ui::Item &)
    {
        // Set the font and style.
        ButtonWidget &btn = widget.as<ButtonWidget>();
        btn.setSizePolicy(ui::Expand, ui::Expand);
        btn.setFont(tabFontId);
        btn.setOverrideImageSize(btn.font().height());
        btn.margins().set("dialog.gap");
        btn.set(Background());
        btn.setBehavior(Focusable, UnsetFlags);

        btn.audienceForPress() += this;
    }

    void widgetUpdatedForItem(GuiWidget &widget, const ui::Item &item)
    {
        widget.as<ButtonWidget>().setShortcutKey(item.as<TabItem>().shortcutKey());
    }

    void buttonPressed(ButtonWidget &button)
    {
        self().setCurrent(buttons->items().find(*buttons->organizer().findItemForWidget(button)));
    }

    void dataItemAdded(ui::Data::Pos, const ui::Item &)
    {
        needUpdate = true;
    }

    void dataItemOrderChanged()
    {
        needUpdate = true;
    }

    void setCurrent(ui::Data::Pos pos)
    {
        if (current != pos && pos < buttons->items().size())
        {
            current = pos;
            updateSelected();
            DE_NOTIFY_PUBLIC(Tab, i)
            {
                i->currentTabChanged(self());
            }
        }
    }

    void updateSelected()
    {
        selected->set(Background(style().colors().colorf(invertedStyle? "tab.inverted.selected"
                                                                      : "tab.selected")));
        for (ui::Data::Pos i = 0; i < buttons->items().size(); ++i)
        {
            const bool sel = (i == current);
            ButtonWidget &w = buttons->itemWidget<ButtonWidget>(buttons->items().at(i));
            w.setFont(sel ? selTabFontId : tabFontId);
            w.setOpacity(sel? 1.f : 0.7f, 0.4);
            if (!invertedStyle)
            {
                w.setTextColor(sel? "tab.selected" : "text");
                w.setHoverTextColor(sel? "tab.selected" : "text");
            }
            else
            {
                w.setTextColor(sel? "tab.inverted.selected" : "inverted.text");
                w.setHoverTextColor(sel? "tab.inverted.selected" : "inverted.text");
            }
            if (sel)
            {
                TimeSpan span = .2;
                if (!selLeft)
                {
                    // Initialize the animated rules for positioning the
                    // selection highlight.
                    selLeft  = new AnimationRule(0);
                    selWidth = new AnimationRule(0);
                    selected->rule()
                        .setInput(Rule::Width,  *selWidth)
                        .setInput(Rule::Left,   *selLeft);
                    span = 0.0;
                }
                // Animate to new position.
                selLeft ->set(w.rule().left(),  span);
                selWidth->set(w.rule().width(), span);

                selected->rule()
                    .setInput(Rule::Height, rule("halfunit"))
                    .setInput(Rule::Top,    w.rule().bottom());
            }
        }
    }

    bool handleShortcutKey(const KeyEvent &key)
    {
        for (auto *w : buttons->childWidgets())
        {
            if (ButtonWidget *but = maybeAs<ButtonWidget>(w))
            {
                if (but->handleShortcut(key))
                {
                    return true;
                }
            }
        }
        return false;
    }

    DE_PIMPL_AUDIENCE(Tab)
};

DE_AUDIENCE_METHOD(TabWidget, Tab)

TabWidget::TabWidget(const String &name)
    : GuiWidget(name), d(new Impl(this))
{
    rule().setInput(Rule::Height, d->buttons->rule().height());
}

void TabWidget::setTabFont(const DotPath &fontId, const DotPath &selectedFontId)
{
    d->tabFontId    = fontId;
    d->selTabFontId = selectedFontId;
}

void TabWidget::useInvertedStyle()
{
    d->invertedStyle = true;
}

void TabWidget::clearItems()
{
    if (d->selLeft)
    {
        // Dependent on the heading widget rules.
        d->selLeft->set(d->selLeft->value());
    }
    items().clear();
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
    DE_ASSERT(d->current < items().size());
    return items().at(d->current).as<TabItem>();
}

void TabWidget::setCurrent(ui::Data::Pos itemPos)
{
    d->setCurrent(itemPos);
}

void TabWidget::update()
{
    GuiWidget::update();

    // Show or hide the selection highlight when enabled/disabled.
    if (isEnabled() && fequal(d->selected->opacity().target(), 0.0f))
    {
        d->selected->setOpacity(1.0f, 300_ms);
    }
    else if (isDisabled() && !fequal(d->selected->opacity().target(), 0.0f))
    {
        d->selected->setOpacity(0.0f, 300_ms);
    }

    if (d->needUpdate)
    {
        d->updateSelected();
        d->needUpdate = false;
    }
}

bool TabWidget::handleEvent(const Event &ev)
{
    if (isEnabled())
    {
        if (ev.isKeyDown())
        {
            const KeyEvent &key = ev.as<KeyEvent>();
            if (d->handleShortcutKey(key))
            {
                return true;
            }
        }
    }
    return GuiWidget::handleEvent(ev);
}

} // namespace de
