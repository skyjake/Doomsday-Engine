/** @file dialogwidget.cpp  Popup dialog.
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

#include "de/DialogWidget"
#include "de/ToggleWidget"
#include "de/ChoiceWidget"
#include "de/GuiRootWidget"
#include "de/SignalAction"
#include "de/DialogContentStylist"

#include <de/KeyEvent>
#include <de/MouseEvent>
#include <QEventLoop>

namespace de {

static TimeDelta const FLASH_ANIM_SPAN = 0.75;

/**
 * Compares dialog button items to determine the order in which they
 * should appear in the UI.
 *
 * @param a  Dialog button item.
 * @param b  Dialog button item.
 *
 * @return @c true, if a < b.
 */
static bool dialogButtonOrder(ui::Item const &a, ui::Item const &b)
{
    DialogButtonItem const &left  = a.as<DialogButtonItem>();
    DialogButtonItem const &right = b.as<DialogButtonItem>();

    if(!left.role().testFlag(DialogWidget::Default) && right.role().testFlag(DialogWidget::Default))
    {
#ifdef MACOSX
        // Default buttons go to the right on OS X.
        return true;
#else
        // Default buttons to the left.
        return false;
#endif
    }
    if(left.role().testFlag(DialogWidget::Default) && !right.role().testFlag(DialogWidget::Default))
    {
#ifdef MACOSX
        // Default buttons go to the right on OS X.
        return false;
#else
        // Default buttons to the left.
        return true;
#endif
    }

    // Order unchanged.
    return false;
}

