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
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#include "abstractfile.h"
#include "dfile.h"
#include "pathdirectory.h"

#ifdef __cplusplus

#include <QList>
#include <de/String>
#include "lumpinfo.h"

/**
 * @defgroup fs File System
 */

namespace de
{
    /**
     * Internally the lump index has two parts: the Primary index (which is populated
     * with lumps from loaded data files) and the Auxiliary index (used to temporarily
     * open a file that is not considered part of the filesystem).
     *
     * Functions that don't know the absolute/logical lumpnum of file will have to check
     * both indexes (e.g., FS1::lumpNumForName()).
     *
     * @ingroup fs
     */
    class FS1
    {
    public:
        /// No files found. @ingroup errors
        DENG2_ERROR(NotFoundError);

        struct PathListItem
        {
            String path;
            int attrib;

            PathListItem(QString const& _path, int _attrib = 0)
                : path(_path), attrib(_attrib)
            {}

            bool operator < (PathListItem const& other) const
            {
                return path.compareWithoutCase(other.path) < 0;
            }
        };
        typedef QList<PathListItem> PathList;

        typedef QList<DFile*> FileList;

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
         * Remove all file records flagged Runtime.
         * @return  Number of records removed.
         */
        int reset();

        /**
         * (Re-)Initialize the path mappings.
         */
        void initPathMap();

        /**
         * Add a new path mapping from source to destination in the vfs.
         * @note Paths will be transformed into absolute paths if needed.
         */
        void mapPath(char const* source, char const* destination);

        /**
         * (Re-)Initialize the path => lump mappings.
         * @note Should be called after WADs have been processed.
         */
        void initLumpPathMap();

        /**
         * Add a new lump mapping so that @a lumpName becomes visible as @a symbolicPath
         * throughout the vfs.
         * @note @a symbolicPath will be transformed into an absolute path if needed.
         */
        void mapPathToLump(char const* symbolicPath, char const* lumpName);

        /**
         * Reset known fileId records so that the next time checkFileId() is called for
         * a filepath, it will pass.
         */
        void resetFileIds();

        /**
         * Maintains a list of identifiers already seen.
         *
         * @return @c true if the given file can be opened, or
         *         @c false, if it has already been opened.
         */
        bool checkFileId(char const* path);

        /**
         * @return  @c true if a file exists at @a path which can be opened for reading.
         */
        bool accessFile(char const* path);

        /**
         * Files with a .wad extension are archived data files with multiple 'lumps',
         * other files are single lumps whose base filename will become the lump name.
         *
         * @param path          Path to the file to be opened. Either a "real" file in the local
         *                      file system, or a "virtual" file in the virtual file system.
         * @param baseOffset    Offset from the start of the file in bytes to begin.
         *
         * @return  Newly added file instance if the operation is successful, else @c NULL.
         */
        AbstractFile* addFile(char const* path, size_t baseOffset = 0);

        /// @note All files are added with baseOffset = @c 0.
        int addFiles(char const* const* paths, int num);

        /**
         * Attempt to remove a file from the virtual file system.
         *
         * @param permitRequired  @c true= allow removal of resources marked as "required"
         *                        by the currently loaded Game.
         * @return @c true if the operation is successful.
         */
        bool removeFile(char const* path, bool permitRequired = false);

        int removeFiles(char const* const* paths, int num, bool permitRequired = false);

        /// @return  Number of lumps in the currently active LumpIndex.
        int lumpCount();

        bool isValidLumpNum(lumpnum_t absoluteLumpNum);

        lumpnum_t lumpNumForName(char const* name, bool silent = true);

        char const* lumpName(lumpnum_t absoluteLumpNum);

        /**
         * Given a logical @a lumpnum retrieve the associated file object.
         *
         * @post The active LumpIndex may have changed!
         *
         * @param absoluteLumpNum   Logical lumpnum associated to the file being looked up.
         * @param lumpIdx           If not @c NULL the translated lumpnum within the owning
         *                          file object is written here.
         *
         * @return  Found file object.
         *
         * @throws NotFoundError If the requested file could not be found.
         */
        AbstractFile& lumpFile(lumpnum_t absoluteLumpNum, int* lumpIdx = 0);

        /**
         * Convenient lookup method for when only the path is needed from the source file.
         * If @a absoluteLumpNum is NOT known then a zero length string is returned.
         */
        inline char const* lumpFilePath(lumpnum_t absoluteLumpNum)
        {
            try
            {
                return Str_Text(lumpFile(absoluteLumpNum).path());
            }
            catch(NotFoundError const&)
            {} // Ignore this error.
            return "";
        }

