/**\file fs_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_misc.h" // For M_LimitedStrCat

#include "m_md5.h"
#include "lumpdirectory.h"
#include "lumpfile.h"
#include "wadfile.h"

#include "filedirectory.h"

D_CMD(Dir);
D_CMD(DumpLump);
D_CMD(ListFiles);

typedef struct filelist_node_s {
    boolean loadedForStartup; // For reset.
    abstractfile_t* fsObject;
    struct filelist_node_s* next;
} filelist_node_t;
typedef filelist_node_t* filelist_t;

typedef int lumpdirectoryid_t;

typedef struct {
    lumpname_t lumpName;
    ddstring_t path; // Full path name.
} lumppathmapping_t;

typedef lumppathmapping_t lumpdirectory_record_t;

typedef struct {
    ddstring_t* source; // Full path name.
    ddstring_t* target; // Full path name.
} vdmapping_t;

typedef struct {
    abstractfile_t* file;
} filehandle_t;

#define FILEIDENTIFIERID_T_MAXLEN 16
#define FILEIDENTIFIERID_T_LASTINDEX 15
typedef byte fileidentifierid_t[FILEIDENTIFIERID_T_MAXLEN];

typedef struct fileidentifier_s {
    fileidentifierid_t hash;
} fileidentifier_t;

static boolean inited = false;
static boolean loadingForStartup;

static filehandle_t* files;
static uint filesCount;

static uint numReadFiles = 0;
static uint maxReadFiles = 0;
static fileidentifier_t* readFiles = 0;

// Head of the llist of file nodes.
static filelist_t fileList;

static lumpdirectory_t* zipLumpDirectory;

static lumpdirectory_t* primaryWadLumpDirectory;
static lumpdirectory_t* auxiliaryWadLumpDirectory;
// @c true = one or more files have been opened using the auxiliary directory.
static boolean auxiliaryWadLumpDirectoryInUse;

// Currently selected lump directory.
static lumpdirectory_t* ActiveWadLumpDirectory;

static lumpdirectory_record_t* lumpDirectoryRecordForId(lumpdirectoryid_t id);
static void resetVDirectoryMappings(void);
static void clearLumpDirectory(void);

#define LUMPDIRECTORY_MAXRECORDS    1024
static lumpdirectory_record_t lumpDirectory[LUMPDIRECTORY_MAXRECORDS + 1];
static vdmapping_t* vdMappings;
static uint vdMappingsCount;
static uint vdMappingsMax;

static FILE* findRealFile(const char* path, const char* mymode, ddstring_t** foundPath);
static abstractfile_t* findLumpFile(const char* path, int* lumpIdx);

void F_Register(void)
{
    C_CMD("dir", "", Dir);
    C_CMD("ls", "", Dir); // Alias
    C_CMD("dir", "s*", Dir);
    C_CMD("ls", "s*", Dir); // Alias

    C_CMD("dump", "s", DumpLump);
    C_CMD("listfiles", "", ListFiles);
}

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: WAD module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static fileidentifier_t* findFileIdentifierForId(fileidentifierid_t id)
{
    uint i = 0;
    while(i < numReadFiles)
    {
        if(!memcmp(readFiles[i].hash, id, FILEIDENTIFIERID_T_LASTINDEX))
            return &readFiles[i];
        ++i;
    }
    return 0;
}

static filehandle_t* findUsedFileHandle(void)
{
    uint i;
    for(i = 0; i < filesCount; ++i)
    {
        filehandle_t* fhdl = &files[i];
        if(!fhdl->file)
            return fhdl;
    }
    return 0;
}

static filehandle_t* getFileHandle(void)
{
    filehandle_t* fhdl = findUsedFileHandle();
    if(!fhdl)
    {
        uint firstNewFile = filesCount;

        filesCount *= 2;
        if(filesCount < 16)
            filesCount = 16;

        // Allocate more memory.
        files = (filehandle_t*)realloc(files, sizeof(*files) * filesCount);
        if(!files)
            Con_Error("getFileHandle: Failed on (re)allocation of %lu bytes for file list.",
                (unsigned long) (sizeof(*files) * filesCount));

        // Clear the new handles.
        memset(files + firstNewFile, 0, sizeof(*files) * (filesCount - firstNewFile));

        fhdl = files + firstNewFile;
    }
    return fhdl;
}

static abstractfile_t* getFreeFile(const char* absolutePath)
{
    filehandle_t* fhdl = getFileHandle();
    return (fhdl->file = F_NewFile(absolutePath));
}

static filelist_node_t* allocFileNode(void)
{
    return (filelist_node_t*) malloc(sizeof(filelist_node_t));
}

static void freeFileNode(filelist_node_t* node)
{
    assert(NULL != node);
    free(node);
}

static abstractfile_t* linkFile(abstractfile_t* fsObject, boolean isStartupFile)
{
    filelist_node_t* node = allocFileNode();
    node->fsObject = fsObject;
    node->loadedForStartup = isStartupFile;
    node->next = fileList;
    fileList = node;
    return fsObject;
}

static abstractfile_t* unlinkFile(filelist_node_t* node)
{
    assert(NULL != node);
    {
    abstractfile_t* fsObject = node->fsObject;
    if(NULL != fileList)
    {
        if(node == fileList)
        {
            filelist_node_t* next = fileList->next;
            freeFileNode(fileList);
            fileList = next;
        }
        else
        {
            filelist_node_t* prev = fileList;
            while(prev->next && prev->next != node) { prev = prev->next; }

            { filelist_node_t* next = prev->next->next;
            freeFileNode(prev->next);
            prev->next = next;
            }
        }
    }
    return fsObject;
    }
}

static filelist_node_t* findFileListNodeForName(const char* path)
{
    filelist_node_t* found = NULL;
    if(NULL != path && path[0] && NULL != fileList)
    {
        filelist_node_t* node;
        ddstring_t buf;

        // Transform the given path into one we can process.
        Str_Init(&buf); Str_Set(&buf, path);
        F_FixSlashes(&buf, &buf);

        // Perform the search.
        node = fileList;
        do
        {
            if(!Str_CompareIgnoreCase(AbstractFile_AbsolutePath(node->fsObject), Str_Text(&buf)))
            {
                found = node;
            }
        } while(found == NULL && NULL != (node = node->next));
        Str_Free(&buf);
    }
    return found;
}

static zipfile_t* newZipFile(FILE* file, const char* absolutePath)
{
    filehandle_t* hndl = getFileHandle();
    hndl->file = (abstractfile_t*)ZipFile_New(file, absolutePath);
    return (zipfile_t*) linkFile(F_OpenStreamFile(hndl->file, file, absolutePath), loadingForStartup);
}

static wadfile_t* newWadFile(FILE* file, const char* absolutePath)
{
    filehandle_t* hndl = getFileHandle();
    hndl->file = (abstractfile_t*)WadFile_New(file, absolutePath);
    return (wadfile_t*) linkFile(F_OpenStreamFile(hndl->file, file, absolutePath), loadingForStartup);
}

static lumpfile_t* newLumpFile(abstractfile_t* fsObject, int lumpIdx,
    const char* absolutePath, lumpname_t name, size_t lumpSize)
{
    /// \note We don't create a file handle for lump files.
    return (lumpfile_t*) linkFile(
        F_OpenStreamLump((abstractfile_t*)LumpFile_New(absolutePath, name, lumpSize), fsObject, lumpIdx, false),
        loadingForStartup);
}

static void pruneLumpDirectorysByFile(abstractfile_t* fsObject)
{
    LumpDirectory_PruneByFile(zipLumpDirectory, fsObject);
    LumpDirectory_PruneByFile(primaryWadLumpDirectory, fsObject);
    if(auxiliaryWadLumpDirectoryInUse)
        LumpDirectory_PruneByFile(auxiliaryWadLumpDirectory, fsObject);
}

static boolean removeListFile(filelist_node_t* node)
{
    assert(NULL != node && NULL != node->fsObject);
    switch(AbstractFile_Type(node->fsObject))
    {
    case FT_UNKNOWNFILE: {
        pruneLumpDirectorysByFile(node->fsObject);
        unlinkFile(node);
        // Close the file; we don't need it any more.
        F_Close(AbstractFile_Handle(node->fsObject));
        F_Release(node->fsObject);
        F_Delete(node->fsObject);
        break;
      }
    case FT_ZIPFILE: {
        zipfile_t* zip = (zipfile_t*)node->fsObject;

        ZipFile_ClearLumpCache(zip);
        pruneLumpDirectorysByFile(node->fsObject);
        unlinkFile(node);
        // Close the file; we don't need it any more.
        ZipFile_Close(zip);
        ZipFile_Delete(zip);
        break;
      }
    case FT_WADFILE: {
        wadfile_t* wad = (wadfile_t*)node->fsObject;

        WadFile_ClearLumpCache(wad);
        pruneLumpDirectorysByFile(node->fsObject);
        unlinkFile(node);
        // Close the file; we don't need it any more.
        WadFile_Close(wad);
        WadFile_Delete(wad);
        break;
      }
    case FT_LUMPFILE: {
        lumpfile_t* lump = (lumpfile_t*)node->fsObject;

        LumpFile_ClearLumpCache(lump);
        pruneLumpDirectorysByFile(node->fsObject);
        unlinkFile(node);
        // Close the file; we don't need it any more.
        LumpFile_Close(lump);
        LumpFile_Delete(lump);
        break;
      }
    default:
        Con_Error("WadCollection::removeFile: Invalid file type %i.", AbstractFile_Type(node->fsObject));
        exit(1); // Unreachable.
    }
    return true;
}

static int directoryContainsLumpsFromFile(const lumpinfo_t* info, void* paramaters)
{
    return 1; // Stop iteration we need go no further.
}

static void clearFileList(lumpdirectory_t* directory)
{
    filelist_node_t* next, *node = fileList;
    while(node)
    {
        next = node->next;
        if(NULL == directory || LumpDirectory_Iterate(directory, node->fsObject, directoryContainsLumpsFromFile))
        {
            removeListFile(node);
        }
        node = next;
    }
}

/**
 * Handles conversion to a logical index that is independent of the lump directory currently in use.
 */
