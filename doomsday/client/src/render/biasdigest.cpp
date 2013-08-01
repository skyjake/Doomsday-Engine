/** @file biasdigest.h Shadow Bias change digest.
 *
 * @authors Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "world/map.h"

#include "render/biasdigest.h"

using namespace de;

static int const FIELDSIZE = (Map::MAX_BIAS_SOURCES / 8);

DENG2_PIMPL_NOREF(BiasDigest)
{
    uint changes[FIELDSIZE];

    Instance()
    {
        de::zap(changes);
    }
};

BiasDigest::BiasDigest() : d(new Instance())
{}

void BiasDigest::reset()
{
    zap(d->changes);
}

void BiasDigest::markSourceChanged(uint index)
{
    // Assume 32-bit uint.
    d->changes[index >> 5] |= (1 << (index & 0x1f));
}

bool BiasDigest::sourceMarkedChanged(uint index) const
{
    // Assume 32-bit uint.
    return (d->changes[index >> 5] & (1 << (index & 0x1f))) != 0;
}
