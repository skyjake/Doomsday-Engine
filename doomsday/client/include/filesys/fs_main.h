/**
 * @file fs_main.h
 *
 * Virtual file system and file (input) stream abstraction layer.
 *
 * This version supports runtime (un)loading.
 *
 * File input. Can read from real files or WAD lumps. Note that reading from
 * WAD lumps means that a copy is taken of the lump when the corresponding
 * 'file' is opened. With big files this uses considerable memory and time.
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2009-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_FILESYS_MAIN_H
#define LIBDENG_FILESYS_MAIN_H

#include "file.h"
#include "filehandle.h"

#ifdef __cplusplus

#include <QList>
#include <QMultiMap>
#include <de/String>
#include <de/PathTree>
#include "fileinfo.h"
#include "filetype.h"
#include "searchpath.h"

/**
 * @defgroup fs File System
 */

/**
 * @defgroup resourceLocationFlags  Resource Location Flags
 *
 * Flags used with de::FS1::find().
 * @ingroup flags
 */
///@{
#define RLF_MATCH_EXTENSION     0x1 /// If an extension is specified in the search term the found file should have it too.

/// Default flags.
#define RLF_DEFAULT             0
///@}

namespace de {

namespace internal {
    template <typename Type>
    inline bool cannotCastFileTo(File1 *file) {
        return dynamic_cast<Type *>(file) == NULL;
    }
}

/**
 * Files with a .wad extension are archived data files with multiple 'lumps',
 * other files are single lumps whose base filename will become the lump name.
 *
 * Internally the lump index has two parts: the Primary index (which is populated
 * with lumps from loaded data files) and the Auxiliary index (used to temporarily
 * open a file that is not considered part of the filesystem).
 *
 * Functions that don't know the absolute/logical lumpnum of file will have to
 * check both indexes (e.g., FS1::lumpNumForName()).
 *
 * @ingroup fs
 */
class FS1
{
public:
    /// No files found. @ingroup errors
    DENG2_ERROR(NotFoundError);

    /// An unknown scheme was referenced. @ingroup errors
    DENG2_ERROR(UnknownSchemeError);

    /**
     * (Search) path groupings in descending priority.
     */
    enum PathGroup
    {
        /// 'Override' paths have the highest priority. These are usually
        /// set according to user specified paths, e.g., via the command line.
        OverridePaths,

        /// 'Extra' paths are those which are determined dynamically when some
        /// runtime resources are loaded. The DED module utilizes these to add
        /// new model search paths found when parsing definition files.
        ExtraPaths,

        /// Default paths are those which are known a priori. These are usually
        /// determined at compile time and are implicit paths relative to the
        /// virtual file system.
        DefaultPaths,

        /// Fallback (i.e., last-resort) paths have the lowest priority. These
        /// are usually set according to user specified paths, e.g., via the
        /// command line.
        FallbackPaths
    };

    /**
     * Scheme defines a file system subspace.
     *
     * @todo The symbolic name of the schme and the path mapping template
     *       (mapPath()) should be defined externally. -ds
     */
    class Scheme
    {
    public:
        /// Symbolic names must be at least this number of characters.
        static int const min_name_length = URI_MINSCHEMELENGTH;

        enum Flag
        {
            /// Packages may include virtual file mappings to the scheme with a
            /// root directory which matches the symbolic name of the scheme.
            ///
            /// @see mapPath()
            MappedInPackages    = 0x01
        };
        Q_DECLARE_FLAGS(Flags, Flag)

        /// Groups of search paths ordered by priority.
        typedef QMultiMap<PathGroup, SearchPath> SearchPaths;

        /// List of found file nodes.
        typedef QList<PathTree::Node *> FoundNodes;

    public:
        explicit Scheme(String symbolicName, Flags flags = 0);
        ~Scheme();

        /// @return  Symbolic name of this scheme (e.g., "Models").
        String const &name() const;

        /**
         * Clear this scheme back to it's "empty" state (i.e., no resources).
         * The search path groups are unaffected.
         */
        void clear();

        /**
         * Rebuild this scheme by re-scanning for resources on all search paths
         * and re-populating the scheme's index.
         *
         * @note Any manually added resources will not be present after this.
         */
        void rebuild();

        /**
         * Reset this scheme, returning it to an empty state and clearing any
         * @ref ExtraPaths which have been registered since its construction.
         */
        inline void reset()
        {
            clearSearchPathGroup(ExtraPaths);
            clear();
        }

