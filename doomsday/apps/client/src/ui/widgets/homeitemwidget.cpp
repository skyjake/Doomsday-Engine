/** @file homeitemwidget.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/homeitemwidget.h"
#include "ui/widgets/homemenuwidget.h"
#include "ui/home/columnwidget.h"

#include <doomsday/res/lumpcatalog.h>
#include <doomsday/game.h>
#include <de/sequentiallayout.h>
#include <de/timer.h>

using namespace de;

DE_GUI_PIMPL(HomeItemWidget)
, DE_OBSERVES(MenuWidget, ItemTriggered)
{
    // Event handler for mouse clicks on the item.
    struct ClickHandler : public GuiWidget::IEventHandler
    {
        Public &owner;

        ClickHandler(Public &owner)
            : owner(owner) {}

        void acquireFocus()
        {
            owner.acquireFocus();
        }

        bool handleEvent(GuiWidget &widget, const Event &event)
        {
            if (widget.isDisabled()) return false;

            if (event.type() == Event::MouseButton)
            {
                const MouseEvent &mouse = event.as<MouseEvent>();
                if (owner.hitTest(event))
                {
                    if (mouse.button() == MouseEvent::Right)
                    {
                        switch (widget.handleMouseClick(event, MouseEvent::Right))
                        {
                        case MouseClickStarted:
                            acquireFocus();
                            return true;

                        case MouseClickAborted:
                            return true;

                        case MouseClickFinished:
                            owner.itemRightClicked();
                            DE_FOR_OBSERVERS(i, owner.audienceForContextMenu())
                            {
                                i->openItemContextMenu(owner);
                            }
                            return true;

                        default:
                            return false; // Ignore.
                        }
                    }

                    if (mouse.state() == MouseEvent::Pressed ||
                        mouse.state() == MouseEvent::DoubleClick)
                    {
                        acquireFocus();
                    }
                    if (mouse.state()  == MouseEvent::DoubleClick &&
                        mouse.button() == MouseEvent::Left)
                    {
                        DE_FOR_OBSERVERS(i, owner.audienceForDoubleClick())
                        {
                            i->itemCoubleClicked(owner);
                        }
                        return true;
                    }
                }
            }
            return false;
        }
    };

    Flags             flags;
    AssetGroup        assets;
    LabelWidget *     background;
    LabelWidget *     icon{nullptr};
    LabelWidget *     label;
    List<GuiWidget *> buttons;
    AnimationRule *   labelRightMargin;
    IndirectRule *    labelMinRightMargin = new IndirectRule;
    const Rule *      buttonsWidth        = nullptr;
    bool              selected            = false;
    bool              keepButtonsVisible  = false;
    bool              buttonsShown        = false;
    DotPath           bgColor{"transparent"};
    DotPath           selectedBgColor{"background"};
    DotPath           textColor{"text"};
    DotPath           selectedTextColor{"text"};
    Timer             buttonHideTimer;

    Impl(Public *i, Flags flags) : Base(i), flags(flags)
    {
        labelRightMargin = new AnimationRule(0);

        self().add(background = new LabelWidget);
        if (!(flags & WithoutIcon))
        {
            self().add(icon = new LabelWidget);
        }
        self().add(label = new LabelWidget);

        // Observe state of the labels.
        assets += *background;
        assets += *label;

        if (icon)
        {
            assets += *icon;

            icon->setBehavior(ContentClipping);
            icon->setImageFit(ui::CoverArea | ui::OriginalAspectRatio);
            icon->setSizePolicy(ui::Filled, ui::Filled);
            icon->margins().setZero();
        }

        label->setSizePolicy(ui::Filled, ui::Expand);
        label->setTextLineAlignment(ui::AlignLeft);
        label->setAlignment(ui::AlignLeft);
        label->setBehavior(ChildVisibilityClipping);

        //background->setBehavior(Focusable);

        buttonHideTimer.setSingleShot(true);
        buttonHideTimer += [this] ()
        {
            for (auto *button : buttons)
            {
                button->setAttribute(DontDrawContent);
            }
        };
    }

    ~Impl() override
    {
        releaseRef(labelRightMargin);
        releaseRef(labelMinRightMargin);
        releaseRef(buttonsWidth);
    }

    void updateButtonLayout()
    {
        SequentialLayout layout(label->rule().right() - *labelRightMargin,
                                label->rule().top(), ui::Right);
        for (auto *button : buttons)
        {
            if (!button->behavior().testFlag(Hidden))
            {
                layout << *button;
            }
            button->rule().setMidAnchorY(label->rule().midY());
        }
        changeRef(buttonsWidth, layout.width() + rule("gap"));

        if (buttonsShown)
        {
            labelRightMargin->set(*buttonsWidth,
                                  labelRightMargin->animation().done()? 400_ms :
                                  labelRightMargin->animation().remainingTime());
        }
    }

    void showButtons(bool show)
    {
        buttonsShown = show;
        if (!buttonsWidth) return;

        if (show)
        {
            buttonHideTimer.stop();
            for (auto *button : buttons)
            {
                button->setAttribute(DontDrawContent, false);
                button->setBehavior(Focusable);
            }
        }
        else
        {
            for (auto *button : buttons)
            {
                button->setBehavior(Focusable, false);
            }
        }

        const TimeSpan SPAN = (self().hasBeenUpdated()? 0.4 : 0.0);
        if (show)
        {
            labelRightMargin->set(*buttonsWidth, SPAN/2);
        }
        else
        {
            labelRightMargin->set(-rule("halfunit"), SPAN);
            buttonHideTimer.setInterval(SPAN);
            buttonHideTimer.start();
        }
    }

    void menuItemTriggered(const ui::Item &actionItem) override
    {
        // Let the parent menu know which of its items is being interacted with.
        self().parentMenu()->setInteractedItem(
            self().parentMenu()->organizer().findItemForWidget(self()), &actionItem);
    }

    void updateColors()
    {
        auto bg = Background(style().colors().colorf(selected? selectedBgColor : bgColor));
        background->set(bg);
        label->setTextColor(selected? selectedTextColor : textColor);
        // Icon matches text color.
        if (icon)
        {
            icon->setImageColor(label->textColorf());
            icon->set(bg);
        }
    }

    /**
     * Determines if this item is inside a Home column. This is not true if the item
     * is used in a standalone dialog, for example.
     */
    bool hasColumnAncestor() const
    {
        for (Widget *i = self().parentWidget(); i; i = i->parent())
        {
            if (is<ColumnWidget>(i)) return true;
        }
        return false;
    }

    DE_PIMPL_AUDIENCES(Activity, DoubleClick, ContextMenu, Selection)
};

