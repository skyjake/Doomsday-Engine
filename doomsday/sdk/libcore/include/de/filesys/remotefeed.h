/** @file remotefeed.h  Feed for remote files.
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

#ifndef LIBDENG2_REMOTEFEED_H
#define LIBDENG2_REMOTEFEED_H

#include "../Feed"
#include "../Address"

namespace de {

/**
 * Feed that communicates with a remote backend and populates local placeholders for
 * remote file data.
 */
class DENG2_PUBLIC RemoteFeed : public Feed
{
public:
    RemoteFeed(Address const &backend, String const &remotePath = String("/"));

    Address backend() const;

    String description() const;
    PopulatedFiles populate(Folder const &folder);
    bool prune(File &file) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_REMOTEFEED_H