        /**
         * Manually add a resource to this scheme. Duplicates are pruned automatically.
         *
         * @param resourceNode  Node which represents the resource.
         *
         * @return  @c true iff this scheme did not already contain the resource.
         */
        bool add(PathTree::Node &resourceNode);

        /**
         * Finds all resources in this scheme.
         *
         * @param name    If not an empty string, only consider resources whose
         *                name begins with this. Case insensitive.
         * @param found   Set of resources which match the search.
         *
         * @return  Number of found resources.
         */
        int findAll(String name, FoundNodes &found);

        /**
         * Add a new search path to this scheme. Newer paths have priority over
         * previously added paths.
         *
         * @param path      New unresolved search path to add. A copy is made.
         * @param group     Group to add this path to. @ref PathGroup
         *
         * @return  @c true if @a path was well-formed and subsequently added.
         */
        bool addSearchPath(SearchPath const &path, PathGroup group = DefaultPaths);

        /**
         * Clear search paths in @a group from the scheme.
         *
         * @param group  Search path group to be cleared.
         */
        void clearSearchPathGroup(PathGroup group);

        /**
         * Provides access to the search paths for efficient traversals.
         */
        SearchPaths const &allSearchPaths() const;

        /**
         * Clear all search paths in all groups in the scheme.
         */
        void clearAllSearchPaths();

        /**
         * Apply mapping for this scheme to the specified path. Mapping must be
         * enabled (with @ref MappedInPackages) otherwise this does nothing.
         *
         * For example, given the scheme name "models":
         *
         * <pre>
         *     "models/mymodel.dmd" => "$(App.DataPath)/$(GamePlugin.Name)/models/mymodel.dmd"
         * </pre>
         *
         * @param path  The path to be mapped (applied in-place).
         *
         * @return  @c true iff mapping was applied to the path.
         */
        bool mapPath(String &path) const;

#if _DEBUG
        void debugPrint() const;
#endif

    private:
        struct Instance;
        Instance *d;
    };

    /// File system subspace schemes.
    typedef QMap<String, Scheme *> Schemes;

    /**
     * PathListItem represents a found path for find file search results.
     */
    struct PathListItem
    {
        Path path;
        int attrib;

        PathListItem(Path const &_path, int _attrib = 0)
            : path(_path), attrib(_attrib)
        {}

        bool operator < (PathListItem const &other) const
        {
            return path < other.path;
        }
    };

    /// List of found path search results.
    typedef QList<PathListItem> PathList;

    /// List of file search results.
    typedef QList<FileHandle *> FileList;

public:
    /**
     * Constructs a new file system.
     */
    FS1();

    virtual ~FS1();

    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

    /**
     * @post No more WADs will be loaded in startup mode.
     */
    void endStartup();

    /**
     * Find a Scheme by symbolic name.
     *
     * @param name  Symbolic name of the scheme.
     * @return  Scheme associated with @a name.
     */
    Scheme &scheme(String name);

    /**
     * @param name      Unique symbolic name of the new scheme. Must be at least
     *                  @c Scheme::min_name_length characters long.
     * @param flags     @ref Scheme::Flag
     */
    Scheme &createScheme(String name, Scheme::Flags flags = 0);

    /**
     * Returns @c true iff a Scheme exists with the symbolic @a name.
     */
    bool knownScheme(String name);

    /**
     * Returns the schemes for efficient traversal.
     */
    Schemes const &allSchemes();

    /**
     * Reset all the schemes, returning their indexes to an empty state and clearing
     * any @ref ExtraPaths which have been registered since creation.
     */
    inline void resetAllSchemes()
    {
        Schemes schemes = allSchemes();
        DENG2_FOR_EACH(Schemes, i, schemes){ (*i)->reset(); }
    }

    /**
     * Add a new path mapping from source to destination.
     * @note Paths will be transformed into absolute paths if needed.
     */
    void addPathMapping(String source, String destination);

    /**
     * Clears all virtual path mappings.
     */
    void clearPathMappings();

    /**
     * Add a new lump mapping so that @a lumpName becomes visible at @a destination.
     */
    void addPathLumpMapping(String lumpName, String destination);

    /**
     * Clears all path => lump mappings.
     *
     * @return  This instance.
     */
    void clearPathLumpMappings();

    /**
     * @return  @c true if a file exists at @a path which can be opened for reading.
     */
    bool accessFile(Uri const &path);

    /**
     * Maintains a list of identifiers already seen.
     *
     * @return  @c true if the given file can be opened, or
     *          @c false if it has already been opened.
     */
    bool checkFileId(Uri const &path);