DENG_GUI_PIMPL(DialogWidget),
DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation),
DENG2_OBSERVES(ChildWidgetOrganizer, WidgetUpdate),
DENG2_OBSERVES(ui::Data, Addition),
DENG2_OBSERVES(ui::Data, Removal),
public ChildWidgetOrganizer::IFilter
{
    Modality modality;
    Flags flags;
    ScrollAreaWidget *area;
    LabelWidget *heading;
    MenuWidget *buttons;
    MenuWidget *extraButtons;
    ui::ListData buttonItems;
    QEventLoop subloop;
    Animation glow;
    bool needButtonUpdate;
    float normalGlow;
    bool animatingGlow;
    DialogContentStylist stylist;

    Instance(Public *i, Flags const &dialogFlags)
        : Base(i),
          modality(Modal),
          flags(dialogFlags),
          heading(0),
          needButtonUpdate(false),
          animatingGlow(false)
    {
        // Initialize the border glow.
        normalGlow = style().colors().colorf("glow").w;
        glow.setValue(normalGlow);
        glow.setStyle(Animation::Linear);

        // Set up widget structure.
        GuiWidget *container = new GuiWidget("container");

        area = new ScrollAreaWidget("area");

        buttons = new MenuWidget("buttons");
        buttons->margins().setTop("");
        buttons->setItems(buttonItems);
        buttons->items().audienceForAddition += this;
        buttons->items().audienceForRemoval += this;
        buttons->organizer().audienceForWidgetCreation += this;
        buttons->organizer().audienceForWidgetUpdate += this;
        buttons->organizer().setFilter(*this);

        extraButtons = new MenuWidget("extra");
        extraButtons->margins().setTop("");
        extraButtons->setItems(buttonItems);
        extraButtons->items().audienceForAddition += this;
        extraButtons->items().audienceForRemoval += this;
        extraButtons->organizer().audienceForWidgetCreation += this;
        extraButtons->organizer().audienceForWidgetUpdate += this;
        extraButtons->organizer().setFilter(*this);

        // The menu maintains its own width and height based on children.
        // Set up one row with variable number of columns.
        buttons->setGridSize(0, ui::Expand, 1, ui::Expand);
        extraButtons->setGridSize(0, ui::Expand, 1, ui::Expand);

        area->rule()
                .setInput(Rule::Left, self.rule().left())
                .setInput(Rule::Top, self.rule().top())
                .setInput(Rule::Width, area->contentRule().width() + area->margins().width());

        // Will a title be included?
        if(flags & WithHeading)
        {
            heading = new LabelWidget;
            heading->setFont("heading");
            heading->margins().setBottom("");
            heading->margins().setTop(style().rules().rule("gap") +
                                      style().rules().rule("dialog.gap"));
            heading->margins().setLeft(style().rules().rule("gap") +
                                       style().rules().rule("dialog.gap"));
            heading->setSizePolicy(ui::Expand, ui::Expand);
            heading->setTextColor("accent");
            heading->setAlignment(ui::AlignLeft);
            heading->setTextLineAlignment(ui::AlignLeft);
            container->add(heading);

            heading->rule()
                    .setInput(Rule::Top, self.rule().top())
                    .setInput(Rule::Left, self.rule().left());

            area->rule().setInput(Rule::Top, heading->rule().bottom());
        }

        area->rule().setInput(Rule::Height, container->rule().height() -
                              buttons->rule().height() /*+ area->margins().height()*/);

        // Buttons below the area.
        buttons->rule()
                .setInput(Rule::Bottom, container->rule().bottom())
                .setInput(Rule::Right, self.rule().right());
        extraButtons->rule()
                .setInput(Rule::Top, buttons->rule().top())
                .setInput(Rule::Left, self.rule().left());

        // A blank container widget acts as the popup content parent.
        container->rule().setInput(Rule::Width, OperatorRule::maximum(
                                       area->rule().width(),
                                       buttons->rule().width() + extraButtons->rule().width()));

        if(flags.testFlag(WithHeading))
        {
            area->rule().setInput(Rule::Height,
                                  container->rule().height()
                                  - heading->rule().height()
                                  - buttons->rule().height()/*
                                  + area->margins().bottom()*/);
        }

        container->add(area);
        container->add(buttons);
        container->add(extraButtons);
        self.setContent(container);
    }

    void updateContentHeight()
    {
        // Determine suitable maximum height.
        Rule const *maxHeight = holdRef(root().viewHeight());
        if(self.openingDirection() == ui::Down)
        {
            changeRef(maxHeight, *maxHeight - self.anchorY() - style().rules().rule("gap"));
        }

        // The container's height is limited by the height of the view. Normally
        // the dialog tries to show the full height of the content area.
        if(!flags.testFlag(WithHeading))
        {
            self.content().rule().setInput(Rule::Height,
                                           OperatorRule::minimum(*maxHeight,
                                                                 area->contentRule().height() +
                                                                 area->margins().height() +
                                                                 buttons->rule().height()));
        }
        else
        {
            self.content().rule().setInput(Rule::Height,
                                           OperatorRule::minimum(*maxHeight,
                                                                 heading->rule().height() +
                                                                 area->contentRule().height() +
                                                                 area->margins().height() +
                                                                 buttons->rule().height()));
        }

        releaseRef(maxHeight);
    }

    bool isItemAccepted(ChildWidgetOrganizer const &organizer, ui::Data const &data, ui::Data::Pos pos) const
    {
        // Only dialog buttons allowed in the dialog button menus.
        if(!data.at(pos).is<DialogButtonItem>()) return false;

        if(&organizer == &buttons->organizer())
        {
            // Non-Action buttons only.
            return !data.at(pos).as<DialogButtonItem>().role().testFlag(Action);
        }
        else if(&organizer == &extraButtons->organizer())
        {
            // Only Action buttons allowed.
            return data.at(pos).as<DialogButtonItem>().role().testFlag(Action);
        }

        DENG2_ASSERT(false); // unexpected
        return false;
    }

    void contextItemAdded(ui::Data::Pos, ui::Item const &)
    {
        needButtonUpdate = true;
    }

    void contextItemRemoved(ui::Data::Pos, ui::Item &)
    {
        needButtonUpdate = true;
    }

    void updateButtonLayout()
    {
        buttons->items().sort(dialogButtonOrder);
        //extraButtons->item().sort(dialogButtonOrder);

        needButtonUpdate = false;
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        // Make sure all label-based widgets in the button area
        // manage their own size.
        if(LabelWidget *lab = widget.maybeAs<LabelWidget>())
        {
            lab->setSizePolicy(ui::Expand, ui::Expand);
        }

        // Apply dialog button specific roles.
        if(ButtonItem const *i = item.maybeAs<ButtonItem>())
        {
            ButtonWidget &but = widget.as<ButtonWidget>();
            if(!i->action())
            {
                if(i->role() & (Accept | Yes))
                {
                    but.setAction(new SignalAction(thisPublic, SLOT(accept())));
                }
                else if(i->role() & (Reject | No))
                {
                    but.setAction(new SignalAction(thisPublic, SLOT(reject())));
                }
            }
        }
    }

    void widgetUpdatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        if(ButtonItem const *i = item.maybeAs<ButtonItem>())
        {
            ButtonWidget &but = widget.as<ButtonWidget>();

            // Button images must be a certain size.
            but.setOverrideImageSize(style().fonts().font("default").height().valuei());

            // Set default label?
            if(item.label().isEmpty())
            {
                if(i->role().testFlag(Accept))
                {
                    but.setText(tr("OK"));
                }
                else if(i->role().testFlag(Reject))
                {
                    but.setText(tr("Cancel"));
                }
                else if(i->role().testFlag(Yes))
                {
                    but.setText(tr("Yes"));
                }
                else if(i->role().testFlag(No))
                {
                    but.setText(tr("No"));
                }
            }

            // Highlight the default button(s).
            if(i->role().testFlag(Default))
            {
                but.setTextColor("dialog.default");
                but.setText(_E(b) + but.text());
            }
            else
            {
                but.setTextColor("text");
            }
        }
    }

    ui::ActionItem const *findDefaultAction() const
    {
        // Note: extra buttons not searched because they shouldn't contain default actions.

        for(ui::Data::Pos i = 0; i < buttons->items().size(); ++i)
        {
            ButtonItem const *act = buttons->items().at(i).maybeAs<ButtonItem>();
            if(act->role().testFlag(Default) &&
               buttons->organizer().itemWidget(i)->isEnabled())
            {
                return act;
            }
        }
        return 0;
    }

    ButtonWidget const &buttonWidget(ui::Item const &item) const
    {
        GuiWidget *w = extraButtons->organizer().itemWidget(item);
        if(w) return w->as<ButtonWidget>();
        // Try the normal buttons.
        return buttons->organizer().itemWidget(item)->as<ButtonWidget>();
    }

    void startBorderFlash()
    {
        animatingGlow = true;
        glow.setValueFrom(1, normalGlow, FLASH_ANIM_SPAN);
        Background bg = self.background();
        bg.color.w = glow;
        self.set(bg);
    }

    void updateBorderFlash()
    {
        Background bg = self.background();
        bg.color.w = glow;
        self.set(bg);

        if(glow.done()) animatingGlow = false;
    }

    void updateBackground()
    {
        Background bg = self.background();
        if(Style::appStyle().isBlurringAllowed())
        {
            /// @todo Should use the Style for this.
            bg.type = Background::BlurredWithBorderGlow;
            bg.solidFill = Vector4f(0, 0, 0, .65f);
        }
        else
        {
            bg.type = Background::BorderGlow;
            bg.solidFill = style().colors().colorf("dialog.background");
        }
        self.set(bg);
    }
};

