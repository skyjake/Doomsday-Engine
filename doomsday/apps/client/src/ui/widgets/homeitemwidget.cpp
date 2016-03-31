/** @file homeitemwidget.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/SequentialLayout>
#include <QTimer>

using namespace de;

DENG_GUI_PIMPL(HomeItemWidget)
{
    struct ClickHandler : public GuiWidget::IEventHandler
    {
        Public &owner;

        ClickHandler(Public &owner)
            : owner(owner) {}

        void acquireFocus()
        {
            owner.acquireFocus();
            emit owner.mouseActivity();
        }

        bool handleEvent(GuiWidget &widget, Event const &event)
        {
            if(widget.isDisabled()) return false;

            if(event.type() == Event::MouseButton)
            {
                MouseEvent const &mouse = event.as<MouseEvent>();
                if(owner.hitTest(event))
                {
                    if(mouse.button() == MouseEvent::Right)
                    {
                        switch(widget.handleMouseClick(event, MouseEvent::Right))
                        {
                        case MouseClickStarted:
                            acquireFocus();
                            return true;

                        case MouseClickAborted:
                            return true;

                        case MouseClickFinished:
                            emit owner.openContextMenu();
                            return true;

                        default:
                            return false; // Ignore.
                        }
                    }

                    if(mouse.state() == MouseEvent::Pressed ||
                       mouse.state() == MouseEvent::DoubleClick)
                    {
                        acquireFocus();
                    }
                    if(mouse.state()  == MouseEvent::DoubleClick &&
                       mouse.button() == MouseEvent::Left)
                    {
                        emit owner.doubleClicked();
                        return true;
                    }
                }
            }
            return false;
        }
    };

    LabelWidget *background;
    LabelWidget *icon;
    LabelWidget *label;
    QList<ButtonWidget *> buttons;
    ScalarRule *labelRightMargin;
    Rule const *buttonsWidth = nullptr;
    bool selected = false;
    DotPath bgColor           { "transparent" };
    DotPath selectedBgColor   { "background" };
    DotPath textColor         { "text" };
    DotPath selectedTextColor { "text" };
    QTimer buttonHideTimer;

    Instance(Public *i) : Base(i)
    {
        labelRightMargin    = new ScalarRule(0);

        self.add(background = new LabelWidget);
        self.add(icon       = new LabelWidget);
        self.add(label      = new LabelWidget);

        label->setSizePolicy(ui::Filled, ui::Expand);
        label->setTextLineAlignment(ui::AlignLeft);
        label->setAlignment(ui::AlignLeft);
        label->setBehavior(ChildVisibilityClipping);

        background->setBehavior(Focusable);
        background->addEventHandler(new ClickHandler(self));

        buttonHideTimer.setSingleShot(true);
        QObject::connect(&buttonHideTimer, &QTimer::timeout, [this] ()
        {
            for(auto *btn : buttons) btn->hide();
        });
    }

    ~Instance()
    {
        releaseRef(labelRightMargin);
        releaseRef(buttonsWidth);
    }

    void updateButtonLayout()
    {
        SequentialLayout layout(label->rule().right() - *labelRightMargin,
                                label->rule().top(), ui::Right);
        for(auto *button : buttons)
        {
            layout << *button;
            button->rule().setMidAnchorY(label->rule().midY());
        }
        changeRef(buttonsWidth, layout.width() + rule("gap"));
    }

    void showButtons(bool show)
    {
        if(!buttonsWidth) return;

        if(show)
        {
            buttonHideTimer.stop();
            for(auto *button : buttons) button->show();
        }

        TimeDelta const SPAN = .5;
        if(show)
        {
            labelRightMargin->set(*buttonsWidth, SPAN);
        }
        else
        {
            labelRightMargin->set(-rule("halfunit"), SPAN);
            buttonHideTimer.setInterval(SPAN.asMilliSeconds());
            buttonHideTimer.start();
        }
    }

    void updateColors()
    {
        background->set(Background(style().colors().colorf(selected? selectedBgColor : bgColor)));
        label->setTextColor(selected? selectedTextColor : textColor);
    }
};

HomeItemWidget::HomeItemWidget(String const &name)
    : GuiWidget(name)
    , d(new Instance(this))
{
    setBehavior(Focusable);

    Rule const &iconSize = d->label->margins().height() +
                           style().fonts().font("default").height() +
                           style().fonts().font("default").lineSpacing();

    d->background->rule()
            .setInput(Rule::Top,    rule().top())
            .setInput(Rule::Left,   d->icon->rule().right())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, d->label->rule().bottom());

    d->icon->rule()
            .setSize(iconSize,    d->label->rule().height())
            .setInput(Rule::Left, rule().left())
            .setInput(Rule::Top,  rule().top());
    d->icon->set(Background(Background::BorderGlow,
                            style().colors().colorf("home.icon.shadow"), 20));

    d->label->rule()
            .setInput(Rule::Top,   rule().top())
            .setInput(Rule::Left,  d->icon->rule().right())
            .setInput(Rule::Right, rule().right());
    d->label->margins().setRight(*d->labelRightMargin +
                                 rule("gap"));

    rule().setInput(Rule::Height, d->label->rule().height());
}

LabelWidget &HomeItemWidget::icon()
{
    return *d->icon;
}

LabelWidget &HomeItemWidget::label()
{
    return *d->label;
}

void HomeItemWidget::setSelected(bool selected)
{
    if(d->selected != selected)
    {
        d->selected = selected;
        if(selected)
        {
            d->showButtons(true);
        }
        else
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
    if(unselected == Inverted)
    {
        d->bgColor   = "accent";
        d->textColor = "inverted.accent";
    }
    else
    {
        d->bgColor   = "transparent";
        d->textColor = "text";
    }

    if(selected == Inverted)
    {
        d->selectedBgColor   = "accent";
        d->selectedTextColor = "inverted.text";
    }
    else
    {
        d->selectedBgColor   = "background";
        d->selectedTextColor = "text";
    }

    d->updateColors();
}

void HomeItemWidget::acquireFocus()
{
    root().setFocus(d->background);
}

void HomeItemWidget::addButton(ButtonWidget *button)
{
    // Common styling.
    button->setSizePolicy(ui::Expand, ui::Expand);

    d->buttons << button;
    d->label->add(button);
    button->hide();
    d->updateButtonLayout();
}
