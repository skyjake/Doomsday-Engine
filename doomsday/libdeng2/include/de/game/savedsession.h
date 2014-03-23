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
 * Expands upon the services provided by the base class, adding various convenience methods
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
         * Parses metadata in Info syntax from @a source.
         */
        void parse(String const &source);

        /**
         * Generates a textual representation of the session metadata with Info syntax.
         */
        String asTextWithInfoSyntax() const;
    };

    /**
     * Abstract base class for serialized, map state readers.
     */
    class DENG2_PUBLIC MapStateReader
    {
    public:
        /// Base class for read errors. @ingroup errors
        DENG2_ERROR(ReadError);

    public:
        /**
         * Construct a new MapStateReader for the given saved @a session.
         *
         * @param session  The saved session to be read.
         */
        MapStateReader(SavedSession const &session);
        virtual ~MapStateReader();

        /**
         * Returns the saved session being read.
         */
        SavedSession const &session() const;

        /**
         * Attempt to load (read/interpret) the serialized map state.
         *
         * @param mapUriStr  Unique identifier of the map state to deserialize.
         */
        virtual void read(String const &mapUriStr) = 0;

    private:
        DENG2_PRIVATE(d)
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
     * Checks whether the saved session contains state data on the specified @a path.
     *
     * @param path  Of the state data to check for. Not case sensitive.
     */
    inline bool hasState(String const &path) const {
        if(path.isEmpty()) return false;
        return has(stateFileName(path));
    }

    /**
     * Locates a state data file in this saved session, or in one of its subfolders.
     * Looks recursively through subfolders.
     *
     * @param path  Path to look for. Relative to the root.
     *
     * @return  The located file, or @c NULL if the path was not found.
     */
    inline File *tryLocateStateFile(String const &path) const {
        if(path.isEmpty()) return false;
        return tryLocateFile(stateFileName(path));
    }

    template <typename Type>
    Type *tryLocateState(String const &path) const {
        return dynamic_cast<Type *>(tryLocateStateFile(path));
    }

    /// @todo remove me
    inline String repoPath() const {
        return parent()->name() / name().fileNameWithoutExtension();
    }

public:
    /**
     * Utility for composing the full name of a state data file in the saved session.
     *
     * @param name  Symbolic name of the state data.
     */
    static String stateFileName(String const &name);

private:
    DENG2_PRIVATE(d)
};

typedef SavedSession::Metadata SessionMetadata;

} // namespace game
} // namespace de

#endif // LIBDENG2_SAVEDSESSION_H
