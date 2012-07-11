/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ADDRESS_H
#define LIBDENG2_ADDRESS_H

#include "../libdeng2.h"
#include "../Log"

#include <QHostAddress>
#include <QTextStream>

namespace de {

class String;
    
/**
 * IP address.
 *
 * @ingroup net
 */
class DENG2_PUBLIC Address : public LogEntry::Arg::Base
{
public:
    Address();

    /**
     * Constructs an Address.
     *
     * @param address  Network address. E.g., "localhost" or "127.0.0.1".
     * @param port     Port number.
     */
    Address(const QHostAddress& address, duint16 port = 0);

    Address(const char* address, duint16 port = 0);

    Address(const Address& other);

    /**
     * Checks two addresses for equality.
     *
     * @param other  Address.
     *
     * @return @c true if the addresses are equal.
     */
    bool operator == (const Address& other) const;

    const QHostAddress& host() const { return _host; }

    void setHost(const QHostAddress& host) { _host = host; }

    duint16 port() const { return _port; }

    void setPort(duint16 p) { _port = p; }

    /**
     * Checks if two IP address match. Port numbers are ignored.
     *
     * @param other  Address to check against.
     * @param mask  Net mask. Use to check if subnets match. The default
     *      checks if two IP addresses match.
     */
    bool matches(const Address& other, duint32 mask = 0xffffffff);

    /**
     * Converts the address to text.
     */
    String asText() const;

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::STRING; }

private:
    QHostAddress _host;
    duint16 _port;
};

DENG2_PUBLIC QTextStream& operator << (QTextStream& os, const Address& address);

} // namespace de

#endif // LIBDENG2_ADDRESS_H
