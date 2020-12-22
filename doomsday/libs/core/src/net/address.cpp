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

#include "de/address.h"
#include "de/string.h"
#include "de/regexp.h"
#include "de/cstring.h"
#include "../src/net/networkinterfaces.h"

#include <the_Foundation/address.h>

namespace de {

DE_PIMPL_NOREF(Address)
{
    String            _pendingLookup;
    tF::ref<iAddress> _addr;
    duint16           port = 0;
    String            textRepr;

    enum Special { Undefined, LocalHost, RemoteHost };
    Special special = Undefined;

    static bool isPredefinedLoopback(const String &host)
    {
        static const RegExp localRegex(
            "localhost|localhost.localdomain|::1|fe80::1|fe80::1%.*|127.0.0.1", CaseInsensitive);
        return localRegex.exactMatch(host);
    }

    void setPending(const String &pending)
    {
        _pendingLookup = pending;

        // Check for the known local addresses.
        if (isPredefinedLoopback(pending))
        {
            special = LocalHost;
        }
    }
    
    iAddress *get()
    {
        if (_pendingLookup)
        {
            _addr = tF::make_ref(new_Address());
            lookupTcp_Address(_addr, _pendingLookup, port);
            _pendingLookup.clear();
        }
        return _addr;
    }

    String dbgstr() const
    {
        return Stringf("Address %p: pending='%s' addr=%p (%s) port=%d repr='%s' spec=%s",
                       this,
                       _pendingLookup.c_str(),
                       static_cast<const iAddress *>(_addr),
                       _addr ? cstr_String(hostName_Address(_addr)) : "--",
                       port,
                       textRepr.c_str(),
                       special == Undefined ? "Undefined"
                                            : special == LocalHost ? "LocalHost" : "RemoteHost");
    }
};

Address::Address() : d(new Impl)
{}

Address::Address(const char *address, duint16 port) : d(new Impl)
{
    d->port = port;
    d->setPending(address);
}

Address::Address(const iAddress *address) : d(new Impl)
{
    d->_addr.reset(address);
    d->port = port_Address(address);
    d->special =
        Impl::isPredefinedLoopback(hostName_Address(address)) ? Impl::LocalHost : Impl::Undefined;
}

Address Address::take(iAddress *address)
{
    Address taker;
    taker.d->_addr.reset(address);
    taker.d->port = port_Address(address);
    taker.d->_pendingLookup.clear();
    taker.d->special =
        Impl::isPredefinedLoopback(hostName_Address(address)) ? Impl::LocalHost : Impl::Undefined;
    return taker;
}

Address::Address(const Address &other) : d(new Impl)
{
    d->_addr.reset(other.d->_addr); // reference the same object
    d->port           = other.d->port;
    d->_pendingLookup = other.d->_pendingLookup;
    d->textRepr       = other.d->textRepr;
    d->special        = other.d->special;
}

Address::operator const iAddress *() const
{
    return d->get();
}

Address &Address::operator=(const Address &other)
{
    d->_addr.reset(other.d->_addr); // reference the same object
    d->_pendingLookup = other.d->_pendingLookup;
    d->port           = other.d->port;
    d->textRepr       = other.d->textRepr;
    d->special        = other.d->special;
    return *this;
}

bool Address::operator<(const Address &other) const
{
    return asText() < other.asText();
}

bool Address::operator==(const Address &other) const
{
    if (d->port != other.d->port) return false;
    return equal_Address(d->get(), other.d->get());
}

bool Address::isNull() const
{
    return !bool(d->_addr);
}

String Address::hostName() const
{
    if (d->_pendingLookup) return d->_pendingLookup;
    return String(hostName_Address(d->get()));
}

bool Address::isLoopback() const
{
    if (d->special == Impl::LocalHost)
    {
        return true;
    }
    return Impl::isPredefinedLoopback(hostName());
}

bool Address::isLocal() const
{
    if (d->special == Impl::Undefined)
    {
        d->special = isHostLocal(*this) ? Impl::LocalHost : Impl::RemoteHost;
    }
    return (d->special == Impl::LocalHost);
}

duint16 Address::port() const
{
    return d->port;
}

//void Address::setPort(duint16 p)
//{
//    d->clearCached();
//    d->port = p;
//}

//bool Address::matches(const Address &other, duint32 mask)
//{
//    return (d->host.toIPv4Address() & mask) == (other.d->host.toIPv4Address() & mask);
//}

String Address::asText() const
{
    if (d->_pendingLookup)
    {
        String str = d->_pendingLookup;
        if (d->port)
        {
            str += Stringf(":%u", d->port);
        }
        return str;
    }
    if (isNull())
    {
        return {};
    }
    if (!d->textRepr)
    {
        d->textRepr = (d->special == Impl::LocalHost ? String("localhost")
                                                     : String::take(toString_Address(d->get())));
//        if (d->port)
//        {
//            d->textRepr += Stringf(":%u", d->port);
//        }
    }
    return d->textRepr;
}

Address Address::parse(const String &addressWithOptionalPort, duint16 defaultPort) // static
{
    duint16 port = defaultPort;
    String  str  = addressWithOptionalPort;

    // Let's see if there is a port number included (address is inside brackets).
    static const RegExp ipPortRegex
            (R"(^\[(localhost|::1|(::ffff:)?[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+|[:A-Fa-f0-9]+[A-Fa-f0-9])\]:([0-9]+)$)");
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

std::ostream &operator<<(std::ostream &os, const Address &address)
{
    os << address.asText().c_str();
    return os;
}

static String cleanIpv4IPAddress(const String &text)
{
    if (text.beginsWith("::ffff:"))
    {
        return text.substr(BytePos(7));
    }
    return text;
}

bool Address::isHostLocal(const Address &host) // static
{
    if (host.d->special == Impl::LocalHost)
    {
        return true;
    }
    else if (host.d->special == Impl::RemoteHost)
    {
        return false;
    }
    if (host.d->_pendingLookup)
    {
        DE_ASSERT(host.isNull());
        // Will have to look up the IP address now.
        host.d->get();
    }
//    debug("isHostLocal %s?", host.d->dbgstr().c_str());
    const String test = cleanIpv4IPAddress(host.hostName());
    for (const Address &addr : internal::NetworkInterfaces::get().allAddresses())
    {
//        debug("- checking: %s", addr.d->dbgstr().c_str());
        if (cleanIpv4IPAddress(addr.hostName()).compareWithoutCase(test) == 0)
        {
            if (host.d->special == Impl::Undefined)
            {
                // Now we know.
                host.d->special = Impl::LocalHost;
            }
//            debug("  YES");
            return true;
        }
    }
//    debug("  NO");
    return false;
}

List<Address> Address::localAddresses() // static
{
    return internal::NetworkInterfaces::get().allAddresses();
}

Address Address::localNetworkInterface(duint16 port) // static
{
    Address found;
    const auto addresses = internal::NetworkInterfaces::get().allAddresses();
    for (const Address &a : addresses)
    {
        // Exclude all loopback and link-local addresses.
        if (!a.isLoopback() && !a.hostName().beginsWith("fe80::"))
        {
            found = a;
            break;
        }
    }
    if (!port) return found;
    return Address(found.hostName(), port);
}

} // namespace de

