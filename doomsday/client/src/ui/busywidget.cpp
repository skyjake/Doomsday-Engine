/** @file busywidget.cpp
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

#include "ui/busywidget.h"
#include "ui/busyvisual.h"
#include "busymode.h"

using namespace de;

DENG2_PIMPL(BusyWidget)
{
    Instance(Public *i) : Base(i)
    {}
};

BusyWidget::BusyWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
}

BusyWidget::~BusyWidget()
{
    delete d;
}

void BusyWidget::update()
{
    DENG_ASSERT(BusyMode_Active());
    BusyMode_Loop();
}

void BusyWidget::draw()
{
    DENG_ASSERT(BusyMode_Active());
    BusyVisual_Render();
}

bool BusyWidget::handleEvent(Event const &)
{
    // Eat events and ignore them.
    return true;
}
