/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "../sdl.h"

#include <cstring>
#include <string>
#include <sstream>

using namespace de;

Address::Address(duint32 ip, duint16 port) : ip_(ip), port_(port)
{}

Address::Address(const std::string& address, duint16 port)
{
    set(address, port);
}

bool Address::operator == (const Address& other) const
{
    return ip_ == other.ip_ && port_ == other.port_;
}

void Address::set(const std::string& address, duint16 port)
{
    using std::string;
    using std::istringstream;

    string hostName;

    string::size_type pos = address.find(':');
    if(pos != string::npos)
    {
        hostName = address.substr(0, pos); 

        // Use the port number that has been specified in the
        // address string.
        istringstream(address.substr(pos + 1)) >> port;
    }
    else
    {
        // We'll assume the entire address string is just a host
        // name.
        hostName = address;
    }
    
    // Use SDL_net to resolve the name.
    IPaddress resolved;
    if(SDLNet_ResolveHost(&resolved, !hostName.empty()? hostName.c_str() : 0, port) < 0)
    {
        throw ResolveError("Address::set", SDLNet_GetError());
    }

    ip_   = SDLNet_Read32(&resolved.host);
    port_ = SDLNet_Read16(&resolved.port);
}

bool Address::matches(const Address& other, duint32 mask)
{
    return (ip_ & mask) == (other.ip_ & mask);
}

std::string Address::asText() const
{
    std::ostringstream os;
    os << *this;
    return os.str();
}

std::ostream& de::operator << (std::ostream& os, const Address& address)
{
    duint part[4] = {
        (address.ip() >> 24) & 0xff,
        (address.ip() >> 16) & 0xff,
        (address.ip() >> 8) & 0xff,
        address.ip() & 0xff
    };
    os << part[0] << "." << part[1] << "." << part[2] << "." << part[3] << ":" << duint(address.port());
    return os;
}
