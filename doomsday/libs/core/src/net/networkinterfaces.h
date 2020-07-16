/** @file networkinterfaces.h  Information about network interfaces.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_NETWORKINTERFACES_H
#define LIBCORE_NETWORKINTERFACES_H

#include "de/libcore.h"
#include "de/list.h"
#include "de/address.h"

namespace de {
namespace internal {

/**
 * Information abuot network interfaces.
 *
 * Maintains a quickly-accessible copy of the network interface data.
 */
class NetworkInterfaces
{
public:
    static NetworkInterfaces &get();

public:
    NetworkInterfaces();

    /**
     * Returns a cached copy of the list of network addresses for all of the currently
     * available network interfaces. Updated periodically in the background.
     *
     * @return Network interface addresses, in IPv6 format.
     */
    List<Address> allAddresses() const;

private:
    DE_PRIVATE(d)
};

} // namespace internal
} // namespace de

#endif // LIBCORE_NETWORKINTERFACES_H
