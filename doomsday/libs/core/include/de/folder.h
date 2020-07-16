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

#ifndef LIBCORE_FOLDER_H
#define LIBCORE_FOLDER_H

#include "de/libcore.h"
#include "de/file.h"
#include "de/list.h"
#include "de/keymap.h"

namespace de {

struct AsyncTask;
class Feed;

/**
 * A folder contains a set of files. It is used for building a tree of files
 * in the file system (de::FS). This is the base class for all types of folders.
 * @ingroup fs
 *
 * You should usually use the high-level API to manipulate files:
 * - createFile() to create a new file
 * - replaceFile() to create a file, always replacing the existing file
 * - destroyFile() to delete a file
 *
 * @par Feeds
 *
 * Feeds are responsible for populating the folder with files. You may attach any
 * number of feeds to the folder.
 *
 * The first Feed attached to a Folder is the primary feed.
 *
 * @par Deriving from Folder
 *
 * See the requirements that apply to deriving from File; the same apply for Folder,
 * as it is itself derived from File.
 *
 * @see Feed
 */
class DE_PUBLIC Folder : public File
{
public:
    /// A folder cannot contain two or more files with the same name. @ingroup errors
    DE_ERROR(DuplicateNameError);

    /// File path did not point to a file. @ingroup errors
    DE_ERROR(NotFoundError);

    /// Creating a new file was unsuccessful. @ingroup errors
    DE_ERROR(NewFileError);

    typedef List<Feed *> Feeds;
    typedef KeyMap<String, File *> Contents; // lower-cased

    enum PopulationBehavior {
        PopulateFullTree       = 0x1,   ///< The full tree is populated.
        PopulateOnlyThisFolder = 0x2,   ///< Do not descend into subfolders while populating.
        PopulateAsync          = 0x4,   ///< Do not block until complete.
        PopulateAsyncFullTree  = PopulateAsync | PopulateFullTree,

        DisableNotification = 0x1000, // internal use: population audience not notified
        DisableIndexing     = 0x2000, // internal use: file is not added to the FS index
    };
    using PopulationBehaviors = Flags;

    /// Behavior for creating new files.
    enum FileCreationBehavior {
        KeepExisting,       ///< Existing file will be kept unchanged (safe).
        ReplaceExisting     ///< Existing file will be replaced.
    };

public:
    explicit Folder(const String &name = {});

    virtual ~Folder();

    String describe() const;

    String describeFeeds() const;

    /**
     * Populates the folder with a set of File instances. Each feed attached to
     * the folder will contribute File instances constructed based on the
     * feeds' source data. Every populated file will also be added to the file
     * system's main index.
     *
     * Repopulation is nondestructive as long as the source data has not
     * changed. Population may be performed more than once during the lifetime
     * of the folder, for example when it's necessary to synchronize it with
     * the contents of a native hard drive directory.
     *
     * This should never be called "just in case"; only populate when you know
     * that the source data has changed and the file tree corresponding that
     * data needs to be updated to reflect the changes. Operations done using
     * File and Folder instances automatically keep the tree up to date even
     * when they apply changes to the source data, so population should not be
     * performed in that case.
     *
     * @param behavior  Behavior of the population operation, see
     *                  Folder::PopulationBehavior.
     */
    virtual void populate(PopulationBehaviors behavior = PopulateFullTree);

    /**
     * Provides read-only access to the content of the folder.
     */
    Contents contents() const;

    LoopResult forContents(const std::function<LoopResult (String name, File &file)> &func) const;

    List<Folder *> subfolders() const;

    /**
     * Unpopulates the folder, i.e., deletes all contained File objects.
     * The attached feeds are not notified, which means the source data they translate
     * remains untouched.
     *
     * Called automatically when the Folder instance is deleted.
     */
    void clear();

    /**
     * Creates a new file in the folder. The feeds attached to the folder will
     * decide what kind of file is actually created, and perform the
     * construction of the new File instance. The new file is added to the file
     * system's index.
     *
     * @param name      Name or path of the new file, relative to this folder.
     * @param behavior  How to treat existing files.
     *
     * @return  The created file (write mode enabled).
     */
    File &createFile(const String &name, FileCreationBehavior behavior = KeepExisting);

    /**
     * Creates a new file in the folder, replacing an existing file with the
     * same name. Same as calling <code>createFile(name, true)</code>.
     *
     * @param name  Name or path of the new file, relative to this folder.
     *
     * @return  The created file (write mode enabled).
     */
    File &replaceFile(const String &name);

    /**
     * Removes a file from a folder. The file will be deleted. If it has an
     * origin feed, the feed will be asked to remove the file as well, which
     * means it will be removed in the source data as well as the file tree.
     *
     * @param name  Name or path of file to remove, relative to this folder.
     */
    void destroyFile(const String &name);

    /**
     * Removes a file from a folder. The file will be deleted.  If it has an
     * origin feed, the feed will be asked to remove the file as well, which
     * means it will be removed in the source data as well as the file tree.
     *
     * @param name  Name of path of file to remove, relative to this folder.
     *
     * @return @c true, if the file was deleted. @c false, if it did not exist.
     */
    bool tryDestroyFile(const String &name);

    /**
     * Removes all files in the folder. The files will be deleted. If the files
     * have origin feeds, the feed will be asked to remove the files as well.
     * The folder remains locked during the entire operation.
     */
    void destroyAllFiles();

    /**
     * Removes all files in the folder and in all its subfolders. The subfolders
     * themselves are retained.
     */
    void destroyAllFilesRecursively();

