/** @file biasdigest.h Shadow Bias change digest.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_SHADOWBIAS_DIGEST_H
#define DENG_RENDER_SHADOWBIAS_DIGEST_H

#include <de/libdeng2.h>

/**
 * Change digest for updating trackers in the Shadow Bias lighting model.
 */
class BiasDigest
{
public:
    BiasDigest();

    /**
     * Return the digest to an empty state (clear all changes).
     */
    void reset();

    /**
     * Mark the identified bias source as having changed.
     */
    void markSourceChanged(uint index);

    /**
     * Returns @c true if the identified bias source is marked as changed.
     */
    bool isSourceChanged(uint index) const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_RENDER_SHADOWBIAS_DIGEST_H
