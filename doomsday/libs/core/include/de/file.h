/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_FILE_H
#define LIBCORE_FILE_H

#include "de/observers.h"
#include "de/iiostream.h"
#include "de/nativepath.h"
#include "de/record.h"
#include "de/string.h"
#include "de/time.h"
#include "de/scripting/iobject.h"
#include "de/filesys/node.h"

namespace de {

class FileSystem;
class Folder;
class Feed;

/**
 * Base class for all files stored in the file system. @ingroup fs
 *
 * Implements the IIOStream interface to allow files to receive and send out a stream of
 * bytes. The default implementation only throws an exception -- it is up to subclasses
 * to implement the stream in the context of the concrete file class.
 *
 * Note that the constructor of File is protected: only subclasses can be instantiated.
 *
 * @par Reading and writing
 *
 * The File class provides a stream-based interface for reading and writing the (entire)
 * contents of the file. Subclasses may provide a more fine-grained or random access
 * interface (e.g., ByteArrayFile).
 *
 * As a rule, newly created files are in write mode, because the assumption is that after
 * creation the next step is to write some content into the file. After all the content
 * has been written, the file should be put to read-only mode. This ensures that no
 * unwanted or accidental writes will occur, and that everybody can access the contents
 * of the file without needing to worry about the content changing. Also, subclasses may
 * depend on this for releasing some internal resources (like a native file write
 * handle).
 *
 * @par Deriving from File
 *
 * Subclasses have some special requirements for their destructors:
 * - deindex() must be called in all subclass destructors so that the instances indexed
 *   under the subclasses' type are removed from the file system's index, too.
 * - The file must be automatically flushed before it gets destroyed (see flush()).
 * - The deletion audience must be notified and @c audienceForDeletion must be cleared
 *   afterwards.
 *
 * Note that classes derived from Folder are subject to the same rules.
 *
 * @par Thread-safety
 *
 * All files are Lockable so that multiple threads can use them simultaneously. As a
 * general rule, the user of a file does not need to lock the file manually; files will
 * lock themselves as appropriate. A user may lock the file manually if long-term
 * exclusive access is required.
 */
class DE_PUBLIC File : public filesys::Node, public IIOStream, public IObject
{
public:
    // Mode flags.
    enum Flag
    {
        ReadOnly  = 0,
        Write     = 0x1,
        DontPrune = 0x4, ///< File should never be pruned. Used for files we create ourselves.
    };

    /// Type of file.
    enum class Type
    {
        File,
        Folder
    };

    /**
     * The file object is about to be deleted. This may be, e.g., due to pruning or
     * because the parent is being deleted.
     *
     * @param file  The file object being deleted.
     */
    DE_AUDIENCE(Deletion, void fileBeingDeleted(const File &file))

    /**
     * Stores the status of a file (size, time of last modification).
     */
    class Status
    {
    public:
        Status(dsize s = 0, const Time &modTime = Time::invalidTime())
            : size(s), modifiedAt(modTime), _type(Type::File) {}

        Status(Type t, dsize s = 0, const Time &modTime = Time::invalidTime())
            : size(s), modifiedAt(modTime), _type(t) {}

        Type type() const { return _type; }

        bool operator == (const Status &s) const {
            return size == s.size && modifiedAt == s.modifiedAt;
        }

        bool operator != (const Status &s) const { return !(*this == s); }

    public:
        dsize size;
        Time modifiedAt;

    private:
        Type _type;
    };

public:
    /**
     * When destroyed, a file is automatically removed from its parent folder
     * and deindexed from the file system.
     *
     * The source file of this file will be destroyed also.
     *
     * @note  Subclasses must call deindex() in their destructors so that
     *        the instances indexed under the subclasses' type are removed
     *        from the index also. Flushing should also be done automatically
     *        as necessary in the subclass.
     */
    virtual ~File();

    /**
     * Remove this file from its file system's index.
     */
    virtual void deindex();

    /**
     * Commits any buffered changes to the content of the file. All subclasses of File
     * must make sure they flush themselves right before they get deleted. Subclasses
     * must also flush themselves when a file in write mode is changed to read-only mode,
     * if necessary.
     */
    virtual void flush();

    /**
     * Empties the contents of the file.
     */
    virtual void clear();

    /// Returns a reference to the application's file system.
    static FileSystem &fileSystem();

    /// Returns the parent folder.
    Folder *parent() const;

    /**
     * Returns a textual description of the file, intended only for humans.
     * This attempts to fully describe the file, taking into consideration
     * the file's type and possible source.
     *
     * @param verbosity  Level of verbosity:
     *                    - Level 1: include file path
     *                    - Level 2: include origin feed description
     *                   Use -1 for autodetection (depends on current log entry level).
     *
     * @return Full human-friendly description of the file.
     */
    String description(int verbosity = -1) const;

