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

#include "WidgetActions"
#include "clientapp.h"
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

bool WidgetActions::tryEvent(Event const &event, String const &context)
{
    ddevent_t ddev;
    InputSystem::convertEvent(event, &ddev);

    if(context.isEmpty())
    {
        // Check all enabled contexts.
        return tryEvent(&ddev);
    }

    // Check a specific binding context for an action (regardless of its activation status).
    if(B_HasContext(context))
    {
        AutoRef<Action> act(B_Context(context).actionForEvent(&ddev, false));
        if(act.get())
        {
            act->trigger();
            return true;
        }
    }

    return false;
}

bool WidgetActions::tryEvent(ddevent_t const *ev)
{
    AutoRef<Action> act(B_ActionForEvent(ev));
    if(act.get())
    {
        act->trigger();
        return true;
    }
    return false;
}

void WidgetActions::trackInput(Event const &event)
{
    ddevent_t ddev;
    InputSystem::convertEvent(event, &ddev);
    ClientApp::inputSystem().trackEvent(&ddev);
}

void WidgetActions::activateContext(String const &context, bool yes)
{
    if(B_HasContext(context))
    {
        B_Context(context).activate(yes);
    }
}
