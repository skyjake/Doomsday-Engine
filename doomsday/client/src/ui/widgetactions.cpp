/** @file widgetactions.cpp  User actions bound to widgets.
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

#include "ui/widgetactions.h"
#include "ui/b_main.h"
#include "ui/b_context.h"

using namespace de;

DENG2_PIMPL(WidgetActions)
{
    Instance(Public *i) : Base(i)
    {
        B_Init();
    }

    ~Instance()
    {
        B_Shutdown();
    }
};

WidgetActions::WidgetActions() : d(new Instance(this))
{}

Action *WidgetActions::actionForEvent(Event const &event)
{
    return 0;
}

bool WidgetActions::tryEvent(Event const &event)
{
    ddevent_t ddev;
    DD_ConvertEvent(event, &ddev);
    return tryEvent(&ddev);
}

bool WidgetActions::tryEvent(ddevent_t const *ev)
{
    std::auto_ptr<Action> act(B_ActionForEvent(ev));
    if(act.get())
    {
        act->trigger();
        return true;
    }
    return false;
}
