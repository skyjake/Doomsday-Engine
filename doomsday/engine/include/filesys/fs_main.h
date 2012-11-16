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

#include "file.h"
#include "filehandle.h"

#ifdef __cplusplus

#include <QList>
#include <de/String>
#include "pathtree.h"
#include "fileinfo.h"

/**
 * @defgroup fs File System
 */

namespace de
{
    namespace internal {
        template <typename Type>
        inline bool cannotCastFileTo(File1* file) {
            return dynamic_cast<Type*>(file) == NULL;
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

            PathListItem(String const& _path, int _attrib = 0)
                : path(_path), attrib(_attrib)
            {}

            bool operator < (PathListItem const& other) const
            {
                return path.compareWithoutCase(other.path) < 0;
            }
        };
        typedef QList<PathListItem> PathList;

        typedef QList<FileHandle*> FileList;

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
         * Add a new path mapping from source to destination in the vfs.
         * @note Paths will be transformed into absolute paths if needed.
         */
        void mapPath(String source, String destination);

        /**
         * Clears all virtual path mappings.
         *
         * @return  This instance.
         */
        FS1& clearPathMappings();

        /**
         * Add a new lump mapping so that @a lumpName becomes visible at @a destination.
         */
        void mapPathToLump(String lumpName, String destination);

        /**
         * Clears all path => lump mappings.
         *
         * @return  This instance.
         */
        FS1& clearPathLumpMappings();

        /**
         * Reset known fileId records so that the next time checkFileId() is called for
         * a filepath, it will pass.
         */
        void resetFileIds();

        /**
         * Maintains a list of identifiers already seen.
         *
         * @return  @c true if the given file can be opened, or
         *          @c false if it has already been opened.
         */
        bool checkFileId(String path);

        /**
         * @return  @c true if a file exists at @a path which can be opened for reading.
         */
        bool accessFile(String path);

        /**
         * Indexes @a file (which must have been opened with this file system) into
         * this file system and adds it to the list of loaded files.
         *
         * @param file      The file to index. Assumed to have not yet been indexed!
         *
         * @return  This instance.
         */
        FS1& index(File1& file);

        /**
         * Removes a file from any lump indexes.
         *
         * @param file  File to remove from the index.
         */
        FS1& deindex(File1& file);

        lumpnum_t lumpNumForName(String name, bool silent = true);

        /**
         * Provides access to the currently active Wad lump name index. This can
         * be used for efficiently looking up files based on name.
         */
        LumpIndex const& nameIndex() const;

        /**
         * Provides access to the Wad lump name index which is applicable to the
         * specified @a absoluteLumpNum. This can be used for efficiently looking
         * up files based on name.
         *
         * @param absoluteLumpNum   Determines which lump index to return. This
         *                          number is then translated into the range for
         *                          the selected index.
         */
        LumpIndex const& nameIndexForLump(lumpnum_t& absoluteLumpNum) const;

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
        FileHandle& openFile(String const& path, String const& mode, size_t baseOffset = 0,
                             bool allowDuplicate = true);

        /**
         * Try to open the specified lump for reading.
         *
         * @param lump       The file to be opened.
         *
         * @return  Handle to the opened file.
         *
         * @todo This method is no longer necessary at this level. Opening a file which
         * is already present in the file system should not require calling back to a
         * method of the file system itself (bad OO design).
         */
        FileHandle& openLump(File1& lump);

        /// Clear all references to this file.
        void releaseFile(File1& file);

        /**
         * Find a single file.
         *
         * @throws NotFoundError If the requested file could not be found.
         */
        File1& find(String path);

        /**
         * Finds all files.
         *
         * @param found         Set of files that match the result.
         *
         * @return  Number of files found.
         */
        int findAll(FileList& found) const;

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
        int findAll(bool (*predicate)(File1& file, void* parameters), void* parameters,
                    FileList& found) const;

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
        int findAll(bool (*predicate)(File1& file, void* parameters), void* parameters,
                    FileList& found) const
        {
            findAll(predicate, parameters, found);
            // Filter out the wrong types.
            QMutableListIterator<FileHandle*> i(found);
            while(i.hasNext())
            {
                i.next();
                if(internal::cannotCastFileTo<Type>(&i.value()->file()))
                    i.remove();
            }
            return found.count();
        }

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
        int findAllPaths(String searchPattern, int flags, PathList& found);

