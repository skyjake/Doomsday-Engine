/** @file dialogcontentstylist.cpp Sets the style for widgets in a dialog.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/DialogContentStylist"
#include "de/DialogWidget"
#include "de/ToggleWidget"
#include "de/LabelWidget"
#include "de/LineEditWidget"
#include "de/AuxButtonWidget"
#include <de/Zeroed>

namespace de {

DENG2_PIMPL_NOREF(DialogContentStylist)
{
    QList<GuiWidget *> containers;
    bool useInfoStyle;
    bool adjustMargins;

    Instance()
        : useInfoStyle(false)
        , adjustMargins(true)
    {}
};

DialogContentStylist::DialogContentStylist() : d(new Instance)
{}

DialogContentStylist::DialogContentStylist(DialogWidget &dialog) : d(new Instance)
{
    setContainer(dialog.area());
}

DialogContentStylist::DialogContentStylist(GuiWidget &container) : d(new Instance)
{
    setContainer(container);
}

DialogContentStylist::~DialogContentStylist()
{
    clear();
}

void DialogContentStylist::clear()
{
    foreach(GuiWidget *w, d->containers)
    {
        w->audienceForChildAddition() -= this;
    }
    d->containers.clear();
}

void DialogContentStylist::setContainer(GuiWidget &container)
{
    clear();
    addContainer(container);
}

void DialogContentStylist::addContainer(GuiWidget &container)
{
    d->containers << &container;
    container.audienceForChildAddition() += this;
}

void DialogContentStylist::setInfoStyle(bool useInfoStyle)
{
    d->useInfoStyle = useInfoStyle;
}

void DialogContentStylist::setAdjustMargins(bool yes)
{
    d->adjustMargins = yes;
}

void DialogContentStylist::widgetChildAdded(Widget &child)
{
    applyStyle(child.as<GuiWidget>());
}

void DialogContentStylist::applyStyle(GuiWidget &w)
{
    if(d->adjustMargins)
    {
        if(!w.is<AuxButtonWidget>())
        {
            w.margins().set("dialog.gap");
        }
    }

    // All label-based widgets should expand on their own.
    if(LabelWidget *lab = w.maybeAs<LabelWidget>())
    {
        lab->setSizePolicy(ui::Expand, ui::Expand);
    }

    // Button background override?
    if(ButtonWidget *but = w.maybeAs<ButtonWidget>())
    {
        if(d->useInfoStyle)
        {
            but->useInfoStyle();
        }
    }

    // Toggles should have no background.
    if(ToggleWidget *tog = w.maybeAs<ToggleWidget>())
    {
        tog->set(GuiWidget::Background());
    }

    if(LineEditWidget *ed = w.maybeAs<LineEditWidget>())
    {
        ed->rule().setInput(Rule::Width, d->containers.first()->style().rules().rule("editor.width"));
    }
}

} // namespace de
