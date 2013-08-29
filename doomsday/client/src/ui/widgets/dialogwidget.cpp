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

#include "ui/widgets/dialogwidget.h"
#include "ui/widgets/togglewidget.h"
#include "ui/widgets/choicewidget.h"
#include "ui/framework/guirootwidget.h"
#include "ui/framework/signalaction.h"
#include "dd_main.h"

#include <de/KeyEvent>
#include <de/MouseEvent>
#include <QEventLoop>

using namespace de;

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

#ifdef MACOSX
    // Default buttons go to the end on OS X.
    if(!left.role().testFlag(DialogWidget::Default) && right.role().testFlag(DialogWidget::Default))
    {
        return true;
    }
    if(left.role().testFlag(DialogWidget::Default) && !right.role().testFlag(DialogWidget::Default))
    {
        return false;
    }

    bool const actionsFirst = true; // OS X actions before other buttons.
#else
    bool const actionsFirst = false; // Actions after the regular buttons.
#endif

    // Action buttons appear before/after other buttons.
    if(left.role().testFlag(DialogWidget::Action) && !right.role().testFlag(DialogWidget::Action))
    {
        return actionsFirst;
    }
    if(!left.role().testFlag(DialogWidget::Action) && right.role().testFlag(DialogWidget::Action))
    {
        return !actionsFirst;
    }

    // Order unchanged.
    return false;
}

DENG_GUI_PIMPL(DialogWidget),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetCreation),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetUpdate),
DENG2_OBSERVES(Widget, ChildAddition), // for styling the contents
DENG2_OBSERVES(ui::Data, Addition),
DENG2_OBSERVES(ui::Data, Removal)
{
    Modality modality;
    Flags flags;
    ScrollAreaWidget *area;
    MenuWidget *buttons;
    QEventLoop subloop;
    Animation glow;
    bool needButtonUpdate;
    float normalGlow;
    bool animatingGlow;

    Instance(Public *i, Flags const &dialogFlags)
        : Base(i),
          modality(Modal),
          flags(dialogFlags),
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
        area->audienceForChildAddition += this;

        buttons = new MenuWidget("buttons");
        buttons->items().audienceForAddition += this;
        buttons->items().audienceForRemoval += this;
        buttons->organizer().audienceForWidgetCreation += this;
        buttons->organizer().audienceForWidgetUpdate += this;

        // The menu maintains its own width and height based on children.
        // Set up one row with variable number of columns.
        buttons->setGridSize(0, ui::Expand, 1, ui::Expand);

        area->rule()
                .setInput(Rule::Left, self.rule().left())
                .setInput(Rule::Top, self.rule().top())
                .setInput(Rule::Width, area->contentRule().width() + area->margins().width());

        if(!flags.testFlag(Buttonless))
        {
            area->rule().setInput(Rule::Height, container->rule().height() -
                                  buttons->rule().height() + area->margins().bottom());

            // Buttons below the area.
            buttons->rule()
                    .setInput(Rule::Top, area->rule().bottom() - area->margins().bottom()) // overlap margins
                    .setInput(Rule::Right, self.rule().right());

            // A blank container widget acts as the popup content parent.
            container->rule().setInput(Rule::Width, OperatorRule::maximum(area->rule().width(),
                                                                          buttons->rule().width()));
        }
        else
        {
            area->rule().setInput(Rule::Height, container->rule().height() + area->margins().height());
            container->rule().setInput(Rule::Width, area->rule().width());
        }

        container->add(area);
        if(!flags.testFlag(Buttonless))
        {
            container->add(buttons);
        }
        self.setContent(container);
    }

    void updateContentHeight()
    {
        // The container's height is limited by the height of the view. Normally
        // the dialog tries to show the full height of the content area.
        if(!flags.testFlag(Buttonless))
        {
            self.content().rule().setInput(Rule::Height,
                                           OperatorRule::minimum(root().viewHeight(),
                                                                 area->contentRule().height() +
                                                                 area->margins().bottom() +
                                                                 buttons->rule().height()));
        }
        else
        {
            // A blank container widget acts as the popup content parent.
            self.content().rule().setInput(Rule::Height,
                                           OperatorRule::minimum(root().viewHeight(),
                                                                 area->contentRule().height() +
                                                                 area->margins().height()));
        }
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
                if(i->role().testFlag(Accept))
                {
                    but.setAction(new SignalAction(thisPublic, SLOT(accept())));
                }
                else if(i->role().testFlag(Reject))
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

    /**
     * Applies the default dialog style for child widgets added to
     * the content area.
     */
    void widgetChildAdded(Widget &areaChild)
    {
        GuiWidget &w = areaChild.as<GuiWidget>();

        w.margins().set("dialog.gap");

        // All label-based widgets should expand on their own.
        if(LabelWidget *lab = w.maybeAs<LabelWidget>())
        {
            lab->setSizePolicy(ui::Expand, ui::Expand);
        }

        // Toggles should have no background.
        if(ToggleWidget *tog = w.maybeAs<ToggleWidget>())
        {
            tog->set(Background());
        }
    }

    ui::ActionItem const *findDefaultAction() const
    {
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
};

DialogWidget::DialogWidget(String const &name, Flags const &flags)
    : PopupWidget(name), d(new Instance(this, flags))
{
    setOpeningDirection(ui::NoDirection);

    Background bg = background();
    if(!App_GameLoaded()) // blurring is not yet compatible with game rendering
    {
        /// @todo Should use the Style for this.
        bg.type = Background::BlurredWithBorderGlow;
        bg.solidFill = Vector4f(0, 0, 0, .65f);
    }
    else
    {
        bg.solidFill = style().colors().colorf("dialog.background");
    }
    set(bg);
}

DialogWidget::Modality DialogWidget::modality() const
{
    return d->modality;
}

ScrollAreaWidget &DialogWidget::area()
{
    return *d->area;
}

MenuWidget &DialogWidget::buttons()
{
    return *d->buttons;
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
    prepare();
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
                    but.action()->trigger();
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
        if(event.isKeyDown() ||
           (event.type() == Event::MouseButton &&
            event.as<MouseEvent>().state() == MouseEvent::Pressed &&
            !hitTest(event.as<MouseEvent>().pos())))
        {
            d->startBorderFlash();
        }
        return true;
    }
    else
    {
        if((event.type() == Event::MouseButton || event.type() == Event::MousePosition) &&
           hitTest(event.as<MouseEvent>().pos()))
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

    // Make sure the newly added widget knows the view size.
    viewResized();
    notifyTree(&Widget::viewResized);

    PopupWidget::open();
}

void DialogWidget::preparePopupForOpening()
{
    PopupWidget::preparePopupForOpening();

    // Redo the layout (items visible now).
    d->buttons->updateLayout();
}

void DialogWidget::finish(int)
{
    close();
}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, String const &label)
    : ui::ActionItem(label, 0), _role(flags)
{}

DialogWidget::ButtonItem::ButtonItem(RoleFlags flags, String const &label, de::Action *action)
    : ui::ActionItem(label, action), _role(flags)
{}