    /**
     * Returns a textual description of this file only. Subclasses must
     * override this method to provide a description relevant to the
     * subclass.
     *
     * @return Human-friendly description of this file only.
     */
    virtual String describe() const;

    /**
     * Sets the origin Feed of the File. The origin feed is the feed that is able
     * to singlehandedly decide whether the File needs to be pruned. Typically
     * this is the Feed that generated the File.
     *
     * @note  Folder instances should not have an origin feed as the folder may
     *        be shared by many feeds.
     *
     * @param feed  The origin feed.
     */
    void setOriginFeed(Feed *feed);

    /**
     * Returns the origin Feed of the File.
     * @see setOriginFeed()
     */
    Feed *originFeed() const;

    /**
     * Sets the source file of this file. The source is where this file is
     * getting its data from. File interpreters use this to access their
     * uninterpreted data. By default all files use themselves as the
     * source, so there is always a valid source for every file. If another
     * file is being used as the source, the source is not typically
     * indexed to the file system.
     *
     * @param source  Source file. The file takes ownership of @a source.
     */
    void setSource(File *source);

    /**
     * Returns the source file.
     *
     * @return  Source file. Always returns a valid pointer.
     * @see setSource()
     */
    const File *source() const;

    /**
     * Returns the source file.
     *
     * @return  Source file. Always returns a valid pointer.
     * @see setSource()
     */
    File *source();

    /**
     * Returns the target of a symbolic link. For most files this is just returns
     * a reference to the file itself.
     *
     * @return Target file.
     *
     * @see LinkFile
     */
    virtual File &target();

    /// @copydoc target()
    virtual const File &target() const;

    /**
     * Updates the status of the file.
     *
     * @param status  New status.
     */
    virtual void setStatus(const Status &status);

    /**
     * Returns the status of the file.
     */
    const Status &status() const;

    inline File::Type type() const { return status().type(); }

    /**
     * Returns the size of the file.
     *
     * @return Size in bytes. If the file does not have a size (purely
     * stream-based file), the size is zero.
     */
    dsize size() const;

    /**
     * Generates a unique identifier generated based on the metadata of the file. This is
     * suitable for use in caches to identify a particular instance of a file.
     */
    virtual Block metaId() const;

    /**
     * Returns the mode of the file.
     */
    Flags mode() const;

    /**
     * Changes the mode of the file. For example, using
     * <code>Write|Truncate</code> as the mode would empty the contents of
     * the file and open it in writing mode.
     *
     * @param newMode  Mode.
     */
    virtual void setMode(const Flags &newMode);

    void setMode(Flags flags, FlagOpArg op);

    /**
     * Makes sure that the file has write access.
     */
    void verifyWriteAccess();

    /**
     * Reinterprets the file. If there is a known interpretation for the file
     * contents, the interpreter will replace this file in the folder. If the
     * file is already interpreted, the previous interpreter is deleted and the
     * original source file is reinterpreted.
     *
     * If the file is in a folder, the folder takes ownership of the returned
     * interpreter. If the file does not have a parent, ownership of the
     * interpreter is given to the caller, while this file's ownership
     * transfers to the interpreter.
     *
     * Note that feeds have the responsibility to apply interpretation on the
     * files they produce (using FileSystem::interpret()).
     *
     * @return The new interpreter, or this file if left uninterpreted. See
     * above for ownership policy.
     */
    File *reinterpret();

    /**
     * Looks up the native file path that corresponds to this file, if such a
     * path exists at all.
     *
     * @return Native path. Empty if there is no corresponding native file or
     * directory.
     */
    NativePath correspondingNativePath() const;

    // Implements IIOStream.
    IOStream &operator << (const IByteArray &bytes);
    IIStream &operator >> (IByteArray &bytes);
    const IIStream &operator >> (IByteArray &bytes) const;

    // Implements IObject.
    Record &objectNamespace();
    const Record &objectNamespace() const;

    // Standard casting methods.
    DE_CAST_METHODS()

public:
    /**
     * Prints a list of files as text with status and mode information included
     * (cf. "ls -l").
     *
     * @param files  List of files to print.
     *
     * @return Text preformatted for fixed-width printing (padded with spaces).
     */
    static String fileListAsText(List<const File *> files);

protected:
    /**
     * Constructs a new file. By default files are in read-only mode.
     *
     * @param name  Name of the file.
     */
    explicit File(const String &name = String());

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_FILE_H */
