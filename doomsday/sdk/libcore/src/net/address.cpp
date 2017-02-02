/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "../src/net/networkinterfaces.h"

#include <QHostInfo>
#include <QRegularExpression>

namespace de {

DENG2_PIMPL_NOREF(Address)
{
    QHostAddress host;
    duint16 port;

    Impl() : port(0) {}
};

Address::Address() : d(new Impl)
{}

Address::Address(char const *address, duint16 port) : d(new Impl)
{
    d->port = port;

    if (QLatin1String(address) == "localhost") // special case
    {
        d->host = QHostAddress(QHostAddress::LocalHostIPv6);
    }
    else
    {
        d->host = QHostAddress(QHostAddress(address).toIPv6Address());
    }
}

Address::Address(QHostAddress const &host, duint16 port) : d(new Impl)
{
    d->host = QHostAddress(host.toIPv6Address());
    d->port = port;
}

Address::Address(Address const &other) : LogEntry::Arg::Base(), d(new Impl)
{
    d->host = other.d->host;
    d->port = other.d->port;
}

Address &Address::operator = (Address const &other)
{
    d->host = other.d->host;
    d->port = other.d->port;
    return *this;
}

bool Address::operator < (Address const &other) const
{
    return asText() < other.asText();
}

bool Address::operator == (Address const &other) const
{
    if (d->port != other.d->port) return false;
    return (isLocal() && other.isLocal()) || (d->host == other.d->host);
}

bool Address::isNull() const
{
    return d->host.isNull();
}

QHostAddress const &Address::host() const
{
    return d->host;
}

void Address::setHost(QHostAddress const &host)
{
    d->host = QHostAddress(host.toIPv6Address());
}

bool Address::isLocal() const
{
    return isHostLocal(d->host);
}

duint16 Address::port() const
{
    return d->port;
}

void Address::setPort(duint16 p)
{
    d->port = p;
}

bool Address::matches(Address const &other, duint32 mask)
{
    return (d->host.toIPv4Address() & mask) == (other.d->host.toIPv4Address() & mask);
}

String Address::asText() const
{
    String result = (isLocal()? String("localhost") : d->host.toString());
    if (d->port)
    {
        result += ":" + QString::number(d->port);
    }
    return result;
}

Address Address::parse(String const &addressWithOptionalPort, duint16 defaultPort) // static
{
    duint16 port = defaultPort;
    String str = addressWithOptionalPort;
    /*int portPosMin = 1;
    if (str.beginsWith(QStringLiteral("::ffff:")))
    {
        // IPv4 address.
        portPosMin = 8;
    }
    int pos = str.lastIndexOf(':');
    if (pos >= portPosMin)
    {
        port = duint16(str.mid(pos + 1).toInt());
        str = str.left(pos);
    }*/
    // Let's see if there is a port number included.
    static QRegularExpression const ipPortRegex
            ("^(localhost|::1|(::ffff:)?[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+|[:A-Fa-f0-9]+[A-Fa-f0-9]):([0-9]+)$");
    //qDebug() << "matching:" << addressWithOptionalPort;
    auto match = ipPortRegex.match(addressWithOptionalPort);
    if (match.hasMatch())
    {
        //qDebug() << match;
        str  = match.captured(1);
        port = duint16(match.captured(3).toInt());
    }
    else
    {
        //qDebug() << "no match!";
    }
    return Address(str.toLatin1(), port);
}

QTextStream &operator << (QTextStream &os, Address const &address)
{
    os << address.asText();
    return os;
}

bool Address::isHostLocal(QHostAddress const &host) // static
{
    if (host.isLoopback()) return true;

    QHostAddress const hostv6(host.toIPv6Address());
    foreach (QHostAddress addr, internal::NetworkInterfaces::get().allAddresses())
    {
        if (addr == hostv6)
            return true;
    }
    return false;
}

} // namespace de

