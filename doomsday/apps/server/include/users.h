/** @file users.h  Users: connected clients.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef SERVER_USERS_H
#define SERVER_USERS_H

#include <de/address.h>
#include <de/observers.h>

/**
 * Abstract base class representing a connected client.
 */
class User
{
public:
    virtual ~User() {}

    virtual de::Address address() const = 0;

    DE_CAST_METHODS()
    DE_DEFINE_AUDIENCE(Disconnect, void userDisconnected(User &))
};

/**
 * A set of connected clients.
 */
class Users
{
public:
    Users();

    virtual ~Users();

    /**
     * Adds a new user to the set of connected users. Users are automatically removed
     * from this collection and deleted when they are disconnected.
     *
     * @param user  User. Ownership transferred.
     */
    virtual void add(User *user);

    int count() const;

    de::LoopResult forUsers(const std::function<de::LoopResult (User &)>& func);

private:
    DE_PRIVATE(d)
};


#endif // SERVER_USERS_H
