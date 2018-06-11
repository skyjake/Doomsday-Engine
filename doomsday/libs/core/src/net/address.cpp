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

#include "de/Address"
#include "de/String"
#include "de/RegExp"
#include "de/CString"
#include "../src/net/networkinterfaces.h"

#include <c_plus/address.h>

namespace de {

DE_PIMPL_NOREF(Address)
{
    cplus::ref<iAddress> addr;
    duint16 port = 0;
    String  textRepr;

    enum Special { Undefined, LocalHost, RemoteHost };
    Special special = Undefined;

    void clearCached()
    {
        textRepr.clear();
        special = Impl::Undefined;
    }
};

Address::Address() : d(new Impl)
{}

Address::Address(char const *address, duint16 port) : d(new Impl)
{
    d->port = port;
    if (CString(address) == "localhost") // special case
    {
        d->addr.reset(newLocalhost_Address(6));
        d->special = Impl::LocalHost;
    }
    else
    {
        d->addr.reset(new_Address());
        lookupHostCStr_Address(d->addr, address, 0);
    }
}

Address::Address(const iAddress *address) : d(new Impl)
{
    d->addr.reset(address);
}

Address Address::take(iAddress *addr)
{
    Address a;
    a.d->addr.reset(addr);
    return a;
}

Address::Address(const Address &other) : d(new Impl)
{
    d->addr.reset(other.d->addr);
    d->port = other.d->port;
}

Address::operator const iAddress *() const
{
    return d->addr;
}

Address::Address(Address const &other)
{
    d->addr.reset(other.d->addr);
}

Address &Address::operator=(Address const &other)
{
    d->addr.reset(copy_Address(other.d->addr));
    d->port     = other.d->port;
    d->textRepr = other.d->textRepr;
    d->special  = other.d->special;
    return *this;
}

bool Address::operator<(Address const &other) const
{
    return asText() < other.asText();
}

bool Address::operator==(Address const &other) const
{
    return asText() == other.asText();
}

String Address::hostName() const
{
    return String(hostName_Address(d->addr));
}

bool Address::isNull() const
{
    return !d->addr;
}

bool Address::isLocal() const
{
    if (d->special == Impl::Undefined)
    {
        d->special = isHostLocal(*d->host) ? Impl::LocalHost : Impl::RemoteHost;
    }
    return (d->special == Impl::LocalHost);
}

duint16 Address::port() const
{
    return d->port;
}

void Address::setPort(duint16 p)
{
    d->clearCached();
    d->port = p;
}

//bool Address::matches(Address const &other, duint32 mask)
//{
//    return (d->host.toIPv4Address() & mask) == (other.d->host.toIPv4Address() & mask);
//}

String Address::asText() const
{
    if (!d->textRepr)
    {
        d->textRepr = (isLocal()? String("localhost") : String::take(toString_Address(d->addr)));
        if (d->port)
        {
            d->textRepr += Stringf(":%u", d->port);
        }
    }
    return d->textRepr;
}

Address Address::parse(String const &addressWithOptionalPort, duint16 defaultPort) // static
{
    duint16 port = defaultPort;
    String  str  = addressWithOptionalPort;

    // Let's see if there is a port number included (address is inside brackets).
    static const RegExp ipPortRegex
            ("^\\[(localhost|::1|(::ffff:)?[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+|[:A-Fa-f0-9]+[A-Fa-f0-9])\\]:([0-9]+)$");
    //qDebug() << "matching:" << addressWithOptionalPort;
    RegExpMatch match;
    if (ipPortRegex.match(addressWithOptionalPort, match))
    {
        //qDebug() << match;
        str  = match.captured(1);
        port = duint16(match.captured(3).toInt());
    }
    else
    {
        //qDebug() << "no match!";
    }
    return Address(str, port);
}

std::ostream &operator<<(std::ostream &os, Address const &address)
{
    os << address.asText().c_str();
    return os;
}

bool Address::isHostLocal(const Address &host) // static
{
//    if (host.isLoopback()) return true;

//    QHostAddress const hostv6(host.toIPv6Address());
    for (const Address &addr : internal::NetworkInterfaces::get().allAddresses())
    {
        if (addr == host) return true;
    }
    return false;
}

} // namespace de