        /**
         * Convenient lookup method for when only the custom property is needed from the source file.
         * If @a absoluteLumpNum is NOT known then @c false is returned.
         */
        bool lumpFileHasCustom(lumpnum_t absoluteLumpNum)
        {
            try
            {
                return lumpFile(absoluteLumpNum).hasCustom();
            }
            catch(NotFoundError const&)
            {} // Ignore this error.
            return false;
        }

        /**
         * Retrieve the LumpInfo metadata record for a lump in the Wad lump index.
         *
         * @post The active LumpIndex may have changed!
         *
         * @param absoluteLumpNum   Logical lumpnum associated to the file being looked up.
         * @param lumpIdx           If not @c NULL the translated lumpnum within the owning
         *                          file object is written here.
         *
         * @return  Metadata record for the lump.
         *
         * @throws NotFoundError If the requested file could not be found.
         */
        LumpInfo const& lumpInfo(lumpnum_t absoluteLumpNum, int* lumpIdx = 0);

        /**
         * Retrieve the LumpInfo metadata record for a lump in the Zip lump index.
         *
         * @param path              Path to search for (made absolute if necessary).
         * @param lumpIdx           If not @c NULL the translated lumpnum within the owning
         *                          file object is written here.
         *
         * @return  Metadata record for the lump.
         *
         * @throws NotFoundError If the requested file could not be found.
         */
        LumpInfo const& zipLumpInfo(char const* path, int* lumpIdx = 0);

        /**
         * Convenient lookup method for when only the length property is needed from a LumpInfo.
         * If @a absoluteLumpNum is NOT known then @c 0 is returned.
         */
        inline size_t lumpLength(lumpnum_t absoluteLumpNum)
        {
            try
            {
                return lumpInfo(absoluteLumpNum).size;
            }
            catch(NotFoundError const&)
            {} // Ignore this error.
            return 0;
        }

        /**
         * Convenient lookup method for when only the last-modified property is needed from a LumpInfo.
         * If @a absoluteLumpNum is NOT known then @c 0 is returned.
         */
        inline uint lumpLastModified(lumpnum_t absoluteLumpNum)
        {
            try
            {
                return lumpInfo(absoluteLumpNum).lastModified;
            }
            catch(NotFoundError const&)
            {} // Ignore this error.
            return 0;
        }

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
        DFile& openFile(char const* path, char const* mode, size_t baseOffset = 0,
                        bool allowDuplicate = true);

        /**
         * Try to locate the specified lump for reading.
         *
         * @param absoluteLumpNum   Logical lumpnum associated to the file being looked up.
         *
         * @return  Handle to the opened file.
         *
         * @throws NotFoundError If the requested lump could not be found.
         */
        DFile& openLump(lumpnum_t absoluteLumpNum);

        /// Clear all references to this file.
        void releaseFile(AbstractFile& file);

        /// Close this file handle.
        void closeFile(DFile& hndl);

        /// Completely destroy this file; close if open, clear references and any acquired identifiers.
        void deleteFile(DFile& hndl);

        /**
         * Finds all files.
         *
         * @param found         Set of files that match the result.
         *
         * @return  Number of files found.
         */
        int findAll(FileList& found);

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
        int findAll(bool (*predicate)(DFile* hndl, void* parameters), void* parameters,
                    FileList& found);

        /**
         * Finds all paths which match the search criteria. Will search the Zip lump index,
         * lump => path mappings and native files in the local file system.
         *
         * @param searchPattern Pattern which defines the scope of the search.
         * @param flags         @ref searchPathFlags
         * @param found         Set of (absolute) paths that match the result.
         *
         * @return  Number of paths found.
         */
        int findAllPaths(char const* searchPattern, int flags, PathList& found);

        /**
         * Print contents of the specified directory of the virtual file system.
         */
        void printDirectory(ddstring_t const* path);

        /**
         * Print contents of the primary lump index.
         */
        void printIndex();

        /**
         * Calculate a CRC for the loaded file list.
         */
        uint loadedFilesCRC();

        /**
         * Try to open the specified WAD archive into the auxiliary lump index.
         *
         * @return  Base index for lumps in this archive.
         */
        lumpnum_t openAuxiliary(char const* filePath, size_t baseOffset = 0);

