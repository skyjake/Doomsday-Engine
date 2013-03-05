/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Address"
#include "de/String"

#include <QHostInfo>

namespace de {

struct Address::Instance
{
    QHostAddress host;
    duint16 port;

    Instance() : port(0) {}
};

Address::Address() : d(new Instance)
{}

Address::Address(char const *address, duint16 port) : d(new Instance)
{
    d->port = port;

    if(QLatin1String(address) == "localhost")
    {
        d->host = QHostAddress(QHostAddress::LocalHost);
    }
    else
    {
        d->host = QHostAddress(address);
    }
}

Address::Address(QHostAddress const &host, duint16 port) : d(new Instance)
{
    d->host = host;
    d->port = port;
}

Address::Address(Address const &other) : LogEntry::Arg::Base(), d(new Instance)
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
    quint32 const a = d->host.toIPv4Address();
    quint32 const b = other.d->host.toIPv4Address();
    if(a == b)
        return d->port < other.d->port;
    else
        return a < b;
}

bool Address::operator == (Address const &other) const
{
    return d->host == other.d->host && d->port == other.d->port;
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
    d->host = host;
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
    String result = (d->host == QHostAddress::LocalHost? "localhost" : d->host.toString());
    if(d->port)
    {
        result += ":" + QString::number(d->port);
    }
    return result;
}

Address Address::parse(String const &addressWithOptionalPort, duint16 defaultPort)
{
    duint16 port = defaultPort;
    String str = addressWithOptionalPort;
    if(str.contains(':'))
    {
        int pos = str.indexOf(':');
        port = str.mid(pos + 1).toInt();
        str = str.left(pos);
    }
    return Address(str.toAscii(), port);
}

QTextStream &operator << (QTextStream &os, Address const &address)
{
    os << address.asText();
    return os;
}

bool Address::isHostLocal(QHostAddress const &host) // static
{
    if(host == QHostAddress::LocalHost) return true;

    QHostInfo info = QHostInfo::fromName(QHostInfo::localHostName());
    foreach(QHostAddress addr, info.addresses())
    {
        if(addr == host) return true;
    }
    return false;
}

} // namespace de

