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
#include "lumpinfo.h"
#include "pathdirectory.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize this module. Cannot be re-initialized, must shutdown first.
void F_Init(void);

/// Shutdown this module.
void F_Shutdown(void);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <QList>

#include "fileid.h"
#include "lumpindex.h"

namespace de {

typedef QList<DFile*> FileList;

/**
 * Internally the lump index has two parts: the Primary index (which is populated
 * with lumps from loaded data files) and the Auxiliary index (used to temporarily
 * open a file that is not considered part of the filesystem).
 *
 * Functions that don't know the absolute/logical lumpnum of file will have to check
 * both indexes (e.g., FS::lumpNumForName()).
 */
class FS
{
public:
    FS();

    ~FS();

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

    /// @note Should be called after WADs have been processed.
    void initLumpDirectoryMappings();

    /**
     * Add a new lump mapping so that @a lumpName becomes visible as @a symbolicPath
     * throughout the vfs.
     * @note @a symbolicPath will be transformed into an absolute path if needed.
     */
    void addLumpDirectoryMapping(char const* lumpName, char const* symbolicPath);

    /**
     * Reset known fileId records so that the next time F_CheckFileId() is
     * called on a file, it will pass.
     */
    void resetFileIds();

    /**
     * Maintains a list of identifiers already seen.
     *
     * @return @c true if the given file can be opened, or
     *         @c false, if it has already been opened.
     */
    bool checkFileId(char const* path);

    /// @return  Number of files in the currently active primary LumpIndex.
    int lumpCount();

    /**
     * @return  @c true if a file exists at @a path which can be opened for reading.
     */
    bool accessFile(char const* path);

    /**
     * @return  The time when the file was last modified, as seconds since
     * the Epoch else zero if the file is not found.
     */
    uint lastModified(char const* fileName);

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

    /**
     * Opens the given file (will be translated) for reading.
     *
     * @post If @a allowDuplicate = @c false a new file ID for this will have been
     * added to the list of known file identifiers if this file hasn't yet been
     * opened. It is the responsibility of the caller to release this identifier when done.
     *
     * @param path      Possibly relative or mapped path to the resource being opened.
     * @param mode      't' = text mode (with real files, lumps are always binary)
     *                  'b' = binary
     *                  'f' = must be a real file in the local file system
     * @param baseOffset  Offset from the start of the file in bytes to begin.
     * @param allowDuplicate  @c false = open only if not already opened.
     *
     * @return  Opened file reference/handle else @c NULL.
     */
    DFile* openFile(char const* path, char const* mode, size_t baseOffset = 0,
                           bool allowDuplicate = true);

    /**
     * Try to locate the specified lump for reading.
     *
     * @param absoluteLumpNum   Logical lumpnum associated to the file being looked up.
     *
     * @return  Handle to the opened file if found.
     */
    DFile* openLump(lumpnum_t absoluteLumpNum);

    bool isValidLumpNum(lumpnum_t absoluteLumpNum);

    /**
     * Find a lump in the Zip LumpIndex.
     *
     * @param path      Path to search for. Relative paths are made absolute if necessary.
     * @param lumpIdx   If not @c NULL the translated lumpnum within the owning file object is written here.
     * @return  File system object representing the file which contains the found lump else @c NULL.
     */
    AbstractFile* findLumpFile(char const* path, int* lumpIdx = 0);

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
     * @return  Found file object else @c NULL
     */
    AbstractFile* lumpFile(lumpnum_t absoluteLumpNum, int* lumpIdx = 0);

    // Convenient lookup method for when only the path is needed from the source file.
    inline char const* lumpFilePath(lumpnum_t absoluteLumpNum) {
        if(AbstractFile* file = lumpFile(absoluteLumpNum)) return Str_Text(file->path());
        return "";
    }

    // Convenient lookup method for when only the custom property is needed from the source file.
    bool lumpFileHasCustom(lumpnum_t absoluteLumpNum) {
        if(AbstractFile* file = lumpFile(absoluteLumpNum)) return file->hasCustom();
        return false;
    }

    /**
     * Retrieve the LumpInfo metadata structure for a lump.
     *
     * @post The active LumpIndex may have changed!
     *
     * @param absoluteLumpNum   Logical lumpnum associated to the file being looked up.
     * @param lumpIdx           If not @c NULL the translated lumpnum within the owning
     *                          file object is written here.
     * @return
     */
    LumpInfo const* lumpInfo(lumpnum_t absoluteLumpNum, int* lumpIdx = 0);

    // Convenient lookup method for when only the length property is needed from a LumpInfo.
    inline size_t lumpLength(lumpnum_t absoluteLumpNum) {
        if(LumpInfo const* info = lumpInfo(absoluteLumpNum)) return info->size;
        return 0;
    }

    // Convenient lookup method for when only the last-modified property is needed from a LumpInfo.
    inline uint lumpLastModified(lumpnum_t absoluteLumpNum) {
        if(LumpInfo const* info = lumpInfo(absoluteLumpNum)) return info->lastModified;
        return 0;
    }

    /// Clear all references to this file.
    void releaseFile(AbstractFile* file);

    /// Close this file handle.
    void closeFile(DFile* hndl);

