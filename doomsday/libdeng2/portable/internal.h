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

#ifndef LIBDENG2_INTERNAL_H
#define LIBDENG2_INTERNAL_H

#include "de/deng.h"
#include "de/Address"
#include "sdl.h"

/**
 * @namespace de::internal
 *
 * Internal utilities.  These are not visible to the user of libdeng2.
 * These are mostly for encapsulating dependencies.
 */
namespace de
{
	namespace internal
	{
		/// Convert an Address to SDL_net's IPaddress.
		void convertAddress(const Address& address, IPaddress* ip);

        /// Convert SDL_Net's IPaddress to an Address.
        Address convertAddress(const IPaddress* ip);

		/// Create an SDL surface.
        SDL_Surface* createSDLSurface(duint flags, duint width, duint height, duint bitsPerPixel);
	}
}

#endif /* LIBDENG2_INTERNAL_H */
