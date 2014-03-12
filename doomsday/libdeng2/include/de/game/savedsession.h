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
#include "../Path"
#include "../Record"
#include "../game/SavedSessionRepository"
#include "../String"

namespace de {
namespace game {

class MapStateReader;

/**
 * Logical component representing a serialized game state on disk.
 *
 * @ingroup game
 */
class DENG2_PUBLIC SavedSession
{
public:
    /// Notified whenever the status of the saved game session changes.
    DENG2_DEFINE_AUDIENCE2(StatusChange, void savedSessionStatusChanged(SavedSession &session))

    /// Notified whenever the metadata of the saved game session changes.
    DENG2_DEFINE_AUDIENCE2(MetadataChange, void savedSessionMetadataChanged(SavedSession &session))

    /// Required/referenced repository is missing. @ingroup errors
    DENG2_ERROR(MissingRepositoryError);

    /// The associated game map state file was missing/unrecognized. @ingroup errors
    DENG2_ERROR(UnrecognizedMapStateError);

    /// Logical session status:
    enum Status {
        Loadable,
        Incompatible,
        Unused
    };

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
         *
         * See the Doomsday Wiki for an example of the syntax:
         * http://dengine.net/dew/index.php?title=Info
         *
         * @todo Use a more generic Record => Info conversion logic.
         */
        String asTextWithInfoSyntax() const;
    };

public:
    SavedSession(String const &fileName = "");
    SavedSession(SavedSession const &other);

    SavedSession &operator = (SavedSession const &other);

    /**
     * Returns the name of the saved session file package (with extension).
     */
    String fileName() const;
    void setFileName(String newName);

    /**
     * Returns the logical status of the saved session. The StatusChange audience is notified
     * whenever the status changes.
     *
     * @see statusAsText()
     */
    Status status() const;

    /**
     * Returns a textual representation of the current status of the saved session.
     *
     * @see status()
     */
    String statusAsText() const;

    inline bool isLoadable() const     { return status() == Loadable; }
    inline bool isIncompatible() const { return status() == Incompatible; }
    inline bool isUnused() const       { return status() == Unused; }

    /**
     * Composes a human-friendly, styled, textual description of the saved session.
     */
    String description() const;

    /**
     * Returns the saved session repository which owns the saved session (if any).
     */
    SavedSessionRepository &repository() const;

    /**
     * Configure the saved session to use the @a newRepository. Once set, the saved session file
     * package (if present) is potentially @em loadable, @em updateable and @em removable.
     *
     * @param newRepository  New SavedSessionRepository to configure for.
     */
    void setRepository(SavedSessionRepository *newRepository);

    /**
     * Determines whether a file package exists for the saved session. However, it may not be
     * compatible with the current game session.
     *
     * @see isLoadable()
     */
    bool hasFile() const;

    /**
     * Determines whether a file package exists for the saved session and if so, reads the
     * session metadata. A repository must be configured for this.
     *
     * @return  @c true iff the session metadata was read successfully.
     *
     * @see setRepository()
     */
    bool recognizeFile();

    /**
     * Attempt to update the status of the saved session from the file package in the repository.
     * If the file path is invalid, unreachable, or the package is not recognized then the saved
     * session is returned to a valid but non-loadable state.
     *
     * @see setRepository(), isLoadable()
     */
    void updateFromFile();

    /**
     * Removes the file package for the saved session from the repository (if configured).
     *
     * @see setRepository()
     */
    void removeFile();

    /**
     * Composes the full path to the saved session file package in the repository (FYI).
     */
    inline Path filePath() const { return repository().folder().path() / fileName(); }

    /**
     * Determines whether a serialized map state exists for the saved session.
     *
     * @param mapUri  Unique map identifier.
     */
    bool hasMapState(String mapUriStr) const;

    /**
     * Determines whether a file package exists for the saved session and if so, reads the
     * session metadata and returns a new MapStateReader instance appropriate for deserializing
     * map state data. A repository must be configured for this.
     *
     * @return  New reader instance if recognized; otherwise @c 0. Ownership given to the caller.
     */
    std::auto_ptr<MapStateReader> mapStateReader();

    /**
     * Provides read-only access to a copy of the deserialized saved session metadata.
     */
    Metadata const &metadata() const;
    void replaceMetadata(Metadata *newMetadata);

private:
    DENG2_PRIVATE(d)
};

typedef SavedSession::Metadata SessionMetadata;

} // namespace game
} // namespace de

#endif // LIBDENG2_SAVEDSESSION_H
