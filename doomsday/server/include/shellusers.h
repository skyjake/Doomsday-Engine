/** @file shellusers.h  All remote shell users.
 * @ingroup server
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

#ifndef SERVER_SHELLUSERS_H
#define SERVER_SHELLUSERS_H

#include <QObject>
#include <QSet>
#include "shelluser.h"

/**
 * All remote shell users.
 */
class ShellUsers : public QObject
{
    Q_OBJECT

public:
    ShellUsers();

    ~ShellUsers();

    /**
     * Adds a new remote shell user to the set of connected users. Users are
     * automatically removed from this collection and deleted when they are
     * disconnected.
     *
     * @param user  User. Ownership transferred.
     */
    void add(ShellUser *user);

    int count() const;

protected slots:
    void userDisconnected();

private:
    QSet<ShellUser *> _users;
};

#endif // SERVER_SHELLUSERS_H
