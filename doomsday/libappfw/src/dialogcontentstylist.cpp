/** @file dialogcontentstylist.cpp Sets the style for widgets in a dialog.
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

#include "DialogContentStylist"
#include "ui/widgets/dialogwidget.h"
#include "ui/widgets/togglewidget.h"
#include "ui/widgets/labelwidget.h"

using namespace de;

DialogContentStylist::DialogContentStylist() : _container(0)
{}

DialogContentStylist::DialogContentStylist(DialogWidget &dialog) : _container(0)
{
    setContainer(dialog.area());
}

DialogContentStylist::DialogContentStylist(GuiWidget &container) : _container(0)
{
    setContainer(container);
}

DialogContentStylist::~DialogContentStylist()
{
    if(_container)
    {
        _container->audienceForChildAddition -= this;
    }
}

void DialogContentStylist::setContainer(GuiWidget &container)
{
    if(_container)
    {
        _container->audienceForChildAddition -= this;
    }

    _container = &container;
    _container->audienceForChildAddition += this;
}

void DialogContentStylist::widgetChildAdded(Widget &child)
{
    applyStyle(child.as<GuiWidget>());
}

void DialogContentStylist::applyStyle(GuiWidget &w)
{
    w.margins().set("dialog.gap");

    // All label-based widgets should expand on their own.
    if(LabelWidget *lab = w.maybeAs<LabelWidget>())
    {
        lab->setSizePolicy(ui::Expand, ui::Expand);
    }

    // Toggles should have no background.
    if(ToggleWidget *tog = w.maybeAs<ToggleWidget>())
    {
        tog->set(GuiWidget::Background());
    }
}