DE_AUDIENCE_METHODS(HomeItemWidget, Activity, DoubleClick, ContextMenu, Selection)

HomeItemWidget::HomeItemWidget(Flags flags, const String &name)
    : GuiWidget(name)
    , d(new Impl(this, flags))
{
    setBehavior(Focusable | ContentClipping);
    setAttribute(AutomaticOpacity);
    addEventHandler(new Impl::ClickHandler(*this));

    AutoRef<Rule> height;
    if (flags.testFlag(AnimatedHeight))
    {
        height.reset(new AnimationRule(d->label->rule().height(), 0.3));
    }
    else
    {
        height.reset(d->label->rule().height());
    }

    d->background->rule()
            .setInput(Rule::Top,    rule().top())
            .setInput(Rule::Height, height)
            .setInput(Rule::Left,   d->icon? d->icon->rule().right() : rule().left())
            .setInput(Rule::Right,  rule().right());

    if (d->icon)
    {
        d->icon->rule()
                .setSize(d->label->margins().height() +
                         style().fonts().font("default").height() +
                         style().fonts().font("default").lineSpacing(),
                         height)
                .setInput(Rule::Left, rule().left())
                .setInput(Rule::Top,  rule().top());
        d->icon->set(Background(Background::BorderGlow,
                                style().colors().colorf("home.icon.shadow"), 20));
    }

    d->label->rule()
            .setInput(Rule::Top,   rule().top())
            .setInput(Rule::Left,  d->icon? d->icon->rule().right() : rule().left())
            .setInput(Rule::Right, rule().right());
    d->label->margins().setRight(OperatorRule::maximum(*d->labelMinRightMargin,
                                                       *d->labelRightMargin) + rule("gap"));

    // Use an animated height rule for smoother list layout behavior.
    rule().setInput(Rule::Height, height);
}

AssetGroup &HomeItemWidget::assets()
{
    return d->assets;
}

LabelWidget &HomeItemWidget::icon()
{
    DE_ASSERT(d->icon);
    return *d->icon;
}

