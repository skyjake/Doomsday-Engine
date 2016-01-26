/** @file columnwidget.cpp  Home column.
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

#include "ui/home/columnwidget.h"

#include <de/LabelWidget>
#include <de/Range>

using namespace de;

DENG_GUI_PIMPL(ColumnWidget)
{
    Instance(Public *i) : Base(i)
    {}
};

ColumnWidget::ColumnWidget(String const &name)
    : GuiWidget(name)
    , d(new Instance(this))
{
    LabelWidget *back = new LabelWidget;
    back->set(Background(Vector4f(0, Rangef(0, .5f).random(), Rangef(0, .5f).random(), 1)));
    back->rule().setRect(rule());
    add(back);
}
