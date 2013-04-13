/** @file widgetactions.h  User actions bound to widgets.
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

#ifndef CLIENT_WIDGETACTIONS_H
#define CLIENT_WIDGETACTIONS_H

#include <de/libdeng2.h>
#include <de/Action>
#include <de/Event>
#include "guiwidget.h"
#include "dd_input.h"

/**
 * User actions bound to widgets. Traditionally called the bindings and binding
 * contexts. @ingroup ui
 *
 * @todo "WidgetActions" is a work-in-progress name.
 *
 * @todo There should be one of these in every widget that has actions bound.
 * When that's done, the old binding contexts become obsolete.
 *
 * @todo What to do about control bindings?
 */
class WidgetActions
{
public:
    WidgetActions();

    /**
     * Returns an action that the user has bound to the provided event.
     *
     * @param event  Event instance.
     *
     * @return  Action bound to the event. Caller gets ownership of the Action.
     */
    de::Action *actionForEvent(de::Event const &event);

    /**
     * If an action has been defined for the event, trigger it.
     *
     * @param event  Event instance.
     *
     * @return @c true if even was triggered, @c false otherwise.
     */
    bool tryEvent(de::Event const &event);

    bool tryEvent(ddevent_t const *ev);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_WIDGETACTIONS_H