static __inline lumpnum_t logicalLumpNum(lumpnum_t lumpNum)
{
    return (lumpNum < 0? -1 :
            ActiveWadLumpDirectory == auxiliaryWadLumpDirectory? lumpNum += AUXILIARY_BASE : lumpNum);
}

static void usePrimaryDirectory(void)
{
    ActiveWadLumpDirectory = primaryWadLumpDirectory;
}

static boolean useAuxiliaryDirectory(void)
{
    if(!auxiliaryWadLumpDirectoryInUse)
        return false;
    ActiveWadLumpDirectory = auxiliaryWadLumpDirectory;
    return true;
}

void F_Init(void)
{
    if(inited) return; // Already been here.

    // This'll force the loader NOT to flag new files as "runtime".
    loadingForStartup = true;

    fileList = NULL;
    zipLumpDirectory = LumpDirectory_New();

    primaryWadLumpDirectory   = LumpDirectory_New();
    auxiliaryWadLumpDirectory = LumpDirectory_New();
    auxiliaryWadLumpDirectoryInUse = false;

    ActiveWadLumpDirectory = primaryWadLumpDirectory;

    inited = true;
}

void F_Shutdown(void)
{
    if(!inited) return;

    F_CloseAuxiliary();
    clearFileList(0);

    LumpDirectory_Delete(primaryWadLumpDirectory), primaryWadLumpDirectory = NULL;
    LumpDirectory_Delete(auxiliaryWadLumpDirectory), auxiliaryWadLumpDirectory = NULL;
    ActiveWadLumpDirectory = NULL;

    LumpDirectory_Delete(zipLumpDirectory), zipLumpDirectory = NULL;

    inited = false;
}

/**
 * Selects which lump directory to use, given a logical lump index.
 * This should be called in all functions that access directories by lump index.
 */
static lumpnum_t chooseDirectory(lumpnum_t lumpNum)
{
    errorIfNotInited("chooseDirectory");
    if(lumpNum >= AUXILIARY_BASE)
    {
        useAuxiliaryDirectory();
        lumpNum -= AUXILIARY_BASE;
    }
    else
    {
        usePrimaryDirectory();
    }
    return lumpNum;
}

void F_EndStartup(void)
{
    errorIfNotInited("F_EndStartup");
    loadingForStartup = false;
    usePrimaryDirectory();
}

static boolean removeFile2(const char* path)
{
    filelist_node_t* node;
    errorIfNotInited("removeFile");
    node = findFileListNodeForName(path);
    if(NULL != node)
        return removeListFile(node);
    return false; // No such file loaded.
}

static boolean removeFile(const char* path)
{
    VERBOSE( Con_Message("Unloading \"%s\"...\n", F_PrettyPath(path)) )
    return removeFile2(path);
}

int F_Reset(void)
{
    int unloadedResources = 0;
    if(inited)
    {
        filelist_node_t* next, *node = fileList;
        while(node)
        {
            next = node->next;
            if(!node->loadedForStartup)
            {
                if(removeFile2(Str_Text(AbstractFile_AbsolutePath(node->fsObject))))
                {
                    ++unloadedResources;
                }
            }
            node = next;
        }
    }
    return unloadedResources;
}

void F_GenerateFileId(const char* str, byte identifier[16])
{
    md5_ctx_t context;
    ddstring_t absPath;

    // First normalize the name.
    Str_Init(&absPath); Str_Set(&absPath, str);
    F_MakeAbsolute(&absPath, &absPath);
    F_FixSlashes(&absPath, &absPath);

#if defined(WIN32) || defined(MACOSX)
    // This is a case insensitive operation.
    strupr(Str_Text(&absPath));
#endif

    md5_init(&context);
    md5_update(&context, (byte*) Str_Text(&absPath), (unsigned int) Str_Length(&absPath));
    md5_final(&context, identifier);

    Str_Free(&absPath);
}

boolean F_CheckFileId(const char* path)
{
    assert(path);
    {
    fileidentifierid_t id;

    if(!F_Access(path))
        return false;

    // Calculate the identifier.
    F_GenerateFileId(path, id);

    if(findFileIdentifierForId(id))
        return false;

    // Allocate a new entry.
    numReadFiles++;
    if(numReadFiles > maxReadFiles)
    {
        if(!maxReadFiles)
            maxReadFiles = 16;
        else
            maxReadFiles *= 2;

        readFiles = (fileidentifier_t*)realloc(readFiles, sizeof(*readFiles) * maxReadFiles);
        if(!readFiles)
            Con_Error("F_CheckFileId: Failed on (re)allocation of %lu bytes while "
                "enlarging readFiles.", (unsigned long) (sizeof(*readFiles) * maxReadFiles));
        memset(readFiles + numReadFiles, 0, sizeof(*readFiles) * (maxReadFiles - numReadFiles));
    }

    memcpy(readFiles[numReadFiles - 1].hash, id, sizeof(id));
    return true;
    }
}

boolean F_ReleaseFileId(const char* path)
{
    fileidentifierid_t id;
    fileidentifier_t* fileIdentifier;

    F_GenerateFileId(path, id);

    fileIdentifier = findFileIdentifierForId(id);
    if(fileIdentifier != 0)
    {
        size_t index = fileIdentifier - readFiles;
        if(index < numReadFiles)
            memmove(readFiles + index, readFiles + index + 1, numReadFiles - index - 1);
        memset(readFiles + numReadFiles, 0, sizeof(*readFiles));
        --numReadFiles;
        return true;
    }
    return false;
}

void F_ResetFileIds(void)
{
    numReadFiles = 0;
}

void F_CloseAll(void)
{
    if(files)
    {
        uint i;
        for(i = 0; i < filesCount; ++i)
        {
            if(!files[i].file) continue;
            F_Delete(files[i].file);
        }
        free(files); files = 0;
    }
    filesCount = 0;
}

void F_Release(abstractfile_t* file)
{
    assert(NULL != file);
    if(files)
    {   // Clear references to the file.
        uint i;
        for(i = 0; i < filesCount; ++i)
        {
            if(files[i].file == file)
                files[i].file = NULL;
        }
    }
}

abstractfile_t* F_NewFile(const char* absolutePath)
{
    abstractfile_t* file = (abstractfile_t*)malloc(sizeof(*file));
    if(!file) Con_Error("F_NewFile: Failed on allocation of %lu bytes for new abstractfile_t.", (unsigned long) sizeof(*file));
    AbstractFile_Init(file, FT_UNKNOWNFILE, NULL, absolutePath);
    return file;
}

void F_Delete(abstractfile_t* file)
{
    assert(NULL != file);
    F_Close(&file->_dfile);
    F_Release(file);
    free(file);
}

/// \note This only works on real files.
static unsigned int readLastModified(const char* path)
{
#ifdef UNIX
    struct stat s;
    stat(path, &s);
    return s.st_mtime;
#endif

#ifdef WIN32
    struct _stat s;
    _stat(path, &s);
    return s.st_mtime;
#endif
}