DialogWidget::DialogWidget(String const &name, Flags const &flags)
    : PopupWidget(name), d(new Instance(this, flags))
{
    d->stylist.setContainer(area());
    setOpeningDirection(ui::NoDirection);
    d->updateBackground();
}

DialogWidget::Modality DialogWidget::modality() const
{
    return d->modality;
}

LabelWidget &DialogWidget::heading()
{
    DENG2_ASSERT(d->heading != 0);
    return *d->heading;
}

ScrollAreaWidget &DialogWidget::area()
{
    return *d->area;
}

/*
MenuWidget &DialogWidget::buttons()
{
    return *d->buttons;
}

MenuWidget &DialogWidget::extraButtons()
{
    return *d->extraButtons;
}
*/

ui::Data &DialogWidget::buttons()
{
    return d->buttonItems;
}

ButtonWidget &DialogWidget::buttonWidget(de::String const &label) const
{
    GuiWidget *w = d->buttons->organizer().itemWidget(label);
    if(w) return w->as<ButtonWidget>();

    w = d->extraButtons->organizer().itemWidget(label);
    DENG2_ASSERT(w != 0);

    return w->as<ButtonWidget>();
}

ButtonWidget *DialogWidget::buttonWidget(int roleId) const
{
    for(uint i = 0; i < d->buttonItems.size(); ++i)
    {
        DialogButtonItem const &item = d->buttonItems.at(i).as<DialogButtonItem>();

        if((item.role() & IdMask) == roleId)
        {
            GuiWidget *w = d->buttons->organizer().itemWidget(i);
            if(w) return &w->as<ButtonWidget>();

            return &d->extraButtons->organizer().itemWidget(i)->as<ButtonWidget>();
        }
    }
    return 0;
}