    /// Completely destroy this file; close if open, clear references and any acquired identifiers.
    void deleteFile(DFile* hndl);

    /**
     * Parm is passed on to the callback, which is called for each file
     * matching the filespec. Absolute path names are given to the callback.
     * Zip directory, DD_DIREC and the real files are scanned.
     *
     * @param flags  @see searchPathFlags
     */
    int allResourcePaths(char const* searchPath, int flags, int (*callback) (char const* path, PathDirectoryNodeType type, void* parameters), void* parameters = 0);

    /**
     * Calculate a CRC for the loaded file list.
     */
    uint loadedFilesCRC();

    /**
     * Try to open the specified WAD archive into the auxiliary lump cache.
     *
     * @return  Base index for lumps in this archive.
     */
    lumpnum_t openAuxiliary(char const* fileName, size_t baseOffset = 0);

    void closeAuxiliary();

    /**
     * Write the data associated with the specified lump index to @a fileName.
     *
     * @param absolutelumpNum   Absolute index of the lump to open.
     * @param fileName          If not @c NULL write the associated data to this path.
     *                          Can be @c NULL in which case the fileName will be chosen automatically.
     *
     * @return  @c true iff successful.
     */
    bool dumpLump(lumpnum_t absoluteLumpNum, char const* fileName = 0);

    /**
     * Write data into a file.
     *
     * @param data  Data to write.
     * @param size  Size of the data in bytes.
     * @param path  Path of the file to create (existing file replaced).
     *
     * @return @c true if successful, otherwise @c false.
     */
    bool dump(void const* data, size_t size, char const* path);

    void printIndex();
    void printDirectory(ddstring_t const* path);

    /**
     * @param count  If not @c NULL the number of elements in the resultant
     *               array is written back here (for convenience).
     *
     * @return  Array of ptrs to files in this system or @c NULL if empty.
     *      Ownership of the array passes to the caller who should ensure to
     *      release it with free() when no longer needed.
     */
    AbstractFile** collectFiles(int* count);

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

    /// Collect a list of paths including those which have been mapped.
    PathList collectLocalPaths(ddstring_t const* searchPath, bool includeSearchPath);

    /**
     * Collect a list of loaded file paths.
     *
     * @param predicate     If not @c NULL, this predicate evaluator callback must
     *                      return @c true for a given path to be included in the
     *                      resultant list.
     * @param parameters    Passed to the predicate evaluator callback.
     */
    PathList collectPaths(bool (*predicate)(DFile* hndl, void* parameters) = 0, void* parameters = 0);

private:
    struct Instance;
    Instance* d;

    void unlinkFile(AbstractFile* file);

    bool unloadFile(char const* path, bool permitRequired = false, bool quiet = false);

    int unloadListFiles(FileList& list, bool nonStartup);

    FILE* findRealFile(char const* path, char const* mymode, ddstring_t** foundPath);

    /**
     * @param mode 'b' = binary mode
     *             't' = text mode
     *             'f' = must be a real file in the local file system.
     *             'x' = skip buffering (used with file-access and metadata-acquire processes).
     */
    DFile* tryOpenFile2(char const* path, char const* mode, size_t baseOffset, bool allowDuplicate);
    DFile* tryOpenFile(char const* path, char const* mode, size_t baseOffset, bool allowDuplicate);
};

} // namespace de

de::FS* App_FileSystem();

extern "C" {
#endif // __cplusplus

/**
 * C wrapper API:
 */

struct filelist_s;
typedef struct filelist_s FileList;

void F_Register(void);

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

LumpInfo const* F_FindInfoForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx);
LumpInfo const* F_FindInfoForLumpNum(lumpnum_t absoluteLumpNum/*, lumpIdx = 0*/);

char const* F_LumpName(lumpnum_t absoluteLumpNum);

size_t F_LumpLength(lumpnum_t absoluteLumpNum);

uint F_LumpLastModified(lumpnum_t absoluteLumpNum);

struct abstractfile_s* F_FindFileForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx);
struct abstractfile_s* F_FindFileForLumpNum(lumpnum_t absoluteLumpNum/*, lumpIdx = 0 */);

void F_Close(struct dfile_s* file);

void F_Delete(struct dfile_s* file);

Str const* F_Path(struct abstractfile_s const* file);

void F_SetCustom(struct abstractfile_s* file, boolean yes);

LumpInfo const* F_LumpInfo(struct abstractfile_s* file, int lumpIdx);

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

int F_AllResourcePaths2(char const* searchPath, int flags, int (*callback) (char const* path, PathDirectoryNodeType type, void* parameters), void* parameters);
int F_AllResourcePaths(char const* searchPath, int flags, int (*callback) (char const* path, PathDirectoryNodeType type, void* parameters)/*, parameters = 0 */);

uint F_CRCNumber(void);

lumpnum_t F_OpenAuxiliary2(char const* fileName, size_t baseOffset);
lumpnum_t F_OpenAuxiliary(char const* fileName/*, baseOffset = 0 */);

void F_CloseAuxiliary(void);

boolean F_DumpLump2(lumpnum_t lumpNum, char const* fileName);
boolean F_DumpLump(lumpnum_t lumpNum/*, fileName = 0*/);

boolean F_Dump(void const* data, size_t size, char const* path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_MAIN_H */