        /**
         * Close the auxiliary lump index if open.
         */
        void closeAuxiliary();

    private:
        struct Instance;
        Instance* d;

        /**
         * @param hndl  Handle to the file to be interpreted. Ownership is passed to
         *              the interpreted file instance.
         * @param path  Absolute VFS path by which the interpreted file will be known.
         * @param info  Prepared info metadata for the file.
         *
         * @return  The interpreted AbstractFile file instance.
         */
        AbstractFile& interpret(DFile& hndl, char const* path, LumpInfo const& info);

        /**
         * Adds a file to any relevant indexes.
         *
         * @param file  File to index.
         */
        void index(AbstractFile& file);

        /**
         * Removes a file from any lump indexes.
         *
         * @param file  File to remove from the index.
         */
        void deindex(AbstractFile& file);

        bool unloadFile(char const* path, bool permitRequired = false, bool quiet = false);
    };

} // namespace de

de::FS1* App_FileSystem();

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

int F_Reset(void);

void F_InitVirtualDirectoryMappings(void);

void F_AddVirtualDirectoryMapping(char const* source, char const* destination);

void F_InitLumpDirectoryMappings(void);

void F_AddLumpDirectoryMapping(char const* lumpName, char const* symbolicPath);

void F_ResetFileIds(void);

boolean F_CheckFileId(char const* path);

int F_LumpCount(void);

int F_Access(char const* path);

uint F_GetLastModified(char const* fileName);

struct abstractfile_s* F_AddFile2(char const* path, size_t baseOffset);
struct abstractfile_s* F_AddFile(char const* path/*, baseOffset = 0*/);

boolean F_RemoveFile2(char const* path, boolean permitRequired);
boolean F_RemoveFile(char const* path/*, permitRequired = false */);

int F_AddFiles(char const* const* paths, int num);

int F_RemoveFiles3(char const* const* paths, int num, boolean permitRequired);
int F_RemoveFiles(char const* const* paths, int num/*, permitRequired = false */);

DFile* F_Open3(char const* path, char const* mode, size_t baseOffset, boolean allowDuplicate);
DFile* F_Open2(char const* path, char const* mode, size_t baseOffset/*, allowDuplicate = true */);
DFile* F_Open(char const* path, char const* mode/*, baseOffset = 0 */);

DFile* F_OpenLump(lumpnum_t absoluteLumpNum);

boolean F_IsValidLumpNum(lumpnum_t absoluteLumpNum);

lumpnum_t F_LumpNumForName(char const* name);

char const* F_LumpSourceFile(lumpnum_t absoluteLumpNum);

boolean F_LumpIsCustom(lumpnum_t absoluteLumpNum);

char const* F_LumpName(lumpnum_t absoluteLumpNum);

size_t F_LumpLength(lumpnum_t absoluteLumpNum);

uint F_LumpLastModified(lumpnum_t absoluteLumpNum);

struct abstractfile_s* F_FindFileForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx);
struct abstractfile_s* F_FindFileForLumpNum(lumpnum_t absoluteLumpNum/*, lumpIdx = 0 */);

void F_Close(struct dfile_s* file);

void F_Delete(struct dfile_s* file);

Str const* F_Path(struct abstractfile_s const* file);

void F_SetCustom(struct abstractfile_s* file, boolean yes);

AutoStr* F_ComposeLumpPath2(struct abstractfile_s* file, int lumpIdx, char delimiter);
AutoStr* F_ComposeLumpPath(struct abstractfile_s* file, int lumpIdx); /*delimiter='/'*/

size_t F_ReadLump(struct abstractfile_s* file, int lumpIdx, uint8_t* buffer);

size_t F_ReadLumpSection(struct abstractfile_s* file, int lumpIdx, uint8_t* buffer,
                         size_t startOffset, size_t length);

uint8_t const* F_CacheLump(struct abstractfile_s* file, int lumpIdx);

void F_UnlockLump(struct abstractfile_s* file, int lumpIdx);

/**
 * Compiles a list of file names, separated by @a delimiter.
 */
void F_ComposeFileList(filetype_t type, boolean markedCustom, char* outBuf, size_t outBufSize, const char* delimiter);

uint F_CRCNumber(void);

lumpnum_t F_OpenAuxiliary2(char const* fileName, size_t baseOffset);
lumpnum_t F_OpenAuxiliary(char const* fileName/*, baseOffset = 0 */);

void F_CloseAuxiliary(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_MAIN_H */