abstractfile_t* F_OpenStreamLump(abstractfile_t* file, abstractfile_t* container, int lumpIdx, boolean dontBuffer)
{
    assert(NULL != file && NULL != container);
    {
    DFILE* hndl = &file->_dfile;
    const lumpinfo_t* info = F_LumpInfo(container, lumpIdx);
    assert(info);

    // Init and load in the lump data.
    hndl->flags.open = true;
    hndl->hndl = NULL;
    hndl->lastModified = info->lastModified;
    if(!dontBuffer)
    {
        hndl->size = info->size;
        hndl->pos = hndl->data = (uint8_t*)malloc(hndl->size);
        if(NULL == hndl->data)
            Con_Error("F_OpenStreamLump: Failed on allocation of %lu bytes for buffered data.",
                (unsigned long) hndl->size);
#if _DEBUG
        VERBOSE2( Con_Printf("Next FILE read from F_OpenStreamLump.\n") )
#endif
        F_ReadLumpSection(container, lumpIdx, (uint8_t*)hndl->data, 0, info->size);
    }
    return file;
    }
}

abstractfile_t* F_OpenStreamFile(abstractfile_t* file, FILE* sysHndl, const char* path)
{
    assert(file && sysHndl && path && path[0]);
    {
    DFILE* hndl = &file->_dfile;
    hndl->hndl = sysHndl;
    hndl->data = NULL;
    hndl->flags.open = true;
    hndl->lastModified = readLastModified(path);
    return file;
    }
}

boolean F_IsValidLumpNum(lumpnum_t absoluteLumpNum)
{
    errorIfNotInited("F_IsValidLumpNum");
    { lumpnum_t lumpNum = chooseDirectory(absoluteLumpNum);
    return LumpDirectory_IsValidIndex(ActiveWadLumpDirectory, lumpNum); }
}

lumpnum_t F_CheckLumpNumForName(const char* name, boolean silent)
{
    lumpnum_t lumpNum = -1;
    errorIfNotInited("F_CheckLumpNumForName");
    if(name && name[0])
    {
        // We have to check both the primary and auxiliary caches because
        // we've only got a name and don't know where it is located. Start with
        // the auxiliary lumps because they take precedence.
        if(useAuxiliaryDirectory())
        {
            lumpNum = LumpDirectory_IndexForName(ActiveWadLumpDirectory, name);
        }

        // Found it yet?
        if(lumpNum < 0)
        {
            usePrimaryDirectory();
            lumpNum = LumpDirectory_IndexForName(ActiveWadLumpDirectory, name);
        }

        if(!silent && lumpNum < 0)
            Con_Message("Warning:F_CheckLumpNumForName: Lump \"%s\" not found.\n", name);
    }
    else if(!silent)
    {
        Con_Message("Warning:F_CheckLumpNumForName: Empty name, returning invalid lumpnum.\n");
    }
    return logicalLumpNum(lumpNum);
}

boolean F_LumpIsFromIWAD(lumpnum_t absoluteLumpNum)
{
    abstractfile_t* fsObject = F_FindFileForLumpNum(absoluteLumpNum);
    if(fsObject)
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:     return  ZipFile_IsIWAD( (zipfile_t*)fsObject);
    case FT_WADFILE:     return  WadFile_IsIWAD( (wadfile_t*)fsObject);
    case FT_LUMPFILE:    return LumpFile_IsIWAD((lumpfile_t*)fsObject);
    default:
        Con_Error("F_LumpIsFromIWAD: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
    return false;
}

abstractfile_t* F_FindFileForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx)
{
    errorIfNotInited("F_FindFileForLumpNum2");
    { lumpnum_t translated = chooseDirectory(absoluteLumpNum);
    if(lumpIdx) *lumpIdx = LumpDirectory_LumpIndex(ActiveWadLumpDirectory, translated);
    return LumpDirectory_SourceFile(ActiveWadLumpDirectory, translated);
    }
}

abstractfile_t* F_FindFileForLumpNum(lumpnum_t absoluteLumpNum)
{
    return F_FindFileForLumpNum2(absoluteLumpNum, NULL);
}

const lumpinfo_t* F_FindInfoForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx_)
{
    int lumpIdx;
    abstractfile_t* fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
    if(!fsObject) return NULL;
    // Does caller whant to know the lump index?
    if(lumpIdx_) *lumpIdx_ = lumpIdx;
    return F_LumpInfo(fsObject, lumpIdx);
}

const lumpinfo_t* F_FindInfoForLumpNum(lumpnum_t absoluteLumpNum)
{
    return F_FindInfoForLumpNum2(absoluteLumpNum, NULL);
}

const char* F_LumpName(lumpnum_t absoluteLumpNum)
{
    const lumpinfo_t* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(info) return info->name;
    return "";
}

size_t F_LumpLength(lumpnum_t absoluteLumpNum)
{
    const lumpinfo_t* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(info) return info->size;
    return 0;
}

const char* F_LumpSourceFile(lumpnum_t absoluteLumpNum)
{
    abstractfile_t* fsObject = F_FindFileForLumpNum(absoluteLumpNum);
    if(fsObject) return Str_Text(AbstractFile_AbsolutePath(fsObject));
    return "";
}

uint F_LumpLastModified(lumpnum_t absoluteLumpNum)
{
    const lumpinfo_t* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(info) return info->lastModified;
    return 0;
}

int F_LumpCount(void)
{
    if(inited)
        return LumpDirectory_NumLumps(ActiveWadLumpDirectory);
    return 0;
}

lumpnum_t F_OpenAuxiliary3(const char* path, FILE* prevOpened, boolean silent)
{
    ddstring_t* foundPath = NULL;

    errorIfNotInited("F_OpenAuxiliary3");

    if(prevOpened == NULL)
    {
        // Try to open the file.
        ddstring_t searchPath;
 
        if(!path || !path[0])
            return -1;

        // Make it a full path.
        Str_Init(&searchPath); Str_Set(&searchPath, path);
        F_FixSlashes(&searchPath, &searchPath);
        F_ExpandBasePath(&searchPath, &searchPath);
        // We must have an absolute path, so prepend the current working directory if necessary.
        F_PrependWorkPath(&searchPath, &searchPath);

        /// \todo Allow opening WAD/ZIP files from lumps in other containers.
        prevOpened = findRealFile(Str_Text(&searchPath), "rbx", &foundPath);
        Str_Free(&searchPath);
        if(!prevOpened)
        {
            Str_Delete(foundPath);
            if(!silent)
            {
                Con_Message("Warning:F_OpenAuxiliary: Resource \"%s\" not found, aborting.\n", path);
            }
            return -1;
        }
    }
    else
    {
        foundPath = Str_New(), Str_Set(foundPath, path);
    }

    if(WadFile_Recognise(prevOpened))
    {
        if(auxiliaryWadLumpDirectoryInUse)
        {
            F_CloseAuxiliary();
        }
        ActiveWadLumpDirectory = auxiliaryWadLumpDirectory;
        auxiliaryWadLumpDirectoryInUse = true;

        // Get a new file record.
        WadFile_PublishLumpsToDirectory(newWadFile(prevOpened, Str_Text(foundPath)), ActiveWadLumpDirectory);
        Str_Delete(foundPath);
        return AUXILIARY_BASE;
    }

    if(!silent)
    {
        if(path && path[0])
            Con_Message("Warning:F_OpenAuxiliary: Resource \"%s\" cannot be found or does not appear to be of recognised format.\n", path);
        else
            Con_Message("Warning:F_OpenAuxiliary: Cannot open a resource with neither a path nor a handle to it.\n");
    }
    if(foundPath)
        Str_Delete(foundPath);
    return -1;
}

lumpnum_t F_OpenAuxiliary2(const char* path, FILE* prevOpened)
{
    return F_OpenAuxiliary3(path, prevOpened, false);
}

lumpnum_t F_OpenAuxiliary(const char* path)
{
    return F_OpenAuxiliary2(path, NULL);
}

void F_CloseAuxiliary(void)
{
    errorIfNotInited("F_CloseAuxiliary");
    if(useAuxiliaryDirectory())
    {
        clearFileList(auxiliaryWadLumpDirectory);
        auxiliaryWadLumpDirectoryInUse = false;
    }
    usePrimaryDirectory();
}

