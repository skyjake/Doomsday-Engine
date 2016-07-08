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
#include <de/Version>

namespace res {

/**
 * Index for data bundles.
 *
 * Parses the "databundles.dei" Info file that specifies identification
 * criteria for known data files. The res::Bundles::match() method can can
 * called on an arbitary data bundle to try finding a matching record in the
 * registry of known data files.
 *
 * @see res::DataBundle
 */
class LIBDOOMSDAY_PUBLIC Bundles
{
public:
    typedef QList<de::Info::BlockElement const *> BlockElements;

    /// Notified when a data bundle refresh/identification has been completed.
    DENG2_DEFINE_AUDIENCE2(Identify, void dataBundlesIdentified())

    DENG2_ERROR(InvalidError);

    struct LIBDOOMSDAY_PUBLIC MatchResult
    {
        de::Info::BlockElement const *bestMatch = nullptr;
        de::dint bestScore = 0;
        de::String packageId;
        de::Version packageVersion = de::Version("");

        operator bool() const { return bestMatch != nullptr; }
    };

public:
    Bundles();

    /**
     * Returns the collection of information for identifying known data files.
     * @return Info document.
     */
    de::Info const &identityRegistry() const;

    BlockElements formatEntries(DataBundle::Format format) const;

    /**
     * Tries to identify of the data files that have been indexed since the
     * previous call of this method. Recognized data files are linked as
     * packages under the /sys/bundles folder.
     *
     * Calling this starts a background task where the identification is performed.
     * The method returns immediately. The Identify audience is notified once the
     * task is finished.
     *
     * @see res::DataBundle::identifyPackages()
     */
    void identify();

    bool isEverythingIdentified() const;

    /**
     * Finds a matching entry in the registry for a given data bundle.
     * @param bundle  Data bundle whose information to look for.
     *
     * @return Best match.
     */
    MatchResult match(DataBundle const &bundle) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_BUNDLES_H
