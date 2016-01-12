/** @file bundles.h   Data bundle indexing.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_RESOURCE_BUNDLES_H
#define LIBDOOMSDAY_RESOURCE_BUNDLES_H

#include "databundle.h"
#include <de/Info>

namespace res {

/**
 * Index for data bundles.
 */
class Bundles
{
public:
    typedef QList<de::Info::BlockElement const *> BlockElements;

    DENG2_ERROR(InvalidError);

public:
    Bundles();

    /**
     * Returns the collection of information for identifying known data files.
     * @return Info document.
     */
    de::Info const &identityRegistry() const;

    BlockElements formatEntries(DataBundle::Format format) const;

    void identify();

private:
    DENG2_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_BUNDLES_H