LabelWidget &HomeItemWidget::label()
{
    return *d->label;
}

const LabelWidget &HomeItemWidget::label() const
{
    return *d->label;
}

void HomeItemWidget::setSelected(bool selected)
{
    if (d->selected != selected)
    {
        d->selected = selected;
        if (selected)
        {
            d->showButtons(true);
        }
        else if (!d->keepButtonsVisible)
        {
            d->showButtons(false);
        }
        d->updateColors();
    }
}

bool HomeItemWidget::isSelected() const
{
    return d->selected;
}

void HomeItemWidget::useNormalStyle()
{
    useColorTheme(Normal);
}

void HomeItemWidget::useInvertedStyle()
{
    useColorTheme(Inverted);
}

void HomeItemWidget::useColorTheme(ColorTheme style)
{
    useColorTheme(style, style);
}

void HomeItemWidget::useColorTheme(ColorTheme unselected, ColorTheme selected)
{
    // Color for a non-selected item.
    if (unselected == Inverted)
    {
        d->bgColor   = "inverted.background";
        d->textColor = "inverted.text";
    }
    else
    {
        d->bgColor   = "transparent";
        d->textColor = "text";
    }

    // Color for a selected item.
    if (selected == Inverted)
    {
        d->selectedBgColor   = "home.item.background.selected.inverted";
        d->selectedTextColor = "inverted.text";
    }
    else
    {
        d->selectedBgColor   = "background";
        d->selectedTextColor = "text";
    }

    d->updateColors();
}

const DotPath &HomeItemWidget::textColorId() const
{
    return d->textColor;
}

void HomeItemWidget::acquireFocus()
{
    root().setFocus(this);
}

HomeMenuWidget *HomeItemWidget::parentMenu()
{
    return maybeAs<HomeMenuWidget>(parentWidget());
}

bool HomeItemWidget::handleEvent(const Event &event)
{
    if (hasFocus() && event.isKey())
    {
        const auto &key = event.as<KeyEvent>();

        if (key.ddKey() == DDKEY_LEFTARROW || key.ddKey() == DDKEY_RIGHTARROW ||
            key.ddKey() == DDKEY_UPARROW   || key.ddKey() == DDKEY_DOWNARROW)
        {
            if ( ! ((key.ddKey() == DDKEY_UPARROW    && isFirstChild()) ||
                    (key.ddKey() == DDKEY_DOWNARROW  && isLastChild())  ||
                    (key.ddKey() == DDKEY_LEFTARROW  && !d->hasColumnAncestor()) ||
                    (key.ddKey() == DDKEY_RIGHTARROW && !d->hasColumnAncestor())) )
            {
                // Fall back to menu and HomeWidget for navigation.
                return false;
            }
        }
    }
    return GuiWidget::handleEvent(event);
}

void HomeItemWidget::focusGained()
{
    setSelected(true);
    DE_NOTIFY(Selection, i) i->itemSelected(*this);
    DE_NOTIFY(Activity, i)  i->mouseActivity(*this);
}

void HomeItemWidget::focusLost()
{
    //setSelected(false);
    //emit deselected();
}

void HomeItemWidget::itemRightClicked()
{}

void HomeItemWidget::addButton(GuiWidget *widget)
{
    // Common styling.
    if (auto *label = maybeAs<LabelWidget>(widget))
    {
        label->setSizePolicy(ui::Expand, ui::Expand);
    }

    // Observing triggers.
    if (auto *menu = maybeAs<MenuWidget>(widget))
    {
        menu->audienceForItemTriggered() += d;
    }

    d->buttons << widget;
    d->label->add(widget);
    widget->setAttribute(DontDrawContent);
    widget->setBehavior(Focusable, false);
    d->updateButtonLayout();
}

GuiWidget &HomeItemWidget::buttonWidget(int index) const
{
    return *d->buttons.at(index);
}

void HomeItemWidget::setKeepButtonsVisible(bool yes)
{
    d->keepButtonsVisible = yes;
    if (yes)
    {
        d->showButtons(true);
    }
    else if (!d->selected)
    {
        d->showButtons(false);
    }
}

void HomeItemWidget::setLabelMinimumRightMargin(const Rule &rule)
{
    d->labelMinRightMargin->setSource(rule);
}

void HomeItemWidget::updateButtonLayout()
{
    d->updateButtonLayout();
}
