/** @file gamestatefolder.h  Archived game state.
 *
 * @authors Copyright © 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_GAMESTATEFOLDER_H
#define LIBDOOMSDAY_GAMESTATEFOLDER_H

#include "libdoomsday.h"
#include "world/ithinkermapping.h"

#include <de/Error>
#include <de/Observers>
#include <de/ArchiveFolder>
#include <de/Record>
#include <de/String>
#include <de/filesys/IInterpreter>

/**
 * Specialized ArchiveFolder that hosts a serialized game session.
 *
 * Expands upon the services provided by the base class, adding various
 * convenience methods for inspecting the data within.
 *
 * @ingroup game
 */
class LIBDOOMSDAY_PUBLIC GameStateFolder : public de::ArchiveFolder
{
public:
    /// Notified whenever the cached metadata of the saved session changes.
    DE_AUDIENCE(MetadataChange, void gameStateFolderMetadataChanged(GameStateFolder &session))

    /**
     * Session metadata.
     */
    class LIBDOOMSDAY_PUBLIC Metadata : public de::Record
    {
    public:
        /**
         * Parses metadata in Info syntax from @a source.
         */
        void parse(de::String const &source);

        /**
         * Composes a human-friendly, styled, textual representation suitable for use in user
         * facing contexts (e.g., GUI widgets).
         */
        de::String asStyledText() const;

        /**
         * Generates a textual representation of the session metadata with Info syntax.
         */
        de::String asInfo() const;
    };

    /**
     * Abstract base class for serialized, map state readers.
     */
    class LIBDOOMSDAY_PUBLIC MapStateReader : public world::IThinkerMapping
    {
    public:
        /// Base class for read errors. @ingroup errors
        DE_ERROR(ReadError);

    public:
        /**
         * Construct a new MapStateReader for the given saved @a session.
         *
         * @param session  The saved session to be read.
         */
        MapStateReader(GameStateFolder const &session);
        virtual ~MapStateReader();

        /**
         * Returns the deserialized metadata for the saved session being read.
         */
        Metadata const &metadata() const;

        /**
         * Returns the root folder of the saved session being read.
         */
        Folder const &folder() const;

        /**
         * Attempt to load (read/interpret) the serialized map state.
         *
         * @param mapUriStr  Unique identifier of the map state to deserialize.
         */
        virtual void read(de::String const &mapUriStr) = 0;

    private:
        DE_PRIVATE(d)
    };

    /**
     * Constructs MapStateReaders for serialized map state interpretation.
     */
    class IMapStateReaderFactory
    {
    public:
        virtual ~IMapStateReaderFactory() = default;

        /**
         * Called while loading a saved session to acquire a MapStateReader for the
         * interpretation of a serialized map state format.
         *
         * @param session    Saved session to acquire a reader for.
         * @param mapUriStr  Unique identifier of the map state to be interpreted.
         *
         * @return  New MapStateReader appropriate for the serialized map state format
         * if recognized. Ownership is given to the caller.
         */
        virtual MapStateReader *makeMapStateReader(
                GameStateFolder const &session, de::String const &mapUriStr) = 0;
    };

public:
    GameStateFolder(File &sourceArchiveFile, de::String const &name = de::String());

    virtual ~GameStateFolder();

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
    inline bool hasState(de::String const &path) const {
        return has(stateFilePath(path));
    }

    /**
     * Locates a state data file in this saved session, or in one of its subfolders.
     * Looks recursively through subfolders.
     *
     * @param path  Path to look for. Relative to the root.
     *
     * @return  The located file, or @c NULL if the path was not found.
     */
    inline File *tryLocateStateFile(de::String const &path) const {
        return tryLocateFile(stateFilePath(path));
    }

    template <typename Type>
    Type *tryLocateState(de::String const &path) const {
        return tryLocate<Type>(stateFilePath(path));
    }

    /**
     * Locates a state data file in this saved or in one of its subfolders. Looks
     * recusively through subfolders.
     *
     * @param path  Path to look for. Relative to this folder.
     *
     * @return  The found file.
     */
    template <typename Type>
    Type &locateState(de::String const &path) const {
        return locate<Type>(stateFilePath(path));
    }

public:
    /**
     * Determines if a package affects gameplay and should therefore be included in
     * savegame and multiplayer metadata. Packages that alter gameplay or game objects
     * must be included, while purely visual content does not.
     *
     * @param packageId  Package identifier.
     *
     * @return @c true, if the package has gameplay-altering contents. @c false, if its
     * contents are only superficial.
     */
    static bool isPackageAffectingGameplay(de::String const &packageId);

    /**
     * Utility for composing the full path of a state data file in the saved session.
     *
     * @param path  Path to and symbolic name of the state data.
     */
    static de::String stateFilePath(de::String const &path);

    struct Interpreter : public de::filesys::IInterpreter {
        de::File *interpretFile(de::File *file) const;
    };

private:
    DE_PRIVATE(d)
};

typedef GameStateFolder::Metadata GameStateMetadata;

#endif // LIBDOOMSDAY_GAMESTATEFOLDER_H
