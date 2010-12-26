/*
 * The Doomsday Engine Project -- dengsv
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <de/types.h>
#include <de/net.h>

#include <QObject>
#include <QFlags>

/**
 * Represents a network connection to a remote party.
 */
class Client : public QObject, public de::Transmitter
{
public:
    enum Right
    {
        /// Client has administration rights. Granted to local users automatically.
        AdminRight = 0x1
    };
    Q_DECLARE_FLAGS(Rights, Right);
    
public:
    Client(const de::Address& address);
    Client(de::Socket* socket);
    
    /**
     * Grants the client whatever rights it should have.
     */
    void grantRights();

    de::Socket& socket();
    de::Channel& base();
    de::Channel& updates();

    // Implements de::Transmitter.
    virtual void send(const de::IByteArray &data);
    
protected:
    void initialize();

public:
    /// Access rights of the client.
    Rights rights;

signals:
    void messageReady();
    void disconnected();

private:
    de::Socket* _socket;
    de::Channel* _base;
    de::Channel* _updates;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Client::Rights);

#endif /* CLIENT_H */
