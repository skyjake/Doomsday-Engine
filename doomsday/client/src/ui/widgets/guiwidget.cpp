/** @file guiwidget.cpp  Base class for graphical widgets.
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

#include "ui/widgets/guiwidget.h"
#include "ui/widgets/guirootwidget.h"
#include "clientapp.h"
#include <de/garbage.h>

using namespace de;

DENG2_PIMPL(GuiWidget)
{
    RuleRectangle rule;

    Instance(Public *i) : Base(i)
    {}
};

GuiWidget::GuiWidget(String const &name) : Widget(name), d(new Instance(this))
{}

GuiRootWidget &GuiWidget::root()
{
    return static_cast<GuiRootWidget &>(Widget::root());
}

Style const &GuiWidget::style()
{
    return ClientApp::windowSystem().style();
}

RuleRectangle &GuiWidget::rule()
{
    return d->rule;
}

RuleRectangle const &GuiWidget::rule() const
{
    return d->rule;
}

static void deleteGuiWidget(void *ptr)
{
    delete reinterpret_cast<GuiWidget *>(ptr);
}

void GuiWidget::deleteLater()
{
    Garbage_TrashInstance(this, deleteGuiWidget);
}