int DialogWidget::exec(GuiRootWidget &root)
{
    d->modality = Modal;

    // The widget is added to the root temporarily (as top child).
    DENG2_ASSERT(!hasRoot());
    root.add(this);

    prepare();

    int result = d->subloop.exec();

    finish(result);

    return result;
}

void DialogWidget::open()
{
    d->modality = NonModal;

    DENG2_ASSERT(hasRoot());
    prepare(); // calls base class's open()
}

ui::ActionItem *DialogWidget::defaultActionItem()
{
    return const_cast<ui::ActionItem *>(d->findDefaultAction());
}

void DialogWidget::update()
{
    PopupWidget::update();

    if(d->needButtonUpdate)
    {
        d->updateButtonLayout();
    }

    if(d->animatingGlow)
    {
        d->updateBorderFlash();
    }
}

bool DialogWidget::handleEvent(Event const &event)
{
    if(event.isKeyDown())
    {
        KeyEvent const &key = event.as<KeyEvent>();

        if(key.ddKey() == DDKEY_ENTER ||
           key.ddKey() == DDKEY_RETURN ||
           key.ddKey() == ' ')
        {
            if(ui::ActionItem const *defaultAction = d->findDefaultAction())
            {
                ButtonWidget const &but = d->buttonWidget(*defaultAction);
                if(but.action())
                {
                    const_cast<de::Action *>(but.action())->trigger();
                }
            }
            return true;
        }

        if(key.ddKey() == DDKEY_ESCAPE)
        {
            // Esc always cancels a dialog.
            reject();
            return true;
        }
    }

    if(d->modality == Modal)
    {
        // The event should already have been handled by the children.
        if((event.isKeyDown() && !(event.as<KeyEvent>().qtKey() == Qt::Key_Shift)) ||
           (event.type() == Event::MouseButton &&
            event.as<MouseEvent>().state() == MouseEvent::Pressed &&
            !hitTest(event)))
        {
            d->startBorderFlash();
        }
        return true;
    }
    else
    {
        if((event.type() == Event::MouseButton || event.type() == Event::MousePosition ||
            event.type() == Event::MouseWheel) &&
           hitTest(event))
        {
            // Non-modal dialogs eat mouse clicks/position inside the dialog.
            return true;
        }
    }

    return PopupWidget::handleEvent(event);
}

void DialogWidget::accept(int result)
{
    if(d->subloop.isRunning())
    {
        DENG2_ASSERT(d->modality == Modal);
        d->subloop.exit(result);
        emit accepted(result);
    }
    else if(d->modality == NonModal)
    {
        emit accepted(result);
        finish(result);
    }
}

void DialogWidget::reject(int result)
{
    if(d->subloop.isRunning())
    {
        DENG2_ASSERT(d->modality == Modal);
        d->subloop.exit(result);
        emit rejected(result);
    }
    else if(d->modality == NonModal)
    {
        emit rejected(result);
        finish(result);
    }
}

void DialogWidget::prepare()
{
    root().setFocus(0);

    if(openingDirection() == ui::NoDirection)
    {
        // Center the dialog.
        setAnchor(root().viewWidth() / 2, root().viewHeight() / 2);
    }

    d->updateContentHeight();

    PopupWidget::open();
}

void DialogWidget::preparePanelForOpening()
{
    PopupWidget::preparePanelForOpening();

    // Redo the layout (items visible now).
    d->buttons->updateLayout();
    d->extraButtons->updateLayout();

    d->updateBackground();
}

void DialogWidget::finish(int)
{
    root().setFocus(0);
    close();
}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, String const &label)
    : ui::ActionItem(label, 0), _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, String const &label, RefArg<de::Action> action)
    : ui::ActionItem(label, action), _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, Image const &image, RefArg<de::Action> action)
    : ui::ActionItem(image, "", action), _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, Image const &image, String const &label, RefArg<de::Action> action)
    : ui::ActionItem(image, label, action), _role(flags)
{}

} // namespace de