        /**
         * Print contents of the specified directory of the virtual file system.
         */
        void printDirectory(String path);

        /**
         * Calculate a CRC for the loaded file list.
         */
        uint loadedFilesCRC();

        /**
         * Try to open the specified WAD archive into the auxiliary lump index.
         *
         * @return  Base index for lumps in this archive.
         */
        lumpnum_t openAuxiliary(String path, size_t baseOffset = 0);

        /**
         * Close the auxiliary lump index if open.
         */
        void closeAuxiliaryPrimaryIndex();

        /**
         * Unload all files loaded after startup.
         * @return  Number of files unloaded.
         */
        FS1& unloadAllNonStartupFiles(int* numUnloaded = 0);

    private:
        struct Instance;
        Instance* d;

        /**
         * @param hndl  Handle to the file to be interpreted. Ownership is passed to
         *              the interpreted file instance.
         * @param path  Absolute VFS path by which the interpreted file will be known.
         * @param info  Prepared info metadata for the file.
         *
         * @return  The interpreted File file instance.
         */
        File1& interpret(FileHandle& hndl, String path, FileInfo const& info);
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

void F_UnloadAllNonStartupFiles(int* numUnloaded);

void F_AddVirtualDirectoryMapping(char const* nativeSourcePath, char const* nativeDestinationPath);

void F_AddLumpDirectoryMapping(char const* lumpName, char const* nativeDestinationPath);

void F_ResetFileIds(void);

boolean F_CheckFileId(char const* nativePath);

int F_LumpCount(void);

int F_Access(char const* nativePath);

void F_Index(struct file1_s* file);

void F_Deindex(struct file1_s* file);

FileHandle* F_Open3(char const* nativePath, char const* mode, size_t baseOffset, boolean allowDuplicate);
FileHandle* F_Open2(char const* nativePath, char const* mode, size_t baseOffset/*, allowDuplicate = true */);
FileHandle* F_Open(char const* nativePath, char const* mode/*, baseOffset = 0 */);

FileHandle* F_OpenLump(lumpnum_t absoluteLumpNum);

boolean F_IsValidLumpNum(lumpnum_t absoluteLumpNum);

lumpnum_t F_LumpNumForName(char const* name);

AutoStr* F_ComposeLumpFilePath(lumpnum_t absoluteLumpNum);

boolean F_LumpIsCustom(lumpnum_t absoluteLumpNum);

AutoStr* F_LumpName(lumpnum_t absoluteLumpNum);

size_t F_LumpLength(lumpnum_t absoluteLumpNum);

uint F_LumpLastModified(lumpnum_t absoluteLumpNum);

struct file1_s* F_FindFileForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx);
struct file1_s* F_FindFileForLumpNum(lumpnum_t absoluteLumpNum/*, lumpIdx = 0 */);

void F_Delete(struct filehandle_s* file);

AutoStr* F_ComposePath(struct file1_s const* file);

void F_SetCustom(struct file1_s* file, boolean yes);

AutoStr* F_ComposeLumpPath2(struct file1_s* file, int lumpIdx, char delimiter);
AutoStr* F_ComposeLumpPath(struct file1_s* file, int lumpIdx); /*delimiter='/'*/

size_t F_ReadLump(struct file1_s* file, int lumpIdx, uint8_t* buffer);

size_t F_ReadLumpSection(struct file1_s* file, int lumpIdx, uint8_t* buffer,
                         size_t startOffset, size_t length);

uint8_t const* F_CacheLump(struct file1_s* file, int lumpIdx);

void F_UnlockLump(struct file1_s* file, int lumpIdx);

/**
 * Compiles a list of file names, separated by @a delimiter.
 */
void F_ComposePWADFileList(char* outBuf, size_t outBufSize, const char* delimiter);

uint F_CRCNumber(void);

lumpnum_t F_OpenAuxiliary2(char const* nativePath, size_t baseOffset);
lumpnum_t F_OpenAuxiliary(char const* nativePath/*, baseOffset = 0 */);

void F_CloseAuxiliary(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_MAIN_H */
