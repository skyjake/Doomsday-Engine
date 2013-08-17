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
#include "dd_main.h"

#include <QEventLoop>

using namespace de;

DENG2_PIMPL(DialogWidget),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetCreation)
{
    Modality modality;
    ScrollAreaWidget *area;
    MenuWidget *buttons;
    QEventLoop subloop;

    Instance(Public *i) : Base(i), modality(Modal)
    {
        area    = new ScrollAreaWidget("area");
        buttons = new MenuWidget("buttons");

        buttons->organizer().audienceForWidgetCreation += this;

        // The menu maintains its own width and height based on children.
        // Set up one row with variable number of columns.
        buttons->setGridSize(0, ui::Expand, 1, ui::Expand);

        area->rule()
                .setInput(Rule::Left, self.rule().left())
                .setInput(Rule::Top, self.rule().top())
                .setInput(Rule::Width, area->contentRule().width() + area->margin() * 2)
                .setInput(Rule::Height, area->contentRule().height() + area->margin() * 2);

        // Buttons below the area.
        buttons->rule()
                .setInput(Rule::Top, area->rule().bottom() - area->margin()) // overlap margins
                .setInput(Rule::Right, self.rule().right());

        // A blank container widget acts as the popup content parent.
        GuiWidget *container = new GuiWidget("container");
        container->rule()
                .setInput(Rule::Width, OperatorRule::maximum(area->rule().width(), buttons->rule().width()))
                .setInput(Rule::Height, area->rule().height() + buttons->rule().height() -
                                        area->margin());
        container->add(area);
        container->add(buttons);
        self.setContent(container);
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &)
    {
        // Make sure all label-based widgets in the button area
        // manage their own size.
        if(widget.is<LabelWidget>())
        {
            widget.as<LabelWidget>().setSizePolicy(ui::Expand, ui::Expand);
        }
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

ScrollAreaWidget &DialogWidget::content()
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

void DialogWidget::preparePopupForOpening()
{
    PopupWidget::preparePopupForOpening();

    // Redo the layout (items visible now).
    d->buttons->updateLayout();
}

void DialogWidget::prepare()
{
    // Center the dialog.
    setAnchor(root().viewWidth() / 2, root().viewHeight() / 2);
    d->buttons->updateLayout();

    // Make sure the newly added widget knows the view size.
    viewResized();
    notifyTree(&Widget::viewResized);

    open();
}

void DialogWidget::finish(int)
{
    close();
}
