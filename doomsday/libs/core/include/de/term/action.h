/** @file shell/action.h  Maps a key event to a signal.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_ACTION_H
#define LIBSHELL_ACTION_H

#include "keyevent.h"
#include "de/action.h"

namespace de { namespace term {

/**
 * Maps a key event to a signal.
 *
 * @ingroup textUi
 */
class DE_PUBLIC Action : public de::Action
{
public:
    using Func = std::function<void()>;

public:
    Action(const String &label);

    Action(const String &label, const Func &func);

//    Action(const String &label, QObject *target, const char *slot = 0);

    Action(const String &label, const KeyEvent &event, const Func &func = {});

    Action(const KeyEvent &event, const Func &func);

    void setLabel(const String &label);

    String label() const;

    /**
     * Triggers the action if the event matches the action's condition.
     *
     * @param ev  Event to check.
     *
     * @return @c true, if the event is eaten by the action.
     */
    bool tryTrigger(const KeyEvent &ev);

protected:
    ~Action();

private:
    KeyEvent _event;
    String   _label;
};

}} // namespace de::shell

#endif // LIBSHELL_ACTION_H