    /**
     * Checks whether the folder contains a file.
     *
     * @param name  File to check for. The name is not case sensitive.
     */
    bool has(const String &name) const;

    inline bool contains(const String &name) const {
        return has(name);
    }

    /**
     * Adds an object to the folder. The object must be an instance of a class
     * derived from File.
     *
     * @param fileObject  Object to add to the folder. The folder takes
     *      ownership of this object. Cannot be NULL.
     *
     * @return  Reference to @a fileObject, for convenience.
     */
    template <typename Type>
    Type &add(Type *fileObject) {
        DE_ASSERT(fileObject != 0);
        add(static_cast<File *>(fileObject));
        return *fileObject;
    }

    /**
     * Adds a file instance to the contents of the folder.
     *
     * @param file  File to add. The folder takes ownership of this instance.
     *
     * @return  Reference to the file, for convenience.
     */
    virtual File &add(File *file);

    /**
     * Removes a file from the folder, by name. The file is not deleted. The
     * ownership of the file is given to the caller.
     *
     * @return  The removed file object. Ownership of the object is given to
     * the caller.
     */
    File *remove(const String &name);

    File *remove(const char *nameUtf8);

    template <typename Type>
    Type *remove(Type *fileObject) {
        DE_ASSERT(fileObject != 0);
        remove(*static_cast<File *>(fileObject));
        return fileObject;
    }

    /**
     * Removes a file from the folder. The file is not deleted. The ownership
     * of the file is given to the caller.
     *
     * @return  The removed file object. Ownership of the object is given to
     * the caller.
     */
    virtual File *remove(File &file);

    File *tryLocateFile(const String &path) const;

    template <typename Type>
    Type *tryLocate(const String &path) const {
        File *f = tryLocateFile(path);
        if (!f) return nullptr;
        if (auto *casted = dynamic_cast<Type *>(f)) {
            return casted;
        }
        if (&f->target() != f) {
            if (auto *casted = dynamic_cast<Type *>(&f->target())) {
                // Link target also accepted, if type matches.
                return casted;
            }
        }
        return nullptr;
    }

    /**
     * Locates a file in this folder or in one of its subfolders. Looks recusively
     * through subfolders.
     *
     * @param path  Path to look for. Relative to this folder.
     *
     * @return  The found file.
     */
    template <typename Type>
    Type &locate(const String &path) const {
        File *found = tryLocateFile(path);
        if (!found) {
            /// @throw NotFoundError  Path didn't exist.
            throw NotFoundError("Folder::locate", "\"" + path + "\" was not found "
                                "(in " + description() + ")");
        }
        if (Type *casted = dynamic_cast<Type *>(found)) {
            return *casted;
        }
        if (found != &found->target()) {
            if (Type *casted = dynamic_cast<Type *>(&found->target())) {
                return *casted;
            }
        }
        /// @throw NotFoundError  Found file could not be cast to the
        /// requested type.
        throw NotFoundError("Folder::locate",
                            found->description() + " has incompatible type; wanted " +
                                DE_TYPE_NAME(Type));
    }

    /**
     * Attach a feed to the folder. The feed will provide content for the
     * folder. The first feed attached to a folder is the primary feed.
     *
     * @param feed  Feed to attach to the folder. The folder gets ownership of
     *              the feed.
     */
    void attach(Feed *feed);

    /**
     * Detaches a feed from the folder. The feed object is not deleted.
     *
     * @param feed  Feed to detach from the folder.
     *
     * @return  Feed instance. Ownership is returned to the caller.
     */
    Feed *detach(Feed &feed);

    /**
     * Makes the specified feed the primary one.
     *
     * @param feed  Feed instance to make the primary feed. Must already be
     *              attached to the Folder.
     */
    void setPrimaryFeed(Feed &feed);

    /**
     * Returns the primary feed of the folder, if there is one.
     * @return Feed instance, or @c nullptr.
     */
    Feed *primaryFeed() const;

    template <typename Type>
    Type *primaryFeedMaybeAs() const {
        if (Feed *f = primaryFeed()) {
            return dynamic_cast<Type *>(f);
        }
        return nullptr;
    }

    /**
     * Detaches all feeds and deletes the Feed instances. Existing files in the
     * folder are unaffected.
     */
    void clearFeeds();

    /**
     * Provides access to the list of Feeds for this folder. The feeds are responsible
     * for creating File and Folder instances in the folder.
     */
    Feeds feeds() const;

    String contentsAsText() const;

    // filesys::Node overrides:
    const Node *tryFollowPath(const PathRef &path) const;
    const Node *tryGetChild(const String &name) const;

public:
    static Folder &root();

    enum WaitBehavior {
        OnlyInBackground,
        BlockingMainThread,
    };
    static void waitForPopulation(WaitBehavior waitBehavior = OnlyInBackground);

    /**
     * When all folder population tasks are finished, performs a callback in the main
     * thread. Does not block the main thread. If nothing is currently being populated,
     * the callback is called immediately before the method returns.
     *
     * @param func  Callback to be called in the main thread.
     *
     * @return Task handle. Can be ignored or added to an AsyncScope.
     */
    static AsyncTask *afterPopulation(std::function<void ()> func);

    static bool isPopulatingAsync();

    static void checkDefaultSettings();

private:
    DE_PRIVATE(d)
};

DE_DECLARE_AUDIENCE(FolderPopulation, void folderPopulationFinished())
DE_EXTERN_AUDIENCE(FolderPopulation)

} // namespace de

#endif /* LIBCORE_FOLDER_H */
