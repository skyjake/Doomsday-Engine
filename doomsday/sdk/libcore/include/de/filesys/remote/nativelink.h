/** @file remote/nativelink.h
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

#ifndef DENG2_FILESYS_NATIVELINK_H
#define DENG2_FILESYS_NATIVELINK_H

#include "link.h"

namespace de {
namespace filesys {

/**
 * Link to a native Doomsday remote repository (see RemoteFeedUser on server).
 */
class DENG2_PUBLIC NativeLink : public Link
{
public:
    NativeLink(String const &address);

protected:
    void wasConnected() override;
    void transmit(Query const &query) override;

private:
    DENG2_PRIVATE(d)
};

} // namespace filesys
} // namespace de

#endif // DENG2_FILESYS_NATIVELINK_H
