/**
 * @mainpage The Doomsday Engine Project: libdeng2
 *
 * Copyright &copy; 2004-2009 Jaakko Ker&auml;nen <jaakko.keranen@iki.fi>
 *
 * <p>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but <b>without any warranty</b>; without even the implied warranty of
 * <b>merchantability</b> or <b>fitness for a particular purpose</b>.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/
 *
 * <h2>Table of Contents</h2>
 * 
 * <h3>Main Modules</h3>
 * - @ref core
 * - @ref fs
 * - @ref net
 * - @ref protocol
 * - @ref script
 * - @ref video
 * - @ref audio
 * - @ref world
 * 
 * <h3>Supporting Modules</h3>
 * - @ref types
 * - @ref data
 * - @ref errors
 */

#include "internal.h"

namespace de
{
    void internal::convertAddress(const Address& address, IPaddress* ip)
    {
        SDLNet_Write32(address.ip(), &ip->host);
        SDLNet_Write16(address.port(), &ip->port);
    }
    
    Address internal::convertAddress(const IPaddress* ip)
    {
        duint32 host;
        SDLNet_Write32(ip->host, &host);
        duint16 port;
        SDLNet_Write16(ip->port, &port);
        
        return Address(host, port);
    }

    SDL_Surface* internal::createSDLSurface(duint flags, duint width, duint height, duint bitsPerPixel)
    {
        return SDL_CreateRGBSurface(flags, width, height, bitsPerPixel, 0, 0, 0, 0);
    }
}
