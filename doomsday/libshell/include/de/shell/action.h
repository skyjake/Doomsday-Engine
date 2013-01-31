/** @file action.h  Maps a key event to a signal.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_ACTION_H
#define LIBSHELL_ACTION_H

#include "KeyEvent"
#include <QObject>

namespace de {
namespace shell {

/**
 * Maps a key event to a signal.
 */
class Action : public QObject
{
    Q_OBJECT

public:
    Action(KeyEvent const &event, QObject *target = 0, char const *slot = 0);

    Action(String const &label, KeyEvent const &event, QObject *target = 0, char const *slot = 0);

    Action(String const &label, QObject *target, char const *slot = 0);

    Action(String const &label);

    ~Action();

    String label() const;

    /**
     * Triggers the action if the event matches the action's condition.
     *
     * @param ev  Event to check.
     *
     * @return @c true, if the event is eaten by the action.
     */
    bool tryTrigger(KeyEvent const &ev);

    void trigger();

signals:
    void triggered();

private:
    KeyEvent _event;
    String _label;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_ACTION_H
