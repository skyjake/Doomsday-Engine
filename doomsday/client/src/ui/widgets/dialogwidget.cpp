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
#include "ui/widgets/guirootwidget.h"
#include "ui/signalaction.h"
#include "dd_main.h"

#include <de/KeyEvent>
#include <QEventLoop>

using namespace de;

DENG2_PIMPL(DialogWidget),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetCreation),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetUpdate)
{
    Modality modality;
    ScrollAreaWidget *area;
    MenuWidget *buttons;
    QEventLoop subloop;

    Instance(Public *i) : Base(i), modality(Modal)
    {
        GuiWidget *container = new GuiWidget("container");

        area    = new ScrollAreaWidget("area");
        buttons = new MenuWidget("buttons");

        buttons->organizer().audienceForWidgetCreation += this;
        buttons->organizer().audienceForWidgetUpdate += this;

        // The menu maintains its own width and height based on children.
        // Set up one row with variable number of columns.
        buttons->setGridSize(0, ui::Expand, 1, ui::Expand);

        area->rule()
                .setInput(Rule::Left, self.rule().left())
                .setInput(Rule::Top, self.rule().top())
                .setInput(Rule::Width, area->contentRule().width() + area->margin() * 2)
                .setInput(Rule::Height, container->rule().height() - buttons->rule().height() +
                          area->margin());

        // Buttons below the area.
        buttons->rule()
                .setInput(Rule::Top, area->rule().bottom() - area->margin()) // overlap margins
                .setInput(Rule::Right, self.rule().right());

        // A blank container widget acts as the popup content parent.
        container->rule().setInput(Rule::Width, OperatorRule::maximum(area->rule().width(),
                                                                      buttons->rule().width()));
        container->add(area);
        container->add(buttons);
        self.setContent(container);
    }

    void updateContentHeight()
    {
        // The container's height is limited by the height of the view. Normally
        // the dialog tries to show the full height of the content area.

        DENG2_ASSERT(self.hasRoot());
        self.content().rule().setInput(Rule::Height,
                                       OperatorRule::minimum(self.root().viewHeight(),
                                                             area->contentRule().height() +
                                                             area->margin() +
                                                             buttons->rule().height()));
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

            if(i->role().testFlag(Default))
            {
                but.setText(_E(b) + but.text());
            }
        }
    }

    ui::ActionItem const *findDefaultAction() const
    {
        for(ui::Context::Pos i = 0; i < buttons->items().size(); ++i)
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
};

DialogWidget::DialogWidget(String const &name)
    : PopupWidget(name), d(new Instance(this))
{
    setOpeningDirection(ui::NoDirection);

    if(!App_GameLoaded()) // blurring is not yet compatible with game rendering
    {
        Background bg = background();
        bg.type = Background::BlurredWithBorderGlow;
        bg.solidFill = Vector4f(0, 0, 0, .65f);
        set(bg);
    }
}

void DialogWidget::setModality(DialogWidget::Modality modality)
{
    d->modality = modality;
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
    // The widget is added to the root temporarily (as top child).
    DENG2_ASSERT(!hasRoot());
    root.add(this);

    prepare();

    int result = d->subloop.exec();

    finish(result);

    return result;
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
            ui::ActionItem const *defaultAction = d->findDefaultAction();
            ButtonWidget const &but = d->buttonWidget(*defaultAction);
            if(but.action())
            {
                but.action()->trigger();
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
        return true;
    }

    return PopupWidget::handleEvent(event);
}

void DialogWidget::accept(int result)
{
    if(d->subloop.isRunning())
    {
        d->subloop.exit(result);
        emit accepted(result);
    }
}

void DialogWidget::reject(int result)
{
    if(d->subloop.isRunning())
    {
        d->subloop.exit(result);
        emit rejected(result);
    }
}

void DialogWidget::prepare()
{
    if(openingDirection() == ui::NoDirection)
    {
        // Center the dialog.
        setAnchor(root().viewWidth() / 2, root().viewHeight() / 2);
    }

    d->updateContentHeight();

    // Make sure the newly added widget knows the view size.
    viewResized();
    notifyTree(&Widget::viewResized);

    d->buttons->updateLayout();

    open();
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
