/** @file savedsession.h  Saved (game) session.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_SAVEDSESSION_H
#define LIBDENG2_SAVEDSESSION_H

#include "../Error"
#include "../Observers"
#include "../PackageFolder"
#include "../Record"
#include "../String"

namespace de {
namespace game {

class MapStateReader;

/**
 * Specialized PackageFolder that hosts a serialized game session.
 *
 * Expands upon the services provided by the base class adding various convenience methods
 * for inspecting the data within.
 *
 * @ingroup game
 */
class DENG2_PUBLIC SavedSession : public PackageFolder
{
public:
    /// Notified whenever the cached metadata of the saved session changes.
    DENG2_DEFINE_AUDIENCE2(MetadataChange, void savedSessionMetadataChanged(SavedSession &session))

    /**
     * Session metadata.
     */
    class DENG2_PUBLIC Metadata : public Record
    {
    public:
        /**
         * Parses metadata in Info syntax from a text string.
         *
         * @param source  Source string to be parsed.
         */
        void parse(String const &source);

        /**
         * Generates a textual representation of the session metadata with Info syntax.
         */
        String asTextWithInfoSyntax() const;
    };

public:
    SavedSession(File &sourceArchiveFile, String const &name = "");

    virtual ~SavedSession();

    /**
     * Composes a human-friendly, styled, textual description of the saved session that
     * is suitable for use in user facing contexts (e.g., GUI widgets).
     */
    String styledDescription() const;

    /**
     * Re-read the metadata for the saved session from the package and cache it.
     */
    void readMetadata();

    /**
     * Provides read-only access to a copy of the deserialized session metadata.
     */
    Metadata const &metadata() const;

    /**
     * Update the cached metadata with @a copied. Note that this will @em not alter the
     * package itself and only affects the local cache. The MetadataChange audience is
     * notified.
     *
     * @param copied  Replacement Metadata. A copy is made.
     */
    void cacheMetadata(Metadata const &copied);

    /**
     * Convenient method of determining whether the saved session contains serialized state
     * data for the specified map.
     *
     * @param mapUri  Unique identifier of the map to look up state data for.
     */
    bool hasMapState(String mapUriStr) const;

    /// @todo remove me
    inline String repoPath() const {
        return parent()->name() / name().fileNameWithoutExtension();
    }

private:
    DENG2_PRIVATE(d)
};

typedef SavedSession::Metadata SessionMetadata;

} // namespace game
} // namespace de

#endif // LIBDENG2_SAVEDSESSION_H