    /**
     * Reset known fileId records so that the next time checkFileId() is called for
     * a filepath, it will pass.
     */
    void resetFileIds();

    /**
     * @param hndl  Handle to the file to be interpreted. Ownership is passed to
     *              the interpreted file instance.
     * @param path  Absolute VFS path by which the interpreted file will be known.
     * @param info  Prepared info metadata for the file.
     *
     * @return  The interpreted File file instance.
     */
    File1 &interpret(FileHandle &hndl, String path, FileInfo const &info);

    /**
     * Indexes @a file (which must have been opened with this file system) into
     * this file system and adds it to the list of loaded files.
     *
     * @param file  The file to index. Assumed to have not yet been indexed!
     */
    void index(File1 &file);

    /**
     * Removes a file from any indexes.
     *
     * @param file  File to remove from the index.
     */
    void deindex(File1 &file);

    /// Clear all references to this file.
    void releaseFile(File1 &file);

    /**
     * Lookup a lump by name.
     *
     * @param name  Name of the lump to lookup.
     * @return  Logical lump number for the found lump; otherwise @c -1.
     *
     * @todo At this level there should be no distinction between lumps. -ds
     */
    lumpnum_t lumpNumForName(String name);

    /**
     * Provides access to the main index of the file system. This can be
     * used for efficiently looking up files based on name.
     */
    LumpIndex const &nameIndex() const;

    /**
     * Opens the given file (will be translated) for reading.
     *
     * @post If @a allowDuplicate = @c false a new file ID for this will have been
     * added to the list of known file identifiers if this file hasn't yet been
     * opened. It is the responsibility of the caller to release this identifier when done.
     *
     * @param path      Possibly relative or mapped path to the resource being opened.
     * @param mode      'b' = binary
     *                  't' = text mode (with real files, lumps are always binary)
     *
     *                  'f' = must be a real file in the local file system
     * @param baseOffset  Offset from the start of the file in bytes to begin.
     * @param allowDuplicate  @c false = open only if not already opened.
     *
     * @return  Handle to the opened file.
     *
     * @throws NotFoundError If the requested file could not be found.
     */
    FileHandle &openFile(String const &path, String const &mode, size_t baseOffset = 0,
                         bool allowDuplicate = true);

    /**
     * Try to open the specified lump for reading.
     *
     * @param lump  The file to be opened.
     *
     * @return  Handle to the opened file.
     *
     * @todo This method is no longer necessary at this level. Opening a file which
     * is already present in the file system should not require calling back to a
     * method of the file system itself (bad OO design).
     */
    FileHandle &openLump(File1 &lump);

    /**
     * Find a single file.
     *
     * @param search  The search term.
     * @return Found file.
     */
    File1 &find(Uri const &search);

    /**
     * Finds all files.
     *
     * @param found  Set of files that match the result.
     *
     * @return  Number of files found.
     */
    int findAll(FileList &found) const;

    /**
     * Finds all files which meet the supplied @a predicate.
     *
     * @param predicate     If not @c NULL, this predicate evaluator callback must
     *                      return @c true for a given file to be included in the
     *                      @a found FileList.
     * @param parameters    Passed to the predicate evaluator callback.
     * @param found         Set of files that match the result.
     *
     * @return  Number of files found.
     */
    int findAll(bool (*predicate)(File1 &file, void *parameters), void *parameters,
                FileList &found) const;

    /**
     * Finds all files of a specific type which meet the supplied @a predicate.
     * Only files that can be represented as @a Type are included in the results.
     *
     * @param predicate     If not @c NULL, this predicate evaluator callback must
     *                      return @c true for a given file to be included in the
     *                      @a found FileList.
     * @param parameters    Passed to the predicate evaluator callback.
     * @param found         Set of files that match the result.
     *
     * @return  Number of files found.
     */
    template <typename Type>
    int findAll(bool (*predicate)(File1 &file, void *parameters), void *parameters,
                FileList &found) const
    {
        findAll(predicate, parameters, found);
        // Filter out the wrong types.
        QMutableListIterator<FileHandle *> i(found);
        while(i.hasNext())
        {
            i.next();
            if(internal::cannotCastFileTo<Type>(&i.value()->file()))
                i.remove();
        }
        return found.count();
    }

