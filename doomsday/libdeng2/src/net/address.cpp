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

using namespace de;

Address::Address() : _port(0)
{}

Address::Address(char const *address, duint16 port)
    : _port(port)
{
    if(QLatin1String(address) == "localhost")
    {
        _host = QHostAddress(QHostAddress::LocalHost);
    }
    else
    {
        _host = QHostAddress(address);
    }
}

Address::Address(QHostAddress const &host, duint16 port)
    : _host(host), _port(port)
{}

Address::Address(String const &addressWithOptionalPort) : _port(0)
{
    String str = addressWithOptionalPort;
    if(str.contains(':'))
    {
        int pos = str.indexOf(':');
        _port = str.mid(pos + 1).toInt();
        str = str.left(pos);
    }
    if(str == "localhost")
    {
        _host = QHostAddress(QHostAddress::LocalHost);
    }
    else
    {
        _host = QHostAddress(str);
    }
}

Address::Address(Address const &other)
    : LogEntry::Arg::Base(), _host(other._host), _port(other._port)
{}

bool Address::operator == (Address const &other) const
{
    return _host == other._host && _port == other._port;
}

bool Address::matches(Address const &other, duint32 mask)
{
    return (_host.toIPv4Address() & mask) == (other._host.toIPv4Address() & mask);
}

String Address::asText() const
{
    String result = (_host == QHostAddress::LocalHost? "localhost" : _host.toString());
    if(_port)
    {
        result += ":" + QString::number(_port);
    }
    return result;
}

QTextStream &de::operator << (QTextStream &os, Address const &address)
{
    os << address.asText();
    return os;
}
