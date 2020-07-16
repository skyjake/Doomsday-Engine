/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ADDRESS_H
#define LIBCORE_ADDRESS_H

#include "de/libcore.h"
#include "de/log.h"

#include <sstream>
#include <the_Foundation/address.h>

namespace de {

class String;

/**
 * IP address.
 *
 * @ingroup net
 */
class DE_PUBLIC Address : public LogEntry::Arg::Base
{
public:
    Address();

    /**
     * Constructs an Address.
     *
     * @param hostNameOrAddress  IP address. E.g., "localhost" or "127.0.0.1".
     *                           Domain names are allowed.
     * @param port  Port number.
     */
    Address(const char *hostNameOrAddress, duint16 port = 0);

    Address(const iAddress *);
    Address(const Address &other);

    operator const iAddress *() const;

    Address &operator=(const Address &other);

    /**
     * Returns the host name that was passed to lookup.
     */
    String hostName() const;

    bool operator<(const Address &other) const;

    /**
     * Checks two addresses for equality.
     *
     * @param other  Address.
     *
     * @return @c true if the addresses are equal.
     */
    bool operator==(const Address &other) const;

    bool isNull() const;

    bool isLoopback() const;

    /**
     * Determines if the address is one of the network interfaces of the local computer.
     */
    bool isLocal() const;

    duint16 port() const;

    /**
     * Converts the address to text.
     */
    String asText() const override;

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const override { return LogEntry::Arg::StringArgument; }

public:
    static Address take(iAddress *);

    static Address parse(const String &addressWithOptionalPort, duint16 defaultPort = 0);

    /**
     * Determines whether a host address refers to the local host.
     */
    static bool isHostLocal(const Address &host);

    static List<Address> localAddresses();

    /**
     * Returns one of the network interface addresses that are not loopback addresses.
     */
    static Address localNetworkInterface(duint16 port = 0);

private:
    DE_PRIVATE(d)
};

DE_PUBLIC std::ostream &operator << (std::ostream &os, const Address &address);

} // namespace de

#endif // LIBCORE_ADDRESS_H