const lumpinfo_t* F_LumpInfo(abstractfile_t* fsObject, int lumpIdx)
{
    assert(fsObject);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:    return  ZipFile_LumpInfo( (zipfile_t*)fsObject, lumpIdx);
    case FT_WADFILE:    return  WadFile_LumpInfo( (wadfile_t*)fsObject, lumpIdx);
    case FT_LUMPFILE:   return LumpFile_LumpInfo((lumpfile_t*)fsObject, lumpIdx);
    default:
        Con_Error("F_LumpInfo: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

size_t F_ReadLumpSection(abstractfile_t* fsObject, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    assert(fsObject);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:  return  ZipFile_ReadLumpSection( (zipfile_t*)fsObject, lumpIdx, buffer, startOffset, length);
    case FT_WADFILE:  return  WadFile_ReadLumpSection( (wadfile_t*)fsObject, lumpIdx, buffer, startOffset, length);
    case FT_LUMPFILE: return LumpFile_ReadLumpSection((lumpfile_t*)fsObject, lumpIdx, buffer, startOffset, length);
    default:
        Con_Error("F_ReadLumpSection: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

const uint8_t* F_CacheLump(abstractfile_t* fsObject, int lumpIdx, int tag)
{
    assert(fsObject);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:    return  ZipFile_CacheLump( (zipfile_t*)fsObject, lumpIdx, tag);
    case FT_WADFILE:    return  WadFile_CacheLump( (wadfile_t*)fsObject, lumpIdx, tag);
    case FT_LUMPFILE:   return LumpFile_CacheLump((lumpfile_t*)fsObject, lumpIdx, tag);
    default:
        Con_Error("F_CacheLump: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

void F_CacheChangeTag(abstractfile_t* fsObject, int lumpIdx, int tag)
{
    assert(fsObject);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:    ZipFile_ChangeLumpCacheTag( (zipfile_t*)fsObject, lumpIdx, tag); break;
    case FT_WADFILE:    WadFile_ChangeLumpCacheTag( (wadfile_t*)fsObject, lumpIdx, tag); break;
    case FT_LUMPFILE:  LumpFile_ChangeLumpCacheTag((lumpfile_t*)fsObject, lumpIdx, tag); break;
    default:
        Con_Error("F_CacheChangeTag: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

boolean F_DumpLump(abstractfile_t* fsObject, int lumpIdx, const char* path)
{
    char buf[LUMPNAME_T_LASTINDEX + 4/*.ext*/ + 1];
    const lumpinfo_t* info = F_LumpInfo(fsObject, lumpIdx);
    const char* fname;
    FILE* outFile;

    if(!info) return false;

    if(path && path[0])
    {
        fname = path;
    }
    else
    {
        memset(buf, 0, sizeof(buf));
        dd_snprintf(buf, 13, "%s.lmp", info->name);
        fname = buf;
    }

    outFile = fopen(fname, "wb");
    if(!outFile)
    {
        Con_Printf("Warning: Failed to open %s for writing (%s), aborting.\n", fname, strerror(errno));
        return false;
    }

    fwrite((void*)F_CacheLump(fsObject, lumpIdx, PU_APPSTATIC), 1, info->size, outFile);
    fclose(outFile);
    F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);

    Con_Printf("%s dumped to %s.\n", info->name, fname);
    return true;
}

uint F_CRCNumber(void)
{
    filelist_node_t* node;
    wadfile_t* wad;
    errorIfNotInited("F_CRCNumber");
    // Find the IWAD's record.
    for(node = fileList; NULL != node; node = node->next)
    {
        if(FT_WADFILE != AbstractFile_Type(node->fsObject)) continue;

        wad = (wadfile_t*)node->fsObject;
        if(WadFile_IsIWAD(wad))
            return WadFile_CalculateCRC(wad);
    }
    return 0;
}

void F_GetPWADFileNames(char* outBuf, size_t outBufSize, char delimiter)
{
    ddstring_t buf;

    if(NULL == outBuf || 0 == outBufSize) return;

    memset(outBuf, 0, outBufSize);

    if(!inited) return;

    /// \fixme Do not use the global fileList, pull records from LumpDirectory(s)
    Str_Init(&buf);
    { filelist_node_t* node;
    for(node = fileList; NULL != node; node = node->next)
    {
        if(FT_WADFILE != AbstractFile_Type(node->fsObject) ||
           WadFile_IsIWAD((wadfile_t*)node->fsObject)) continue;

        Str_Clear(&buf);
        F_FileNameAndExtension(&buf, Str_Text(AbstractFile_AbsolutePath(node->fsObject)));
        if(stricmp(Str_Text(&buf) + Str_Length(&buf) - 3, "lmp"))
            M_LimitedStrCat(outBuf, Str_Text(&buf), 64, delimiter, outBufSize);
    }}
    Str_Free(&buf);
}

void F_PrintLumpDirectory(void)
{
    if(!inited) return;
    // Always the primary directory.
    LumpDirectory_Print(primaryWadLumpDirectory);
}

const char* Zip_SourceFile(lumpnum_t lumpNum)
{
    return Str_Text(AbstractFile_AbsolutePath(LumpDirectory_SourceFile(zipLumpDirectory, lumpNum)));
}

lumpnum_t Zip_Find(const char* searchPath)
{
    lumpnum_t result = -1;
    if(inited)
    {
        ddstring_t absSearchPath;
        // Convert to an absolute path.
        Str_Init(&absSearchPath); Str_Set(&absSearchPath, searchPath);
        F_PrependBasePath(&absSearchPath, &absSearchPath);

        // Perform the search.
        result = LumpDirectory_IndexForPath(zipLumpDirectory, Str_Text(&absSearchPath));
        Str_Free(&absSearchPath);
    }
    return result;
}

uint F_GetLastModified(const char* fileName)
{
    // Try to open the file, but don't buffer any contents.
    abstractfile_t* file = F_Open(fileName, "rx");
    unsigned int modified = 0;

    if(!file)
        return 0;

    modified = AbstractFile_LastModified(file);
    F_Delete(file);
    return modified;
}

/// @return  @c true, if the mapping matched the path.
boolean F_MapPath(ddstring_t* path, vdmapping_t* vd)
{
    assert(NULL != path && NULL != vd);
    if(!strnicmp(Str_Text(path), Str_Text(vd->target), Str_Length(vd->target)))
    {
        // Replace the beginning with the source path.
        ddstring_t temp;
        Str_Init(&temp);
        Str_Set(&temp, Str_Text(vd->source));
        Str_PartAppend(&temp, Str_Text(path), Str_Length(vd->target), Str_Length(path) - Str_Length(vd->target));
        Str_Copy(path, &temp);
        Str_Free(&temp);
        return true;
    }
    return false;
}

typedef struct {
    /// Callback to make for each processed node.
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters);

    /// Data passed to the callback.
    void* paramaters;

    /// Current search pattern.
    const ddstring_t* pattern;
} findzipfileworker_paramaters_t;

typedef struct foundentry_s {
    ddstring_t path;
    int attrib;
} foundentry_t;

static int C_DECL compareFoundEntryByPath(const void* a, const void* b)
{
    return Str_CompareIgnoreCase(&((const foundentry_t*)a)->path, Str_Text(&((const foundentry_t*)b)->path));
}

/// Collect a list of paths including those which have been mapped.
static foundentry_t* collectLocalPaths(const ddstring_t* searchPath, int* retCount)
{
    ddstring_t wildPath, origWildPath;
    foundentry_t* found = NULL;
    int count = 0, max = 0;
    finddata_t fd;

    Str_Init(&origWildPath);
    Str_Appendf(&origWildPath, "%s*", Str_Text(searchPath));

    Str_Init(&wildPath);
    { int i;
    for(i = -1; i < (int)vdMappingsCount; ++i)
    {
        Str_Copy(&wildPath, &origWildPath);

        // Possible mapping?
        if(i >= 0 && !F_MapPath(&wildPath, &vdMappings[i]))
            continue; // Not mapped.

        if(!myfindfirst(Str_Text(&wildPath), &fd))
        {   // First path found.
            do
            {
                // Ignore relative directory symbolics.
                if(strcmp(fd.name, ".") && strcmp(fd.name, ".."))
                {
                    if(count >= max)
                    {
                        if(0 == max)
                            max = 16;
                        else
                            max *= 2;
                        found = realloc(found, sizeof(*found) * max);
                    }
                    Str_Init(&found[count].path);
                    Str_Set(&found[count].path, fd.name);
                    if((fd.attrib & A_SUBDIR) && DIR_SEP_CHAR != Str_RAt(&found[count].path, 0))
                        Str_AppendChar(&found[count].path, DIR_SEP_CHAR);
                    found[count].attrib = fd.attrib;
                    ++count;
                }
            } while(!myfindnext(&fd));
        }
        myfindend(&fd);
    }}

    Str_Free(&origWildPath);
    Str_Free(&wildPath);

    if(retCount)
        *retCount = count;
    if(0 != count)
        return found;
    return NULL;
}

static int iterateLocalPaths(const ddstring_t* pattern, const ddstring_t* searchPath,
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters),
    void* paramaters)
{
    assert(pattern && searchPath && !Str_IsEmpty(searchPath) && callback);
    {
    int result = 0, count;
    foundentry_t* foundPaths = collectLocalPaths(searchPath, &count);

    if(NULL != foundPaths)
    {
        ddstring_t path, localPattern;

        // Sort all the foundPaths entries.
        qsort(foundPaths, count, sizeof(foundentry_t), compareFoundEntryByPath);

        Str_Init(&localPattern);
        Str_Appendf(&localPattern, "%s%s", Str_Text(searchPath), Str_Text(pattern));

        Str_Init(&path);
        {int i;
        for(i = 0; i < count; ++i)
        {
            // Is the caller's iteration still in progress?
            if(0 == result)
            {
                // Compose the full path to the found file/directory.
                Str_Clear(&path);
                Str_Appendf(&path, "%s%s", Str_Text(searchPath), Str_Text(&foundPaths[i].path));

                // Does this match the pattern?
                if(F_MatchFileName(Str_Text(&path), Str_Text(&localPattern)))
                {
                    // Pass this path to the caller.
                    result = callback(&path, (foundPaths[i].attrib & A_SUBDIR)? PT_BRANCH : PT_LEAF, paramaters);
                }
            }

            // We're done with this path.
            Str_Free(&foundPaths[i].path);
        }}
        Str_Free(&path);
        Str_Free(&localPattern);
        free(foundPaths);
    }

    return result;
    }
}

static int findZipFileWorker(const lumpinfo_t* lumpInfo, void* paramaters)
{
    assert(NULL != lumpInfo && NULL != paramaters);
    {
    findzipfileworker_paramaters_t* p = (findzipfileworker_paramaters_t*)paramaters;
    if(F_MatchFileName(Str_Text(&lumpInfo->path), Str_Text(p->pattern)))
    {
        return p->callback(&lumpInfo->path, PT_LEAF, p->paramaters);
    }
    return 0; // Continue search.
    }
}

int F_AllResourcePaths2(const char* rawSearchPattern,
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters),
    void* paramaters)
{
    ddstring_t searchPattern, searchName, searchDirectory;
    int result = 0;

    // First normalize the raw search pattern into one we can process.
    Str_Init(&searchPattern); Str_Set(&searchPattern, rawSearchPattern);
    Str_Strip(&searchPattern);
    F_FixSlashes(&searchPattern, &searchPattern);
    F_ExpandBasePath(&searchPattern, &searchPattern);

    // An absolute path is required so resolve relative to the base path.
    // if not already absolute.
    F_PrependBasePath(&searchPattern, &searchPattern);

    // Check the Zip directory.
    { findzipfileworker_paramaters_t p;
    p.callback = callback;
    p.paramaters = paramaters;
    p.pattern = &searchPattern;

    result = LumpDirectory_Iterate2(zipLumpDirectory, NULL, findZipFileWorker, (void*)&p);
    if(0 != result)
    {   // Find didn't finish.
        goto searchEnded;
    }}

    // Check the dir/WAD direcs.
    { int i;
    for(i = 0; Str_Length(&lumpDirectory[i].path) != 0; ++i)
    {
        lumpdirectory_record_t* rec = &lumpDirectory[i];
        if(!F_MatchFileName(Str_Text(&rec->path), Str_Text(&searchPattern)))
            continue;
        result = callback(&rec->path, PT_LEAF, paramaters);
        if(0 != result)
            goto searchEnded;
    }}

    /**
     * Check real files on the search path.
     * Our existing normalized search pattern cannot be used as-is due to the
     * interface of the search algorithm requiring that the name and directory of
     * the pattern be specified separately.
     */

    // Extract just the name and/or extension.
    Str_Init(&searchName);
    F_FileNameAndExtension(&searchName, Str_Text(&searchPattern));

    // Extract the directory path.
    Str_Init(&searchDirectory);
    F_FileDir(&searchDirectory, &searchPattern);

    result = iterateLocalPaths(&searchName, &searchDirectory, callback, paramaters);

    Str_Free(&searchName);
    Str_Free(&searchDirectory);

searchEnded:
    Str_Free(&searchPattern);
    return result;
}

int F_AllResourcePaths(const char* searchPath,
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters))
{
    return F_AllResourcePaths2(searchPath, callback, 0);
}

static FILE* findRealFile(const char* path, const char* mymode, ddstring_t** foundPath)
{
    ddstring_t* mapped;
    char mode[8];
    FILE* file;

    strcpy(mode, "r"); // Open for reading.
    if(strchr(mymode, 't'))
        strcat(mode, "t");
    if(strchr(mymode, 'b'))
        strcat(mode, "b");

    // Try opening as a real file.
    file = fopen(path, mode);
    if(file)
    {
        if(foundPath)
        {
            *foundPath = Str_New();
            Str_Set(*foundPath, path);
        }
        return file;
    }

    // Any applicable virtual directory mappings?
    if(vdMappingsCount == 0)
        return NULL;

    mapped = Str_New();
    { uint i;
    for(i = 0; i < vdMappingsCount; ++i)
    {
        Str_Set(mapped, path);
        if(!F_MapPath(mapped, &vdMappings[i]))
            continue;
        // The mapping was successful.
        file = fopen(Str_Text(mapped), mode);
        if(file)
        {
            VERBOSE( Con_Message("F_OpenFile: \"%s\" opened as %s.\n", F_PrettyPath(Str_Text(mapped)), path) )
            if(foundPath)
                *foundPath = mapped;
            else
                Str_Delete(mapped);
            return file;
        }
    }}
    Str_Delete(mapped);
    return NULL;
}

static abstractfile_t* findLumpFile(const char* path, int* lumpIdx)
{
    ddstring_t absSearchPath;
    lumpnum_t lumpNum;

    if(!path || !path[0] || !inited)
        return false;

    /**
     * First check the Zip directory.
     */

    // Convert to an absolute path.
    Str_Init(&absSearchPath); Str_Set(&absSearchPath, path);
    F_PrependBasePath(&absSearchPath, &absSearchPath);

    // Perform the search.
    lumpNum = LumpDirectory_IndexForPath(zipLumpDirectory, Str_Text(&absSearchPath));
    if(lumpNum >= 0)
    {
        abstractfile_t* fsObject = LumpDirectory_SourceFile(zipLumpDirectory, lumpNum);
        if(lumpIdx)
            *lumpIdx = LumpDirectory_LumpIndex(zipLumpDirectory, lumpNum);
        Str_Free(&absSearchPath);
        return fsObject;
    }

    /**
     * Next try the dir/WAD redirects.
     */

    // We must have an absolute path, so prepend the current
    // working directory if necessary.
    Str_Set(&absSearchPath, path);
    F_PrependWorkPath(&absSearchPath, &absSearchPath);
    { int i = 0;
    for(i = 0; !Str_IsEmpty(&lumpDirectory[i].path); ++i)
    {
        lumpdirectory_record_t* rec = &lumpDirectory[i];
        if(!Str_CompareIgnoreCase(&rec->path, Str_Text(&absSearchPath)))
        {
            lumpnum_t lumpNum = -1;

            // We have to check both the primary and auxiliary caches because
            // we've only got a name and don't know where it is located. Start with
            // the auxiliary lumps because they take precedence.
            if(useAuxiliaryDirectory())
            {
                lumpNum = LumpDirectory_IndexForName(ActiveWadLumpDirectory, rec->lumpName);
            }

            // Found it yet?
            if(lumpNum < 0)
            {
                usePrimaryDirectory();
                lumpNum = LumpDirectory_IndexForName(ActiveWadLumpDirectory, rec->lumpName);
            }

            if(lumpNum >= 0)
            {
                abstractfile_t* fsObject = LumpDirectory_SourceFile(ActiveWadLumpDirectory, lumpNum);
                if(lumpIdx)
                    *lumpIdx = LumpDirectory_LumpIndex(ActiveWadLumpDirectory, lumpNum);
                Str_Free(&absSearchPath);
                return fsObject;
            }
        }
    }}

    Str_Free(&absSearchPath);
    return NULL;
}

abstractfile_t* F_OpenLump(lumpnum_t absoluteLumpNum, boolean dontBuffer)
{
    int lumpIdx;
    abstractfile_t* fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
    if(fsObject)
    {
        const lumpinfo_t* info = F_LumpInfo(fsObject, lumpIdx);
        /// \todo All lumps should be attributed with an absolute file path not just those from Zips.
        /// \note We don't create a file handle for lump files.
        return F_OpenStreamLump((abstractfile_t*)LumpFile_New(Str_Text(&info->path), info->name, info->size), fsObject, lumpIdx, dontBuffer);
    }
    return 0;
}

abstractfile_t* F_Open(const char* path, const char* mode)
{
    ddstring_t searchPath, *foundPath = NULL;
    boolean dontBuffer, reqRealFile;
    FILE* file = NULL;

    if(!path || !path[0])
        return NULL;

    if(NULL == mode) mode = "";

    dontBuffer  = (strchr(mode, 'x') != NULL);
    reqRealFile = (strchr(mode, 'f') != NULL);

    // Make it a full path.
    Str_Init(&searchPath); Str_Set(&searchPath, path);
    F_FixSlashes(&searchPath, &searchPath);
    F_ExpandBasePath(&searchPath, &searchPath);

    // First check for lumps?
    if(!reqRealFile)
    {
        int lumpIdx;
        abstractfile_t* fsContainer = findLumpFile(Str_Text(&searchPath), &lumpIdx);
        if(fsContainer)
        {
            const lumpinfo_t* info = F_LumpInfo(fsContainer, lumpIdx);
            Str_Free(&searchPath);
            return F_OpenStreamLump(getFreeFile(Str_Text(&info->path)), fsContainer, lumpIdx, dontBuffer);
        }
    }

    // Try to open as a real file then.
    // We must have an absolute path, so prepend the current working directory if necessary.
    F_PrependWorkPath(&searchPath, &searchPath);
    file = findRealFile(Str_Text(&searchPath), mode, &foundPath);
    Str_Free(&searchPath);
    if(file)
    {
        abstractfile_t* fsObject = F_OpenStreamFile(getFreeFile(Str_Text(foundPath)), file, Str_Text(foundPath));
        Str_Delete(foundPath);
        return fsObject;
    }
    // Not found.
    return NULL;
}

int F_Access(const char* path)
{
    // Open for reading, but don't buffer anything.
    abstractfile_t* file = F_Open(path, "rx");
    if(file)
    {
        F_Delete(file);
        return true;
    }
    return false;
}

static __inline void initLumpPathMapping(lumppathmapping_t* lpm)
{
    assert(lpm);
    Str_Init(&lpm->path);
    memset(lpm->lumpName, 0, sizeof(lpm->lumpName));
}

static __inline void clearLumpPathMapping(lumppathmapping_t* lpm)
{
    Str_Free(&lpm->path);
    memset(lpm->lumpName, 0, sizeof(lpm->lumpName));
}

static lumpdirectoryid_t getUnusedLumpDirectoryId(void)
{
    lumpdirectoryid_t id;
    // \fixme Why no dynamic allocation?
    for(id = 0; Str_Length(&lumpDirectory[id].path) != 0 && id < LUMPDIRECTORY_MAXRECORDS; ++id);
    if(id == LUMPDIRECTORY_MAXRECORDS)
    {
        Con_Error("getUnusedLumpDirectoryId: Not enough records.\n");
    }
    return id;
}

static int toLumpDirectoryId(const char* path)
{
    if(path && path[0])
    {
        int i;
        for(i = 0; Str_Length(&lumpDirectory[i].path) != 0; ++i)
        {
            if(!Str_CompareIgnoreCase(&lumpDirectory[i].path, path))
                return i;
        }
    }
    return -1;
}

static lumpdirectory_record_t* newLumpDirectoryRecord(lumpdirectoryid_t id)
{
    return lumpDirectoryRecordForId(id);
}

static lumpdirectory_record_t* lumpDirectoryRecordForId(lumpdirectoryid_t id)
{
    if(id >= 0 && id < LUMPDIRECTORY_MAXRECORDS)
        return &lumpDirectory[id];
    return 0;
}

static void clearLumpDirectory(void)
{
    int i;
    for(i = 0; Str_Length(&lumpDirectory[i].path) != 0; ++i)
    {
        lumpdirectory_record_t* rec = &lumpDirectory[i];
        Str_Free(&rec->path);
    }
}

/**
 * The path names are converted to full paths before adding to the table.
 */
static void addLumpDirectoryMapping(const char* lumpName, const ddstring_t* symbolicPath)
{
    assert(lumpName && symbolicPath);
    {
    lumpdirectory_record_t* rec;
    ddstring_t fullPath, path;

    if(!lumpName[0] || Str_Length(symbolicPath) == 0)
        return;

    // Convert the symbolic path into a real path.
    Str_Init(&path);
    F_ResolveSymbolicPath(&path, symbolicPath);

    // Since the path might be relative, let's explicitly make the path absolute.
    { char* full;
    Str_Init(&fullPath);
    Str_Set(&fullPath, full = _fullpath(0, Str_Text(&path), 0)); free(full);
    }

    // If this path already exists, we'll just update the lump name.
    rec = lumpDirectoryRecordForId(toLumpDirectoryId(Str_Text(&fullPath)));
    if(!rec)
    {   // Acquire a new record.
        rec = newLumpDirectoryRecord(getUnusedLumpDirectoryId());
        assert(rec);
        Str_Copy(&rec->path, &fullPath);
    }
    memcpy(rec->lumpName, lumpName, sizeof(rec->lumpName));
    rec->lumpName[LUMPNAME_T_LASTINDEX] = '\0';

    Str_Free(&fullPath);
    Str_Free(&path);

    VERBOSE( Con_Message("addLumpDirectoryMapping: \"%s\" -> %s\n", rec->lumpName,
        F_PrettyPath(Str_Text(&rec->path))) )
    }
}

static void resetVDirectoryMappings(void)
{
    if(vdMappings)
    {
        // Free the allocated memory.
        uint i;
        for(i = 0; i < vdMappingsCount; ++i)
        {
            Str_Delete(vdMappings[i].source);
            Str_Delete(vdMappings[i].target);
        }
        free(vdMappings); vdMappings = 0;
    }
    vdMappingsCount = vdMappingsMax = 0;
}

static abstractfile_t* tryAddZipFile(FILE* file, const char* absolutePath)
{
    zipfile_t* zip = NULL;
    errorIfNotInited("tryAddZipFile");
    if(ZipFile_Recognise(file))
    {
        // Get a new file record.
        zip = newZipFile(file, absolutePath);
    }
    return (abstractfile_t*)zip;
}

static abstractfile_t* tryAddWadFile(FILE* file, const char* absolutePath)
{
    wadfile_t* wad = NULL;
    errorIfNotInited("tryAddWadFile");
    if(WadFile_Recognise(file))
    {
        // Get a new file record.
        wad = newWadFile(file, absolutePath);
    }
    return (abstractfile_t*)wad;
}

static abstractfile_t* addLumpFile(abstractfile_t* fsObject, int lumpIdx,
    const char* absolutePath, boolean isDehackedPatch)
{
    const lumpinfo_t* info = F_LumpInfo(fsObject, lumpIdx);
    lumpname_t name;

    errorIfNotInited("addLumpFile");

    // Prepare the name of this single-lump file.
    if(isDehackedPatch)
    {
        strncpy(name, "DEHACKED", LUMPNAME_T_MAXLEN);
    }
    else
    {
        // Is there a prefix to be omitted in the name?
        char* slash = strrchr(absolutePath, DIR_SEP_CHAR);
        int offset = 0;
        // The slash must not be too early in the string.
        if(slash && slash >= absolutePath + 2)
        {
            // Good old negative indices.
            if(slash[-2] == '.' && slash[-1] >= '1' && slash[-1] <= '9')
            {
                offset = slash[-1] - '1' + 1;
            }
        }
        F_ExtractFileBase2(name, absolutePath, LUMPNAME_T_MAXLEN, offset);
    }

    return (abstractfile_t*)newLumpFile(fsObject, lumpIdx, absolutePath, name, info->size);
}

static abstractfile_t* F_AddFile2(FILE* handle, const char* absolutePath)
{
    assert(NULL != handle && NULL != absolutePath && absolutePath[0]);
    {
    struct filehandler_s {
        resourcetype_t resourceType;
        abstractfile_t* (*tryLoadFile)(FILE* handle, const char* absolutePath);
    } static const handlers[] = {
        { RT_ZIP,  tryAddZipFile },
        { RT_WAD,  tryAddWadFile },
        { RT_NONE, NULL }
    }, *hdlr = NULL;
    resourcetype_t resourceType = F_GuessResourceTypeByName(absolutePath);
    abstractfile_t* fsObject = NULL;

    VERBOSE( Con_Message("Loading \"%s\"...\n", F_PrettyPath(absolutePath)) )

    // Firstly try the expected format given the file name.
    for(hdlr = handlers; NULL != hdlr->tryLoadFile; hdlr++)
    {
        if(hdlr->resourceType != resourceType) continue;

        fsObject = hdlr->tryLoadFile(handle, absolutePath);
        break;
    }

    // If not yet loaded; try each recognisable format.
    /// \todo Order here should be determined by the resource locator.
    { int n = 0;
    while(NULL == fsObject && NULL != handlers[n].tryLoadFile)
    {
        if(hdlr == &handlers[n]) continue; // We already know its not in this format.

        fsObject = handlers[n++].tryLoadFile(handle, absolutePath);
    }}
    return fsObject;
    }
}

boolean F_AddFile(const char* fileName, boolean allowDuplicate)
{
    abstractfile_t* fsObject;
    ddstring_t searchPath, *foundPath;
    int lumpIdx;
    FILE* file;

    // Filename given?
    if(!fileName || !fileName[0])
        return false;

    // Make it a full fileName.
    Str_Init(&searchPath); Str_Set(&searchPath, fileName);
    F_FixSlashes(&searchPath, &searchPath);
    F_ExpandBasePath(&searchPath, &searchPath);

    // First check for lumps?
    fsObject = findLumpFile(Str_Text(&searchPath), &lumpIdx);
    if(fsObject)
    {
        resourcetype_t type;

        // Do not read files twice.
        if(!allowDuplicate && !F_CheckFileId(fileName))
        {
            Con_Message("\"%s\" already loaded.\n", F_PrettyPath(fileName));
            return false;
        }

        // DeHackEd patch files require special handling...
        type = F_GuessResourceTypeByName(fileName);

        fsObject = (abstractfile_t*)addLumpFile(fsObject, lumpIdx, Str_Text(&searchPath), (type == RT_DEH));
        LumpFile_PublishLumpsToDirectory((lumpfile_t*)fsObject, ActiveWadLumpDirectory);
        Str_Free(&searchPath);
        return true;
    }

    // Try to open as a real file then. We must have an absolute path, so
    // prepend the current working directory if necessary.
    F_PrependWorkPath(&searchPath, &searchPath);
    file = findRealFile(Str_Text(&searchPath), "rb", &foundPath);
    if(!file)
    {
        Str_Free(&searchPath);
        Con_Message("Warning:F_AddFile: Resource \"%s\" not found, aborting.\n", fileName);
        return false;
    }

    // Do not read files twice.
    if(!allowDuplicate && !F_CheckFileId(Str_Text(foundPath)))
    {
        Str_Free(&searchPath);
        Con_Message("\"%s\" already loaded.\n", F_PrettyPath(Str_Text(foundPath)));
        return false;
    }

    // Search path is used here rather than found path as the latter may have
    // been mapped to another location. We want the file to be attributed with
    // the path it is to be known by throughout the virtual file system.
    fsObject = F_AddFile2(file, Str_Text(&searchPath));
    Str_Free(&searchPath);
    Str_Delete(foundPath);

    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:
        ZipFile_PublishLumpsToDirectory((zipfile_t*)fsObject, zipLumpDirectory);
        break;
    case FT_WADFILE: {
        wadfile_t* wad = (wadfile_t*)fsObject;
        WadFile_PublishLumpsToDirectory(  (wadfile_t*)fsObject, ActiveWadLumpDirectory);
        // Print the 'CRC' number of the IWAD, so it can be identified.
        /// \todo Do not do this here.
        if(WadFile_IsIWAD(wad))
            Con_Message("  IWAD identification: %08x\n", WadFile_CalculateCRC(wad));
        break;
      }
    default:
        Con_Error("F_AddFile: Invalid file type %i.", (int) AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
    return true;
}

boolean F_AddFiles(const char* const* filenames, size_t num, boolean allowDuplicate)
{
    boolean succeeded = false;
    { size_t i;
    for(i = 0; i < num; ++i)
    {
        if(F_AddFile(filenames[i], allowDuplicate))
        {
            VERBOSE2( Con_Message("Done loading %s\n", F_PrettyPath(filenames[i])) );
            succeeded = true; // At least one has been loaded.
        }
        else
            Con_Message("Warning: Errors occured while loading %s\n", filenames[i]);
    }}

    // A changed file list may alter the main lump directory.
    if(succeeded)
    {
        DD_UpdateEngineState();
    }
    return succeeded;
}

boolean F_RemoveFile(const char* path)
{
    boolean unloadedResources = removeFile(path);
    if(unloadedResources)
        DD_UpdateEngineState();
    return unloadedResources;
}

boolean F_RemoveFiles(const char* const* filenames, size_t num)
{
    boolean succeeded = false;
    { size_t i;
    for(i = 0; i < num; ++i)
    {
        if(removeFile(filenames[i]))
        {
            VERBOSE2( Con_Message("Done unloading %s\n", F_PrettyPath(filenames[i])) );
            succeeded = true; // At least one has been unloaded.
        }
        else
            Con_Message("Warning: Errors occured while unloading %s\n", filenames[i]);
    }}

    // A changed file list may alter the main lump directory.
    if(succeeded)
    {
        DD_UpdateEngineState();
    }
    return succeeded;
}

void F_AddResourcePathMapping(const char* source, const char* destination)
{
    ddstring_t* src, *dst;
    vdmapping_t* vd;

    // Convert to absolute path names.
    src = Str_Set(Str_New(), source);
    Str_Strip(src);
    F_FixSlashes(src, src);
    if(DIR_SEP_CHAR != Str_RAt(src, 0))
        Str_AppendChar(src, DIR_SEP_CHAR);
    F_ExpandBasePath(src, src);
    F_PrependWorkPath(src, src);

    dst = Str_Set(Str_New(), destination);
    Str_Strip(dst);
    F_FixSlashes(dst, dst);
    if(DIR_SEP_CHAR != Str_RAt(dst, 0))
        Str_AppendChar(dst, DIR_SEP_CHAR);
    F_ExpandBasePath(dst, dst);
    F_PrependWorkPath(dst, dst);

    // Allocate more memory if necessary.
    if(++vdMappingsCount > vdMappingsMax)
    {
        vdMappingsMax *= 2;
        if(vdMappingsMax < vdMappingsCount)
            vdMappingsMax = 2*vdMappingsCount;

        vdMappings = (vdmapping_t*) realloc(vdMappings, sizeof(*vdMappings) * vdMappingsMax);
        if(NULL == vdMappings)
            Con_Error("F_AddResourcePathMapping: Failed on allocation of %lu bytes for mapping list.",
                (unsigned long) (sizeof(*vdMappings) * vdMappingsMax));
    }

    // Fill in the info into the array.
    vd = &vdMappings[vdMappingsCount - 1];
    vd->source = src;
    vd->target = dst;

    VERBOSE( Con_Message("Resources in \"%s\" now mapped to \"%s\"\n", Str_Text(vd->source), Str_Text(vd->target)) );
}

/// Skip all whitespace except newlines.
static __inline const char* skipSpace(const char* ptr)
{
    assert(ptr);
    while(*ptr && *ptr != '\n' && isspace(*ptr))
        ptr++;
    return ptr;
}

static boolean parseLumpPathMapping(lumppathmapping_t* lpm, const char* buffer)
{
    const char* ptr = buffer, *end;
    size_t len;

    // Find the start of the lump name.
    ptr = skipSpace(ptr);
    if(!*ptr || *ptr == '\n')
    {   // Just whitespace??
        return false;
    }

    // Find the end of the lump name.
    end = M_FindWhite((char*)ptr);
    if(!*end || *end == '\n')
    {
        return false;
    }

    len = end - ptr;
    if(len > 8)
    {   // Invalid lump name.
        return false;
    }

    clearLumpPathMapping(lpm);
    strncpy(lpm->lumpName, ptr, len);

    // Find the start of the file path.
    ptr = skipSpace(end);
    if(!*ptr || *ptr == '\n')
    {   // Missing file path.
        return false;
    }

    // We're at the file path.
    Str_Set(&lpm->path, ptr);
    // Get rid of any extra whitespace on the end.
    Str_StripRight(&lpm->path);
    return true;
}

/**
 * LUMPNAM0 \Path\In\The\Base.ext
 * LUMPNAM1 Path\In\The\RuntimeDir.ext
 *  :
 */
static boolean parseLumpDirectoryMap(const char* buffer)
{
    assert(buffer);
    {
    boolean successful = false;
    lumppathmapping_t lpm;
    ddstring_t line;
    const char* ch;

    initLumpPathMapping(&lpm);

    Str_Init(&line);
    ch = buffer;
    do
    {
        ch = Str_GetLine(&line, ch);
        if(!parseLumpPathMapping(&lpm, Str_Text(&line)))
        {   // Failure parsing the mapping.
            // Ignore errors in individual mappings and continue parsing.
            //goto parseEnded;
        }
        else
        {
            strupr(lpm.lumpName);
            F_FixSlashes(&lpm.path, &lpm.path);
            addLumpDirectoryMapping(lpm.lumpName, &lpm.path);
        }
    } while(*ch);

    // Success.
    successful = true;

//parseEnded:
    clearLumpPathMapping(&lpm);
    Str_Free(&line);
    return successful;
    }
}

void F_InitializeResourcePathMap(void)
{
    int argC = Argc();

    resetVDirectoryMappings();

    // Create virtual directory mappings by processing all -vdmap options.
    { int i;
    for(i = 0; i < argC; ++i)
    {
        if(strnicmp("-vdmap", Argv(i), 6))
            continue; // This is not the option we're looking for.

        if(i < argC - 1 && !ArgIsOption(i + 1) && !ArgIsOption(i + 2))
        {
            F_AddResourcePathMapping(Argv(i + 1), Argv(i + 2));
            i += 2;
        }
    }}
}

void F_InitDirec(void)
{
    static boolean inited = false;
    size_t bufSize = 0;
    uint8_t* buf = NULL;

    if(inited)
    {   // Free old paths, if any.
        clearLumpDirectory();
        memset(lumpDirectory, 0, sizeof(lumpDirectory));
    }

    // Add the contents of all DD_DIREC lumps.
    { lumpnum_t i;
    for(i = 0; i < F_LumpCount(); ++i)
    {
        const lumpinfo_t* info = F_FindInfoForLumpNum(i);
        abstractfile_t* fsObject;
        size_t lumpLength;
        int lumpIdx;

        if(strnicmp(info->name, "DD_DIREC", 8))
            continue;

        // Make a copy of it so we can ensure it ends in a null.
        lumpLength = info->size;
        if(bufSize < lumpLength + 1)
        {
            bufSize = lumpLength + 1;
            buf = (uint8_t*) realloc(buf, bufSize);
            if(NULL == buf)
                Con_Error("F_InitDirec: Failed on (re)allocation of %lu bytes for temporary read buffer.", (unsigned long) bufSize);
        }

        fsObject = F_FindFileForLumpNum2(i, &lumpIdx);
        F_ReadLumpSection(fsObject, lumpIdx, buf, 0, lumpLength);
        buf[lumpLength] = 0;
        parseLumpDirectoryMap((const char*)buf);
    }}

    if(NULL != buf)
        free(buf);

    inited = true;
}

void F_ShutdownDirec(void)
{
    resetVDirectoryMappings();
    clearLumpDirectory();
}

int F_MatchFileName(const char* string, const char* pattern)
{
    const char* in = string, *st = pattern;

    while(*in)
    {
        if(*st == '*')
        {
            st++;
            continue;
        }

        if(*st != '?' && (tolower((unsigned char) *st) != tolower((unsigned char) *in)))
        {
            // A mismatch. Hmm. Go back to a previous '*'.
            while(st >= pattern && *st != '*')
                st--;
            if(st < pattern)
                return false; // No match!
            // The asterisk lets us continue.
        }

        // This character of the pattern is OK.
        st++;
        in++;
    }

    // Match is good if the end of the pattern was reached.
    while(*st == '*')
        st++; // Skip remaining asterisks.

    return *st == 0;
}

static int C_DECL compareFileNodeByFilePath(const void* a_, const void* b_)
{
    abstractfile_t* a = (*((const filelist_node_t**)a_))->fsObject;
    abstractfile_t* b = (*((const filelist_node_t**)b_))->fsObject;
    return Str_CompareIgnoreCase(AbstractFile_AbsolutePath(a), Str_Text(AbstractFile_AbsolutePath(b)));
}

static filelist_node_t** collectOpenFileNodes(size_t* count)
{
    filelist_node_t** list, *node;
    size_t numFiles;

    errorIfNotInited("collectOpenFiles");

    // Count files.
    numFiles = 0;
    for(node = fileList; NULL != node; node = node->next) { ++numFiles; };

    // Allocate return array.
    list = (filelist_node_t**)malloc(sizeof(*list) * (numFiles+1));
    if(!list)
        Con_Error("collectOpenFiles: Failed on allocation of %lu bytes for file list.",
            (unsigned long) (sizeof(*list) * (numFiles+1)));

    // Collect file pointers.
    numFiles = 0;
    for(node = fileList; NULL != node; node = node->next)
    {
        list[numFiles++] = node;
    }
    list[numFiles] = NULL; // Terminate.
    if(count)
        *count = numFiles;
    return list;
}

/**
 * Prints the resource path to the console.
 * This is a f_allresourcepaths_callback_t.
 */
int printResourcePath(const ddstring_t* fileNameStr, pathdirectory_nodetype_t type,
    void* paramaters)
{
    assert(fileNameStr && VALID_PATHDIRECTORY_NODETYPE(type));
    {
    const char* fileName = Str_Text(fileNameStr);
    boolean makePretty = F_IsRelativeToBasePath(fileName);
    Con_Printf("  %s\n", makePretty? F_PrettyPath(fileName) : fileName);
    return 0; // Continue the listing.
    }
}

static void printDirectory(const ddstring_t* path)
{
    ddstring_t dir;

    Str_Init(&dir); Str_Set(&dir, Str_Text(path));
    Str_Strip(&dir);
    F_FixSlashes(&dir, &dir);
    // Make sure it ends in a directory separator character.
    if(Str_RAt(&dir, 0) != DIR_SEP_CHAR)
        Str_AppendChar(&dir, DIR_SEP_CHAR);
    if(!F_ExpandBasePath(&dir, &dir))
        F_PrependBasePath(&dir, &dir);

    Con_Printf("Directory: %s\n", F_PrettyPath(Str_Text(&dir)));

    // Make the pattern.
    Str_AppendChar(&dir, '*');
    F_AllResourcePaths(Str_Text(&dir), printResourcePath);

    Str_Free(&dir);
}

/**
 * Print contents of directories as Doomsday sees them.
 */
D_CMD(Dir)
{
    ddstring_t path;
    Str_Init(&path);
    if(argc > 1)
    {
        int i;
        for(i = 1; i < argc; ++i)
        {
            Str_Set(&path, argv[i]);
            printDirectory(&path);
        }
    }
    else
    {
        Str_Set(&path, "/");
        printDirectory(&path);
    }
    Str_Free(&path);
    return true;
}

D_CMD(DumpLump)
{
    if(inited)
    {
        lumpnum_t absoluteLumpNum = F_CheckLumpNumForName(argv[1], true);
        if(absoluteLumpNum >= 0)
        {
            int lumpIdx;
            abstractfile_t* fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
            return F_DumpLump(fsObject, lumpIdx, NULL);
        }
        Con_Printf("No such lump.\n");
        return false;
    }
    Con_Printf("WAD module is not presently initialized.\n");
    return false;
}

D_CMD(ListFiles)
{
    size_t totalFiles = 0, totalPackages = 0;
    if(inited)
    {
        size_t numNodes;
        filelist_node_t** ptr, **list = collectOpenFileNodes(&numNodes);
        if(!list) return true;

        // Sort nodes so we get a nice alpha-numerical list.
        qsort(list, numNodes, sizeof(filelist_node_t*), compareFileNodeByFilePath);

        for(ptr = list; NULL != *ptr; ptr++)
        {
            filelist_node_t* node = *ptr;
            size_t fileCount;
            uint crc;

            switch(AbstractFile_Type(node->fsObject))
            {
            case FT_UNKNOWNFILE:
                fileCount = 1;
                crc = 0;
                break;
            case FT_ZIPFILE:
                fileCount = ZipFile_LumpCount((zipfile_t*)node->fsObject);
                crc = 0;
                break;
            case FT_WADFILE: {
                wadfile_t* wad = (wadfile_t*)node->fsObject;
                crc = (WadFile_IsIWAD(wad)? WadFile_CalculateCRC(wad) : 0);
                fileCount = WadFile_LumpCount(wad);
                break;
              }
            case FT_LUMPFILE:
                fileCount = LumpFile_LumpCount((lumpfile_t*)node->fsObject);
                crc = 0;
                break;
            default:
                Con_Error("CCmdListLumps: Invalid file type %i.", AbstractFile_Type(node->fsObject));
                exit(1); // Unreachable.
            }

            Con_Printf("\"%s\" (%lu %s%s)", F_PrettyPath(Str_Text(AbstractFile_AbsolutePath(node->fsObject))),
                (unsigned long) fileCount, fileCount != 1 ? "files" : "file",
                (node->loadedForStartup? ", startup" : ""));
            if(0 != crc)
                Con_Printf(" [%08x]", crc);
            Con_Printf("\n");

            totalFiles += fileCount;
            ++totalPackages;
        }

        free(list);
    }
    Con_Printf("Total: %lu files in %lu packages.\n", (unsigned long) totalFiles, (unsigned long)totalPackages);
    return true;
}