    /**
     * Search the file system for a path to a file.
     *
     * @param search        The search term. If a scheme is specified, first check
     *                      for a similarly named Scheme with which to limit the
     *                      search. If not found within the scheme then perform a
     *                      wider search of the whole file system.
     * @param flags         @ref resourceLocationFlags
     * @param rclass        Class of resource being searched for. If no file is found
     *                      which matches the search term and a non-null resource
     *                      class is specified try alternative names for the file
     *                      according to the list of known file extensions for each
     *                      file type associated with this class of resource.
     *
     * @return  The found path.
     *
     * @throws NotFoundError If the requested file could not be found.
     *
     * @todo Fold into find() -ds
     */
    String findPath(Uri const &search, int flags, ResourceClass &rclass);
    String findPath(Uri const &search, int flags);

    /**
     * Finds all paths which match the search criteria. Will search the Zip
     * lump index, lump => path mappings and native files in the local system.
     *
     * @param searchPattern Pattern which defines the scope of the search.
     * @param flags         @ref searchPathFlags
     * @param found         Set of (absolute) paths that match the result.
     *
     * @return  Number of paths found.
     */
    int findAllPaths(Path searchPattern, int flags, PathList &found);

    /**
     * Print contents of the specified directory of the virtual file system.
     */
    void printDirectory(Path path);

    /**
     * Calculate a CRC for the loaded file list.
     */
    uint loadedFilesCRC();

    /**
     * Unload all files loaded after startup.
     * @return  Number of files unloaded.
     */
    int unloadAllNonStartupFiles();

private:
    struct Instance;
    Instance *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FS1::Scheme::Flags)

} // namespace de

de::FS1 &App_FileSystem();

/**
 * Returns the application's data base path in the format expected by FS1.
 * @return Base path.
 */
de::String App_BasePath();

extern "C" {
#endif // __cplusplus

/**
 * C wrapper API:
 */

struct filelist_s;
typedef struct filelist_s FileList;

void F_Register(void);

/// Initialize this module. Cannot be re-initialized, must shutdown first.
void F_Init(void);

/// Shutdown this module.
void F_Shutdown(void);

void F_EndStartup(void);

int F_UnloadAllNonStartupFiles();

void F_AddVirtualDirectoryMapping(char const *nativeSourcePath, char const *nativeDestinationPath);

void F_AddLumpDirectoryMapping(char const *lumpName, char const *nativeDestinationPath);

void F_ResetFileIds(void);

boolean F_CheckFileId(char const *nativePath);

int F_LumpCount(void);

void F_Index(struct file1_s *file);

void F_Deindex(struct file1_s *file);

FileHandle *F_Open3(char const *nativePath, char const *mode, size_t baseOffset, boolean allowDuplicate);
FileHandle *F_Open2(char const *nativePath, char const *mode, size_t baseOffset/*, allowDuplicate = true */);
FileHandle *F_Open(char const *nativePath, char const *mode/*, baseOffset = 0 */);

FileHandle *F_OpenLump(lumpnum_t lumpNum);

boolean F_IsValidLumpNum(lumpnum_t lumpNum);

lumpnum_t F_LumpNumForName(char const *name);

AutoStr *F_ComposeLumpFilePath(lumpnum_t lumpNum);

boolean F_LumpIsCustom(lumpnum_t lumpNum);

AutoStr *F_LumpName(lumpnum_t lumpNum);

size_t F_LumpLength(lumpnum_t lumpNum);

uint F_LumpLastModified(lumpnum_t lumpNum);

struct file1_s *F_FindFileForLumpNum2(lumpnum_t lumpNum, int *lumpIdx);
struct file1_s *F_FindFileForLumpNum(lumpnum_t lumpNum/*, lumpIdx = 0 */);

void F_Delete(struct filehandle_s *file);

AutoStr *F_ComposePath(struct file1_s const *file);

void F_SetCustom(struct file1_s *file, boolean yes);

AutoStr *F_ComposeLumpPath2(struct file1_s *file, int lumpIdx, char delimiter);
AutoStr *F_ComposeLumpPath(struct file1_s *file, int lumpIdx/*, delimiter ='/' */);

size_t F_ReadLump(struct file1_s *file, int lumpIdx, uint8_t *buffer);

size_t F_ReadLumpSection(struct file1_s *file, int lumpIdx, uint8_t *buffer,
                         size_t startOffset, size_t length);

uint8_t const *F_CacheLump(struct file1_s *file, int lumpIdx);

void F_UnlockLump(struct file1_s *file, int lumpIdx);

/**
 * Compiles a list of file names, separated by @a delimiter.
 */
void F_ComposePWADFileList(char *outBuf, size_t outBufSize, char const *delimiter);

uint F_LoadedFilesCRC(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_MAIN_H */
