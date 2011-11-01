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

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_misc.h" // For M_LimitedStrCat

#include "m_md5.h"
#include "lumpinfo.h"
#include "lumpdirectory.h"
#include "lumpfile.h"
#include "wadfile.h"
#include "zipfile.h"
#include "filelist.h"

D_CMD(Dir);
D_CMD(DumpLump);
D_CMD(ListFiles);

/**
 * Lump Directory Mapping.  Maps lump to resource path.
 */
typedef struct {
    lumpname_t lumpName;
    ddstring_t path; // Absolute path.
} ldmapping_t;

/**
 * Virtual Directory Mapping.  Maps a resource path to another resource.
 */
typedef struct {
    ddstring_t source; // Absolute path.
    ddstring_t destination; // Absolute path.
} vdmapping_t;

/**
 * FileIdentifier
 */
#define FILEIDENTIFIERID_T_MAXLEN 16
#define FILEIDENTIFIERID_T_LASTINDEX 15
typedef byte fileidentifierid_t[FILEIDENTIFIERID_T_MAXLEN];

typedef struct fileidentifier_s {
    fileidentifierid_t hash;
} fileidentifier_t;

static boolean inited = false;
static boolean loadingForStartup;

static FileList* openFiles;
static FileList* loadedFiles;

static uint fileIdentifiersCount = 0;
static uint fileIdentifiersMax = 0;
static fileidentifier_t* fileIdentifiers = NULL;

static lumpdirectory_t* zipLumpDirectory;

static lumpdirectory_t* primaryWadLumpDirectory;
static lumpdirectory_t* auxiliaryWadLumpDirectory;
// @c true = one or more files have been opened using the auxiliary directory.
static boolean auxiliaryWadLumpDirectoryInUse;

// Currently selected lump directory.
static lumpdirectory_t* ActiveWadLumpDirectory;

// Lump directory mappings.
static uint ldMappingsCount = 0;
static uint ldMappingsMax = 0;
static ldmapping_t* ldMappings = NULL;

// Virtual(-File) directory mappings.
static uint vdMappingsCount = 0;
static uint vdMappingsMax = 0;
static vdmapping_t* vdMappings = NULL;

static void clearLDMappings(void);
static void clearVDMappings(void);
static boolean applyVDMapping(ddstring_t* path, vdmapping_t* vdm);

static FILE* findRealFile(const char* path, const char* mymode, ddstring_t** foundPath);

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
    Con_Error("%s: VFS module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static fileidentifier_t* findFileIdentifierForId(fileidentifierid_t id)
{
    fileidentifier_t* found = NULL;
    if(fileIdentifiersCount)
    {
        uint i = 0;
        do
        {
            if(!memcmp(fileIdentifiers[i].hash, id, FILEIDENTIFIERID_T_LASTINDEX))
                found = &fileIdentifiers[i];
        } while(!found && ++i < fileIdentifiersCount);
    }
    return found;
}

static int findFileNodeIndexForPath(FileList* list, const char* path_)
{
    int found = -1;
    if(NULL != path_ && path_[0] && !FileList_Empty(list))
    {
        abstractfile_t* file;
        ddstring_t path;
        int i;

        // Transform the path into one we can process.
        Str_Init(&path); Str_Set(&path, path_);
        F_FixSlashes(&path, &path);

        // Perform the search.
        i = 0;
        do
        {
            file = FileList_GetFile(list, i);
            if(!Str_CompareIgnoreCase(AbstractFile_Path(file), Str_Text(&path)))
            {
                found = i;
            }
        } while(found == -1 && ++i < FileList_Size(list));
        Str_Free(&path);
    }
    return found;
}

static abstractfile_t* newUnknownFile(DFile* file, const lumpinfo_t* info)
{
    abstractfile_t* af = (abstractfile_t*)malloc(sizeof *af);
    if(!af) Con_Error("newUnknownFile: Failed on allocation of %lu bytes for new abstractfile_t.",
        (unsigned long) sizeof *af);
    return AbstractFile_Init(af, FT_UNKNOWNFILE, file, info);
}

static abstractfile_t* newZipFile(DFile* file, const lumpinfo_t* info)
{
    return (abstractfile_t*)ZipFile_New(file, info);
}

static abstractfile_t* newWadFile(DFile* file, const lumpinfo_t* info)
{
    return (abstractfile_t*)WadFile_New(file, info);
}

static abstractfile_t* newLumpFile(DFile* file, const lumpinfo_t* info)
{
    return (abstractfile_t*)LumpFile_New(file, info);
}

static int pruneLumpsFromDirectorysByFile(abstractfile_t* af)
{
    int pruned = LumpDirectory_PruneByFile(zipLumpDirectory, af);
    pruned += LumpDirectory_PruneByFile(primaryWadLumpDirectory, af);
    if(auxiliaryWadLumpDirectoryInUse)
        pruned += LumpDirectory_PruneByFile(auxiliaryWadLumpDirectory, af);
    return pruned;
}

static boolean removeLoadedFile(int loadedFilesNodeIndex)
{
    DFile* file = FileList_Get(loadedFiles, loadedFilesNodeIndex);
    abstractfile_t* af = DFile_File(file);

    switch(AbstractFile_Type(af))
    {
    case FT_UNKNOWNFILE: break;

    case FT_ZIPFILE:  ZipFile_ClearLumpCache( ( zipfile_t*)af); break;
    case FT_WADFILE:  WadFile_ClearLumpCache( ( wadfile_t*)af); break;
    case FT_LUMPFILE: LumpFile_ClearLumpCache((lumpfile_t*)af); break;

    default:
        Con_Error("WadCollection::removeLoadedFile: Invalid file type %i.", AbstractFile_Type(af));
        exit(1); // Unreachable.
    }

    F_ReleaseFileId(Str_Text(AbstractFile_Path(af)));
    pruneLumpsFromDirectorysByFile(af);

    F_Delete(FileList_RemoveAt(loadedFiles, loadedFilesNodeIndex));
    return true;
}

static int directoryContainsLumpsFromFile(const lumpinfo_t* info, void* paramaters)
{
    return 1; // Stop iteration we need go no further.
}

static void clearLoadedFiles(lumpdirectory_t* directory)
{
    abstractfile_t* file;
    int i;
    for(i = FileList_Size(loadedFiles) - 1; i >= 0; i--)
    {
        file = FileList_GetFile(loadedFiles, i);
        if(!directory || LumpDirectory_Iterate(directory, file, directoryContainsLumpsFromFile))
        {
            removeLoadedFile(i);
        }
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

static void usePrimaryWadLumpDirectory(void)
{
    ActiveWadLumpDirectory = primaryWadLumpDirectory;
}

static boolean useAuxiliaryWadLumpDirectory(void)
{
    if(!auxiliaryWadLumpDirectoryInUse)
        return false;
    ActiveWadLumpDirectory = auxiliaryWadLumpDirectory;
    return true;
}

static void clearLumpDirectorys(void)
{
    LumpDirectory_Delete(primaryWadLumpDirectory), primaryWadLumpDirectory = NULL;
    LumpDirectory_Delete(auxiliaryWadLumpDirectory), auxiliaryWadLumpDirectory = NULL;
    ActiveWadLumpDirectory = NULL;

    LumpDirectory_Delete(zipLumpDirectory), zipLumpDirectory = NULL;
}

static void clearOpenFiles(void)
{
    int i;
    for(i = FileList_Size(openFiles) - 1; i >= 0; i--)
    {
        F_Delete(FileList_Get(openFiles, i));
    }
    FileList_Clear(openFiles);
}

/**
 * Selects which lump directory to use, given a logical lump index.
 * This should be called in all functions that access directories by lump index.
 */
static lumpnum_t chooseWadLumpDirectory(lumpnum_t lumpNum)
{
    errorIfNotInited("chooseWadLumpDirectory");
    if(lumpNum >= AUXILIARY_BASE)
    {
        useAuxiliaryWadLumpDirectory();
        lumpNum -= AUXILIARY_BASE;
    }
    else
    {
        usePrimaryWadLumpDirectory();
    }
    return lumpNum;
}

static boolean unloadFile2(const char* path)
{
    int idx;
    errorIfNotInited("unloadFile2");
    idx = findFileNodeIndexForPath(loadedFiles, path);
    if(idx >= 0)
        return removeLoadedFile(idx);
    return false; // No such file loaded.
}

static boolean unloadFile(const char* path)
{
    VERBOSE( Con_Message("Unloading \"%s\"...\n", F_PrettyPath(path)) )
    return unloadFile2(path);
}

static void clearFileIds(void)
{
    if(fileIdentifiers)
    {
        free(fileIdentifiers), fileIdentifiers = NULL;
    }
    if(fileIdentifiersCount)
    {
        fileIdentifiersCount = fileIdentifiersMax = 0;
    }
}

void F_PrintFileId(byte identifier[16])
{
    assert(identifier);
    { uint i;
    for(i = 0; i < 16; ++i)
        Con_Printf("%02x", identifier[i]);
    }
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
    fileIdentifiersCount++;
    if(fileIdentifiersCount > fileIdentifiersMax)
    {
        if(!fileIdentifiersMax)
            fileIdentifiersMax = 16;
        else
            fileIdentifiersMax *= 2;

        fileIdentifiers = (fileidentifier_t*)realloc(fileIdentifiers, fileIdentifiersMax * sizeof *fileIdentifiers);
        if(!fileIdentifiers)
            Con_Error("F_CheckFileId: Failed on (re)allocation of %lu bytes while "
                "enlarging fileIdentifiers.", (unsigned long) (fileIdentifiersMax * sizeof *fileIdentifiers));
        memset(fileIdentifiers + fileIdentifiersCount, 0, (fileIdentifiersMax - fileIdentifiersCount) * sizeof *fileIdentifiers);
    }

#if _DEBUG
    VERBOSE(
        Con_Printf("Added file identifier ");
        F_PrintFileId(id);
        Con_Printf(" - \"%s\"\n", F_PrettyPath(path)) )
#endif
    memcpy(fileIdentifiers[fileIdentifiersCount - 1].hash, id, sizeof(id));
    return true;
    }
}

boolean F_ReleaseFileId(const char* path)
{
    if(path && path[0])
    {
        fileidentifierid_t id;
        fileidentifier_t* fileIdentifier;

        F_GenerateFileId(path, id);

        fileIdentifier = findFileIdentifierForId(id);
        if(fileIdentifier)
        {
            size_t index = fileIdentifier - fileIdentifiers;
            if(index < fileIdentifiersCount-1)
                memmove(fileIdentifiers + index, fileIdentifiers + index + 1, fileIdentifiersCount - index - 1);
            memset(fileIdentifiers + fileIdentifiersCount, 0, sizeof *fileIdentifiers);
            --fileIdentifiersCount;

#if _DEBUG
            VERBOSE(
                Con_Printf("Released file identifier ");
                F_PrintFileId(id);
                Con_Printf(" - \"%s\"\n", F_PrettyPath(path)) )
#endif
            return true;
        }
    }
    return false;
}

void F_ResetFileIds(void)
{
    fileIdentifiersCount = 0;
}

void F_InitLumpInfo(lumpinfo_t* info)
{
    assert(info);
    memset(info->name, 0, sizeof info->name);
    Str_Init(&info->path);
    info->lumpIdx = 0;
    info->size = 0;
    info->compressedSize = 0;
    info->lastModified = 0;
    info->container = NULL;
}

void F_CopyLumpInfo(lumpinfo_t* dst, const lumpinfo_t* src)
{
    assert(dst && src);
    memcpy(dst->name, src->name, sizeof dst->name);
    Str_Init(&dst->path);
    if(!Str_IsEmpty(&src->path))
        Str_Set(&dst->path, Str_Text(&src->path));
    dst->lumpIdx = src->lumpIdx;
    dst->size = src->size;
    dst->compressedSize = src->compressedSize;
    dst->lastModified = src->lastModified;
    dst->container = src->container;
}

void F_DestroyLumpInfo(lumpinfo_t* info)
{
    assert(info);
    Str_Free(&info->path);
}

void F_Init(void)
{
    if(inited) return; // Already been here.

    // This'll force the loader NOT to flag new files as "runtime".
    loadingForStartup = true;

    DFileBuilder_Init();

    openFiles     = FileList_New();
    loadedFiles   = FileList_New();

    zipLumpDirectory          = LumpDirectory_New();
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

    clearVDMappings();
    clearLDMappings();

    clearLoadedFiles(0);
    FileList_Delete(loadedFiles), loadedFiles = NULL;
    clearOpenFiles();
    FileList_Delete(openFiles), openFiles = NULL;

    clearFileIds(); // Should be null-op if bookkeeping is correct.
    clearLumpDirectorys();

    DFileBuilder_Shutdown();

    inited = false;
}

void F_EndStartup(void)
{
    errorIfNotInited("F_EndStartup");
    loadingForStartup = false;
    usePrimaryWadLumpDirectory();
}

static int unloadListFiles(FileList* list, boolean nonStartup)
{
    assert(list);
    {
    abstractfile_t* file;
    int i, unloaded = 0;
    for(i = FileList_Size(list) - 1; i >= 0; i--)
    {
        file = FileList_GetFile(list, i);
        if(!nonStartup || !AbstractFile_HasStartup(file))
        {
            if(unloadFile2(Str_Text(AbstractFile_Path(file))))
            {
                ++unloaded;
            }
        }
    }
    return unloaded;
    }
}

int F_Reset(void)
{
    int unloaded = 0;
    if(inited)
    {
#if _DEBUG
        // List all open files with their identifiers.
        VERBOSE(
            Con_Printf("Open files at reset:\n");
            FileList_Print(openFiles);
            Con_Printf("End\n") )
#endif

        // Perform non-startup file unloading...
        unloaded = unloadListFiles(loadedFiles, true/*non-startup*/);

#if _DEBUG
        // Sanity check: look for dangling identifiers.
        { uint i;
        fileidentifierid_t nullId;
        memset(nullId, 0, sizeof nullId);
        for(i = 0; i < fileIdentifiersCount; ++i)
        {
            fileidentifier_t* id = fileIdentifiers + i;
            if(!memcmp(id->hash, &nullId, FILEIDENTIFIERID_T_LASTINDEX)) continue;

            Con_Printf("Warning: Dangling file identifier: ");
            F_PrintFileId(id->hash);
            Con_Printf("\n");
        }}
#endif

        // Reset file IDs so previously seen files can be processed again.
        /// \fixme this releases the ID of startup files too but given the
        /// only startup file is doomsday.pk3 which we never attempt to load
        /// again post engine startup, this isn't an immediate problem.
        F_ResetFileIds();

        // Update the dir/WAD translations.
        F_InitLumpDirectoryMappings();
        F_InitVirtualDirectoryMappings();
    }
    return unloaded;
}

boolean F_IsValidLumpNum(lumpnum_t absoluteLumpNum)
{
    errorIfNotInited("F_IsValidLumpNum");
    { lumpnum_t lumpNum = chooseWadLumpDirectory(absoluteLumpNum);
    return LumpDirectory_IsValidIndex(ActiveWadLumpDirectory, lumpNum); }
}

lumpnum_t F_CheckLumpNumForName2(const char* name, boolean silent)
{
    lumpnum_t lumpNum = -1;
    errorIfNotInited("F_CheckLumpNumForName");
    if(name && name[0])
    {
        // We have to check both the primary and auxiliary caches because
        // we've only got a name and don't know where it is located. Start with
        // the auxiliary lumps because they take precedence.
        if(useAuxiliaryWadLumpDirectory())
        {
            lumpNum = LumpDirectory_IndexForName(ActiveWadLumpDirectory, name);
        }

        // Found it yet?
        if(lumpNum < 0)
        {
            usePrimaryWadLumpDirectory();
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

lumpnum_t F_CheckLumpNumForName(const char* name)
{
    return F_CheckLumpNumForName2(name, false); 
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

uint F_LumpLastModified(lumpnum_t absoluteLumpNum)
{
    const lumpinfo_t* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(info) return info->lastModified;
    return 0;
}

abstractfile_t* F_FindFileForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx)
{
    errorIfNotInited("F_FindFileForLumpNum2");
    { lumpnum_t translated = chooseWadLumpDirectory(absoluteLumpNum);
    if(lumpIdx) *lumpIdx = LumpDirectory_LumpIndex(ActiveWadLumpDirectory, translated);
    return LumpDirectory_SourceFile(ActiveWadLumpDirectory, translated);
    }
}

abstractfile_t* F_FindFileForLumpNum(lumpnum_t absoluteLumpNum)
{
    return F_FindFileForLumpNum2(absoluteLumpNum, NULL);
}

const char* F_LumpSourceFile(lumpnum_t absoluteLumpNum)
{
    abstractfile_t* fsObject = F_FindFileForLumpNum(absoluteLumpNum);
    if(fsObject) return Str_Text(AbstractFile_Path(fsObject));
    return "";
}

boolean F_LumpIsCustom(lumpnum_t absoluteLumpNum)
{
    abstractfile_t* fsObject = F_FindFileForLumpNum(absoluteLumpNum);
    if(fsObject) return !AbstractFile_HasIWAD(fsObject);
    return false;
}

int F_LumpCount(void)
{
    if(inited)
        return LumpDirectory_NumLumps(ActiveWadLumpDirectory);
    return 0;
}

lumpnum_t F_OpenAuxiliary3(const char* path, size_t baseOffset, boolean silent)
{
    ddstring_t* foundPath = NULL;
    ddstring_t searchPath;
    DFile* dfile;
    FILE* file;

    errorIfNotInited("F_OpenAuxiliary");

    if(!path || !path[0])
        return -1;

    // Make it a full path.
    Str_Init(&searchPath); Str_Set(&searchPath, path);
    F_FixSlashes(&searchPath, &searchPath);
    F_ExpandBasePath(&searchPath, &searchPath);
    // We must have an absolute path, so prepend the current working directory if necessary.
    F_PrependWorkPath(&searchPath, &searchPath);

    /// \todo Allow opening WAD/ZIP files from lumps in other containers.
    file = findRealFile(Str_Text(&searchPath), "rb", &foundPath);
    Str_Free(&searchPath);
    if(!file)
    {
        if(foundPath)
            Str_Delete(foundPath);
        if(!silent)
        {
            Con_Message("Warning:F_OpenAuxiliary: Resource \"%s\" not found, aborting.\n", path);
        }
        return -1;
    }

    dfile = DFileBuilder_NewFromFile(file, baseOffset);

    if(WadFile_Recognise(dfile))
    {
        lumpinfo_t info;

        if(auxiliaryWadLumpDirectoryInUse)
        {
            F_CloseAuxiliary();
        }
        ActiveWadLumpDirectory = auxiliaryWadLumpDirectory;
        auxiliaryWadLumpDirectoryInUse = true;

        // Prepare the temporary info descriptor.
        F_InitLumpInfo(&info);
        Str_Set(&info.path, Str_Text(foundPath));
        Str_Strip(&info.path);
        F_FixSlashes(&info.path, &info.path);
        info.lastModified = F_LastModified(Str_Text(foundPath));

        Str_Delete(foundPath);

        dfile = DFileBuilder_NewFromAbstractFile(newWadFile(dfile, &info));
        FileList_AddBack(openFiles,   dfile);
        FileList_AddBack(loadedFiles, DFileBuilder_NewCopy(dfile));
        WadFile_PublishLumpsToDirectory((wadfile_t*)DFile_File(dfile), ActiveWadLumpDirectory);

        // We're done with the descriptor.
        F_DestroyLumpInfo(&info);
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

lumpnum_t F_OpenAuxiliary2(const char* path, size_t baseOffset)
{
    return F_OpenAuxiliary3(path, baseOffset, false);
}

lumpnum_t F_OpenAuxiliary(const char* path)
{
    return F_OpenAuxiliary2(path, 0);
}

void F_CloseAuxiliary(void)
{
    errorIfNotInited("F_CloseAuxiliary");
    if(useAuxiliaryWadLumpDirectory())
    {
        clearLoadedFiles(auxiliaryWadLumpDirectory);
        auxiliaryWadLumpDirectoryInUse = false;
    }
    usePrimaryWadLumpDirectory();
}

void F_ReleaseFile(abstractfile_t* fsObject)
{
    DFile* file;
    int i;
    if(!fsObject) return;
    for(i = FileList_Size(openFiles) - 1; i >= 0; i--)
    {
        file = FileList_Get(openFiles, i);
        if(DFile_File(file) == fsObject)
        {
            FileList_RemoveAt(openFiles, i);
        }
    }
}

void F_Close(DFile* file)
{
    DFile_Close(file);
}

void F_Delete(DFile* file)
{
    abstractfile_t* af = DFile_File(file);
    DFile_Close(file);
    switch(AbstractFile_Type(af))
    {
    case FT_UNKNOWNFILE:
        F_ReleaseFile(af);
        AbstractFile_Destroy(af);
        free(af);
        break;

    case FT_ZIPFILE:  ZipFile_Delete( ( zipfile_t*)af); break;
    case FT_WADFILE:  WadFile_Delete( ( wadfile_t*)af); break;
    case FT_LUMPFILE: LumpFile_Delete((lumpfile_t*)af); break;
    default:
        Con_Error("F_Delete: Invalid file type %i.", AbstractFile_Type(af));
        exit(1); // Unreachable.
    }
    DFile_Delete(file, true);
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
        memset(buf, 0, sizeof buf);
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
    errorIfNotInited("F_CRCNumber");
    /**
     * We define the CRC as that of the lump directory of the first loaded IWAD.
     */
    if(!FileList_Empty(loadedFiles))
    {
        abstractfile_t* file, *found = NULL;
        int i = 0;
        do
        {
            file = FileList_GetFile(loadedFiles, i);
            if(FT_WADFILE == AbstractFile_Type(file) && AbstractFile_HasIWAD(file))
            {
                found = file;
            }
        } while(!found && ++i < FileList_Size(loadedFiles));
        if(found)
        {
            return WadFile_CalculateCRC((wadfile_t*)found);
        }
    }
    return 0;
}

typedef struct {
    filetype_t type; // Only 
    boolean includeIWAD;
    boolean includeOther;
} compositepathpredicateparamaters_t;

boolean C_DECL compositePathPredicate(DFile* hndl, void* paramaters)
{
    assert(hndl && paramaters);
    {
    compositepathpredicateparamaters_t* p = (compositepathpredicateparamaters_t*)paramaters;
    abstractfile_t* file = DFile_File(hndl);
    if((!VALID_FILETYPE(p->type) || p->type == AbstractFile_Type(file)) &&
       ((p->includeIWAD  &&  AbstractFile_HasIWAD(file)) ||
        (p->includeOther && !AbstractFile_HasIWAD(file))))
    {
        const ddstring_t* path = AbstractFile_Path(file);
        if(stricmp(Str_Text(path) + Str_Length(path) - 3, "lmp"))
            return true; // Include this.
    }
    return false; // Not this.
    }
}

void F_GetPWADFileNames(char* outBuf, size_t outBufSize, const char* delimiter)
{
    if(NULL == outBuf || 0 == outBufSize) return;
    memset(outBuf, 0, outBufSize);
    if(inited)
    {
        compositepathpredicateparamaters_t p = { FT_WADFILE, false, true };
        ddstring_t* str = FileList_ToString4(loadedFiles, PTSF_TRANSFORM_EXCLUDE_DIR,
            delimiter, compositePathPredicate, (void*)&p);
        strncpy(outBuf, Str_Text(str), outBufSize);
        Str_Delete(str);
    }
}

void F_PrintLumpDirectory(void)
{
    if(!inited) return;
    // Always the primary directory.
    LumpDirectory_Print(primaryWadLumpDirectory);
}

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
        if(i >= 0 && !applyVDMapping(&wildPath, &vdMappings[i]))
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
                        found = (foundentry_t*)realloc(found, max * sizeof *found);
                        if(!found)
                            Con_Error("collectLocalPaths: Failed on (re)allocation of %lu bytes while "
                                "resizing found path collection.", (unsigned long) (max * sizeof *found));
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

typedef struct {
    /// Callback to make for each processed file.
    int (*callback) (const ddstring_t* path, pathdirectory_nodetype_t type, void* paramaters);

    /// Data passed to the callback.
    void* paramaters;

    /// Current search pattern.
    const ddstring_t* pattern;
} findlumpworker_paramaters_t;

static int findLumpWorker(const lumpinfo_t* lumpInfo, void* paramaters)
{
    assert(NULL != lumpInfo && NULL != paramaters);
    {
    findlumpworker_paramaters_t* p = (findlumpworker_paramaters_t*)paramaters;
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
    { findlumpworker_paramaters_t p;
    p.callback = callback;
    p.paramaters = paramaters;
    p.pattern = &searchPattern;

    result = LumpDirectory_Iterate2(zipLumpDirectory, NULL, findLumpWorker, (void*)&p);
    if(0 != result)
    {   // Find didn't finish.
        goto searchEnded;
    }}

    // Check the dir/WAD direcs.
    if(ldMappingsCount)
    {
        uint i;
        for(i = 0; i < ldMappingsCount; ++i)
        {
            ldmapping_t* rec = &ldMappings[i];
            if(!F_MatchFileName(Str_Text(&rec->path), Str_Text(&searchPattern)))
                continue;
            result = callback(&rec->path, PT_LEAF, paramaters);
            if(0 != result)
                goto searchEnded;
        }
    }

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
        if(!applyVDMapping(mapped, &vdMappings[i]))
            continue;
        // The mapping was successful.
        file = fopen(Str_Text(mapped), mode);
        if(file)
        {
            VERBOSE( Con_Message("findRealFile: \"%s\" opened as %s.\n", F_PrettyPath(Str_Text(mapped)), path) )
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

abstractfile_t* F_FindLumpFile(const char* path, int* lumpIdx)
{
    ddstring_t absSearchPath;
    lumpnum_t lumpNum;

    if(!inited || !path || !path[0])
        return NULL;

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
    if(ldMappingsCount)
    {
        uint i = 0;

        // We must have an absolute path, so prepend the current working directory if necessary.
        Str_Set(&absSearchPath, path);
        F_PrependWorkPath(&absSearchPath, &absSearchPath);
        
        for(i = 0; i < ldMappingsCount; ++i)
        {
            ldmapping_t* rec = &ldMappings[i];
            lumpnum_t absoluteLumpNum;

            if(Str_CompareIgnoreCase(&rec->path, Str_Text(&absSearchPath))) continue;

            absoluteLumpNum = F_CheckLumpNumForName2(rec->lumpName, true);
            if(absoluteLumpNum < 0) continue;

            Str_Free(&absSearchPath);
            return F_FindFileForLumpNum2(absoluteLumpNum, lumpIdx);
        }
    }

    Str_Free(&absSearchPath);
    return NULL;
}

static DFile* openAsLumpFile(abstractfile_t* container, int lumpIdx,
    const char* absolutePath, boolean isDehackedPatch, boolean dontBuffer)
{
    lumpinfo_t info;
    DFile* file;

    // Prepare the temporary info descriptor.
    F_InitLumpInfo(&info);
    F_CopyLumpInfo(&info, F_LumpInfo(container, lumpIdx));
    Str_Set(&info.path, absolutePath);
    Str_Strip(&info.path);
    F_FixSlashes(&info.path, &info.path);

    // Prepare the name of this single-lump file.
    if(isDehackedPatch)
    {
        strncpy(info.name, "DEHACKED", LUMPNAME_T_MAXLEN);
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
        F_ExtractFileBase2(info.name, Str_Text(&info.path), LUMPNAME_T_LASTINDEX, offset);
        info.name[LUMPNAME_T_LASTINDEX] = '\0';
    }

    file = DFileBuilder_NewFromAbstractFile(
        newLumpFile(DFileBuilder_NewFromAbstractFileLump(container, lumpIdx, dontBuffer), &info));

    // We're done with the descriptor.
    F_DestroyLumpInfo(&info);
    return file;
}

static DFile* tryOpenAsZipFile(DFile* file, const lumpinfo_t* info)
{
    if(inited && ZipFile_Recognise(file))
    {
        return DFileBuilder_NewFromAbstractFile(newZipFile(file, info));
    }
    return NULL;
}

static DFile* tryOpenAsWadFile(DFile* file, const lumpinfo_t* info)
{
    if(inited && WadFile_Recognise(file))
    {
        return DFileBuilder_NewFromAbstractFile(newWadFile(file, info));
    }
    return NULL;
}

static DFile* tryOpenFile3(DFile* file, const lumpinfo_t* info)
{
    assert(file && info);
    {
    static const struct filehandler_s {
        resourcetype_t resourceType;
        DFile* (*tryOpenFile)(DFile* file, const lumpinfo_t* info);
    } handlers[] = {
        { RT_ZIP,  tryOpenAsZipFile },
        { RT_WAD,  tryOpenAsWadFile },
        { RT_NONE, NULL }
    }, *hdlr = NULL;
    resourcetype_t resourceType = F_GuessResourceTypeByName(Str_Text(&info->path));
    DFile* hndl = NULL;

    // Firstly try the expected format given the file name.
    for(hdlr = handlers; NULL != hdlr->tryOpenFile; hdlr++)
    {
        if(hdlr->resourceType != resourceType) continue;

        hndl = hdlr->tryOpenFile(file, info);
        break;
    }

    // If not yet loaded; try each recognisable format.
    /// \todo Order here should be determined by the resource locator.
    { int n = 0;
    while(NULL == hndl && NULL != handlers[n].tryOpenFile)
    {
        if(hdlr != &handlers[n]) // We already know its not in this format.
        {
            hndl = handlers[n].tryOpenFile(file, info);
        }
        ++n;
    }}

    // If still not loaded; this an unknown format.
    if(!hndl)
    {
        hndl = DFileBuilder_NewFromAbstractFile(newUnknownFile(file, info));
    }
    assert(hndl);
    return hndl;
    }
}

/**
 * @param mode 'b' = binary mode
 *             't' = text mode
 *             'f' = must be a real file in the local file system.
 *             'x' = skip buffering (used with file-access and metadata-acquire processes).
 */
static DFile* tryOpenFile2(const char* path, const char* mode, size_t baseOffset, boolean allowDuplicate)
{
    ddstring_t searchPath, *foundPath = NULL;
    boolean dontBuffer, reqRealFile;
    lumpinfo_t info;
    DFile* dfile;
    FILE* file;

    if(!path || !path[0])
        return NULL;

    if(NULL == mode) mode = "";
    dontBuffer = (strchr(mode, 'x') != NULL);
    reqRealFile = (strchr(mode, 'f') != NULL);

    // Make it a full path.
    Str_Init(&searchPath); Str_Set(&searchPath, path);
    F_FixSlashes(&searchPath, &searchPath);
    F_ExpandBasePath(&searchPath, &searchPath);

    // First check for lumps?
    if(!reqRealFile)
    {
        int lumpIdx;
        abstractfile_t* container = F_FindLumpFile(Str_Text(&searchPath), &lumpIdx);
        if(container)
        {
            resourcetype_t type;

            // Do not read files twice.
            if(!allowDuplicate && !F_CheckFileId(Str_Text(&searchPath)))
            {
                return NULL;
            }

            // DeHackEd patch files require special handling...
            type = F_GuessResourceTypeByName(path);

            dfile = openAsLumpFile(container, lumpIdx, Str_Text(&searchPath), (type == RT_DEH), dontBuffer);
            Str_Free(&searchPath);
            return dfile;
        }
    }

    // Try to open as a real file then. We must have an absolute path, so
    // prepend the current working directory if necessary.
    F_PrependWorkPath(&searchPath, &searchPath);
    file = findRealFile(Str_Text(&searchPath), mode, &foundPath);
    if(!file)
    {
        Str_Free(&searchPath);
        return NULL;
    }

    // Do not read files twice.
    if(!allowDuplicate && !F_CheckFileId(Str_Text(foundPath)))
    {
        fclose(file);
        Str_Free(&searchPath);
        return NULL;
    }

    // Prepare the temporary info descriptor.
    // Search path is used here rather than found path as the latter may have
    // been mapped to another location. We want the file to be attributed with
    // the path it is to be known by throughout the virtual file system.
    F_InitLumpInfo(&info);
    Str_Set(&info.path, Str_Text(&searchPath));
    Str_Strip(&info.path);
    F_FixSlashes(&info.path, &info.path);
    info.lastModified = F_LastModified(Str_Text(foundPath));

    Str_Free(&searchPath);
    Str_Delete(foundPath);

    dfile = tryOpenFile3(DFileBuilder_NewFromFile(file, baseOffset), &info);

    // We're done with the descriptor.
    F_DestroyLumpInfo(&info);
    return dfile;
}

static DFile* tryOpenFile(const char* path, const char* mode, size_t baseOffset, boolean allowDuplicate)
{
    DFile* file = tryOpenFile2(path, mode, baseOffset, allowDuplicate);
    if(file) return FileList_AddBack(openFiles, file);
    return NULL;
}

DFile* F_Open3(const char* path, const char* mode, size_t baseOffset, boolean allowDuplicate)
{
#if _DEBUG
    uint i;
    assert(path && mode);
    for(i = 0; mode[i]; ++i)
    {
        if(mode[i] != 'r' && mode[i] != 't' && mode[i] != 'b' && mode[i] != 'f')
            Con_Error("F_Open: Unsupported file open-op in mode string %s for path \"%s\"\n", mode, path);
    }
#endif
    return tryOpenFile(path, mode, baseOffset, allowDuplicate);
}

DFile* F_Open2(const char* path, const char* mode, size_t baseOffset)
{
    return F_Open3(path, mode, baseOffset, true);
}

DFile* F_Open(const char* path, const char* mode)
{
    return F_Open2(path, mode, 0);
}

int F_Access(const char* path)
{
    DFile* file = tryOpenFile(path, "rx", 0, true);
    if(file)
    {
        F_Delete(file);
        return true;
    }
    return false;
}

uint F_GetLastModified(const char* fileName)
{
    // Try to open the file, but don't buffer any contents.
    DFile* file = tryOpenFile(fileName, "rx", 0, true);
    uint modified = 0;
    if(file)
    {
        modified = AbstractFile_LastModified(DFile_File(file));
        F_Delete(file);
    }
    return modified;
}

boolean F_AddFile(const char* path, size_t baseOffset, boolean allowDuplicate)
{
    DFile* file = F_Open3(path, "rb", baseOffset, allowDuplicate);
    abstractfile_t* fsObject;

    if(!file)
    {
        if(allowDuplicate)
        {
            Con_Message("Warning:F_AddFile: Resource \"%s\" not found, aborting.\n", path);
        }
        else if(F_Access(path))
        {
            Con_Message("\"%s\" already loaded.\n", F_PrettyPath(path));
        }
        return false;
    }
    fsObject = DFile_File(file);

    VERBOSE( Con_Message("Loading \"%s\"...\n", F_PrettyPath(Str_Text(AbstractFile_Path(fsObject)))) )

    if(loadingForStartup)
        AbstractFile_SetStartup(fsObject, true);
    FileList_AddBack(loadedFiles, DFileBuilder_NewCopy(file));

    // Publish lumps to one or more LumpDirectorys.
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
        if(AbstractFile_HasIWAD(fsObject))
            Con_Message("  IWAD identification: %08x\n", WadFile_CalculateCRC(wad));
        break;
      }
    case FT_LUMPFILE:
        LumpFile_PublishLumpsToDirectory((lumpfile_t*)fsObject, ActiveWadLumpDirectory);
        break;
    default:
        Con_Error("F_AddFile: Invalid file type %i.", (int) AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
    return true;
}

boolean F_AddFiles(const char* const* paths, size_t num, boolean allowDuplicate)
{
    boolean succeeded = false;
    { size_t i;
    for(i = 0; i < num; ++i)
    {
        if(F_AddFile(paths[i], 0, allowDuplicate))
        {
            VERBOSE2( Con_Message("Done loading %s\n", F_PrettyPath(paths[i])) );
            succeeded = true; // At least one has been loaded.
        }
        else
            Con_Message("Warning: Errors occured while loading %s\n", paths[i]);
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
    boolean unloadedResources = unloadFile(path);
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
        if(unloadFile(filenames[i]))
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

DFile* F_OpenLump(lumpnum_t absoluteLumpNum)
{
    int lumpIdx;
    abstractfile_t* container = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
    if(container)
    {
        /// \todo All lumps should be attributed with an absolute file path not just those from Zips.
        abstractfile_t* fsObject = newLumpFile(DFileBuilder_NewFromAbstractFileLump(container, lumpIdx, false), F_LumpInfo(container, lumpIdx));
        if(fsObject)
        {
            return FileList_AddBack(openFiles, DFileBuilder_NewFromAbstractFile(fsObject));
        }
    }
    return NULL;
}

static void clearLDMappings(void)
{
    if(ldMappings)
    {
        uint i;
        for(i = 0; i < ldMappingsCount; ++i)
        {
            Str_Free(&ldMappings[i].path);
        }
        free(ldMappings); ldMappings = 0;
    }
    ldMappingsCount = ldMappingsMax = 0;
}

static ldmapping_t* findLDMappingForPath(const char* path)
{
    ldmapping_t* ldm = NULL;
    if(ldMappings)
    {
        uint i = 0;
        do
        {
            if(!Str_CompareIgnoreCase(&ldMappings[i].path, path))
                ldm = &ldMappings[i];
        } while(!ldm && ++i < ldMappingsCount);
    }
    return ldm;
}

void F_AddLumpDirectoryMapping(const char* lumpName, const char* symbolicPath)
{
    ldmapping_t* ldm;
    ddstring_t fullPath, path;

    if(!lumpName || !lumpName[0] || !symbolicPath || !symbolicPath[0])
        return;

    // Convert the symbolic path into a real path.
    Str_Init(&path), Str_Set(&path, symbolicPath);
    F_ResolveSymbolicPath(&path, &path);

    // Since the path might be relative, let's explicitly make the path absolute.
    { char* full;
    Str_Init(&fullPath);
    Str_Set(&fullPath, full = _fullpath(0, Str_Text(&path), 0)); free(full);
    }
    Str_Free(&path);

    // Have already mapped this path?
    ldm = findLDMappingForPath(Str_Text(&fullPath));
    if(!ldm)
    {
        // No. Acquire another mapping.
        if(++ldMappingsCount > ldMappingsMax)
        {
            ldMappingsMax *= 2;
            if(ldMappingsMax < ldMappingsCount)
                ldMappingsMax = 2*ldMappingsCount;

            ldMappings = (ldmapping_t*) realloc(ldMappings, ldMappingsMax * sizeof *ldMappings);
            if(NULL == ldMappings)
                Con_Error("F_AddLumpDirectoryMapping: Failed on allocation of %lu bytes for mapping list.",
                    (unsigned long) (ldMappingsMax * sizeof *ldMappings));
        }
        ldm = &ldMappings[ldMappingsCount - 1];

        Str_Init(&ldm->path), Str_Set(&ldm->path, Str_Text(&fullPath));
        Str_Free(&fullPath);
    }

    // Set the lumpname.
    memcpy(ldm->lumpName, lumpName, sizeof ldm->lumpName);
    ldm->lumpName[LUMPNAME_T_LASTINDEX] = '\0';

    VERBOSE( Con_Message("F_AddLumpDirectoryMapping: \"%s\" -> %s\n", ldm->lumpName, F_PrettyPath(Str_Text(&ldm->path))) )
}

/// Skip all whitespace except newlines.
static __inline const char* skipSpace(const char* ptr)
{
    assert(ptr);
    while(*ptr && *ptr != '\n' && isspace(*ptr))
        ptr++;
    return ptr;
}

static boolean parseLDMapping(lumpname_t lumpName, ddstring_t* path, const char* buffer)
{
    assert(NULL != lumpName && NULL != path);
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

    memset(lumpName, 0, LUMPNAME_T_MAXLEN);
    strncpy(lumpName, ptr, len);
    strupr(lumpName);

    // Find the start of the file path.
    ptr = skipSpace(end);
    if(!*ptr || *ptr == '\n')
    {   // Missing file path.
        return false;
    }

    // We're at the file path.
    Str_Set(path, ptr);
    // Get rid of any extra whitespace on the end.
    Str_StripRight(path);
    F_FixSlashes(path, path);
    return true;
    }
}

/**
 * LUMPNAM0 \Path\In\The\Base.ext
 * LUMPNAM1 Path\In\The\RuntimeDir.ext
 *  :
 */
static boolean parseLDMappingList(const char* buffer)
{
    assert(buffer);
    {
    boolean successful = false;
    lumpname_t lumpName;
    ddstring_t path, line;
    const char* ch;

    Str_Init(&line);
    Str_Init(&path);
    ch = buffer;
    do
    {
        ch = Str_GetLine(&line, ch);
        if(!parseLDMapping(lumpName, &path, Str_Text(&line)))
        {   // Failure parsing the mapping.
            // Ignore errors in individual mappings and continue parsing.
            //goto parseEnded;
        }
        else
        {
            F_AddLumpDirectoryMapping(lumpName, Str_Text(&path));
        }
    } while(*ch);

    // Success.
    successful = true;

//parseEnded:
    Str_Free(&line);
    Str_Free(&path);
    return successful;
    }
}

void F_InitLumpDirectoryMappings(void)
{
    static boolean inited = false;
    size_t bufSize = 0;
    uint8_t* buf = NULL;

    if(inited)
    {   // Free old paths, if any.
        clearLDMappings();
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
                Con_Error("F_InitLumpDirectoryMappings: Failed on (re)allocation of %lu bytes for temporary read buffer.", (unsigned long) bufSize);
        }

        fsObject = F_FindFileForLumpNum2(i, &lumpIdx);
        F_ReadLumpSection(fsObject, lumpIdx, buf, 0, lumpLength);
        buf[lumpLength] = 0;
        parseLDMappingList((const char*)buf);
    }}

    if(NULL != buf)
        free(buf);

    inited = true;
}

static void clearVDMappings(void)
{
    if(vdMappings)
    {
        // Free the allocated memory.
        uint i;
        for(i = 0; i < vdMappingsCount; ++i)
        {
            Str_Free(&vdMappings[i].source);
            Str_Free(&vdMappings[i].destination);
        }
        free(vdMappings); vdMappings = 0;
    }
    vdMappingsCount = vdMappingsMax = 0;
}

/// @return  @c true, if the mapping matched the path.
static boolean applyVDMapping(ddstring_t* path, vdmapping_t* vdm)
{
    assert(NULL != path && NULL != vdm);
    if(!strnicmp(Str_Text(path), Str_Text(&vdm->destination), Str_Length(&vdm->destination)))
    {
        // Replace the beginning with the source path.
        ddstring_t temp;
        Str_Init(&temp);
        Str_Set(&temp, Str_Text(&vdm->source));
        Str_PartAppend(&temp, Str_Text(path), Str_Length(&vdm->destination), Str_Length(path) - Str_Length(&vdm->destination));
        Str_Copy(path, &temp);
        Str_Free(&temp);
        return true;
    }
    return false;
}

static vdmapping_t* findVDMappingForSourcePath(const char* source)
{
    vdmapping_t* vdm = NULL;
    if(vdMappings)
    {
        uint i = 0;
        do
        {
            if(!Str_CompareIgnoreCase(&vdMappings[i].source, source))
                vdm = &vdMappings[i];
        } while(!vdm && ++i < vdMappingsCount);
    }
    return vdm;
}

void F_AddVirtualDirectoryMapping(const char* source, const char* destination)
{
    vdmapping_t* vdm;
    ddstring_t src;

    if(!source || !source[0] || !destination || !destination[0])
        return;

    // Make this an absolute path.
    Str_Init(&src), Str_Set(&src, source);
    Str_Strip(&src);
    F_FixSlashes(&src, &src);
    if(DIR_SEP_CHAR != Str_RAt(&src, 0))
        Str_AppendChar(&src, DIR_SEP_CHAR);
    F_ExpandBasePath(&src, &src);
    F_PrependWorkPath(&src, &src);

    // Have already mapped this source path?
    vdm = findVDMappingForSourcePath(Str_Text(&src));
    if(!vdm)
    {
        // No. Acquire another mapping.
        if(++vdMappingsCount > vdMappingsMax)
        {
            vdMappingsMax *= 2;
            if(vdMappingsMax < vdMappingsCount)
                vdMappingsMax = 2*vdMappingsCount;

            vdMappings = (vdmapping_t*) realloc(vdMappings, vdMappingsMax * sizeof *vdMappings);
            if(NULL == vdMappings)
                Con_Error("F_AddVirtualDirectoryMapping: Failed on allocation of %lu bytes for mapping list.",
                    (unsigned long) (vdMappingsMax * sizeof *vdMappings));
        }
        vdm = &vdMappings[vdMappingsCount - 1];

        Str_Init(&vdm->source), Str_Set(&vdm->source, Str_Text(&src));
        Str_Init(&vdm->destination);
        Str_Free(&src);
    }

    // Set the destination.
    Str_Set(&vdm->destination, destination);
    Str_Strip(&vdm->destination);
    F_FixSlashes(&vdm->destination, &vdm->destination);
    if(DIR_SEP_CHAR != Str_RAt(&vdm->destination, 0))
        Str_AppendChar(&vdm->destination, DIR_SEP_CHAR);
    F_ExpandBasePath(&vdm->destination, &vdm->destination);
    F_PrependWorkPath(&vdm->destination, &vdm->destination);

    VERBOSE( Con_Message("Resources in \"%s\" now mapped to \"%s\"\n", Str_Text(&vdm->source), Str_Text(&vdm->destination)) );
}

void F_InitVirtualDirectoryMappings(void)
{
    int argC = Argc();

    clearVDMappings();

    // Create virtual directory mappings by processing all -vdmap options.
    { int i;
    for(i = 0; i < argC; ++i)
    {
        if(strnicmp("-vdmap", Argv(i), 6))
            continue; // This is not the option we're looking for.

        if(i < argC - 1 && !ArgIsOption(i + 1) && !ArgIsOption(i + 2))
        {
            F_AddVirtualDirectoryMapping(Argv(i + 1), Argv(i + 2));
            i += 2;
        }
    }}
}

static int C_DECL compareFileByFilePath(const void* a_, const void* b_)
{
    abstractfile_t* a = *((abstractfile_t* const*)a_);
    abstractfile_t* b = *((abstractfile_t* const*)b_);
    return Str_CompareIgnoreCase(AbstractFile_Path(a), Str_Text(AbstractFile_Path(b)));
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

static void printVFDirectory(const ddstring_t* path)
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
            printVFDirectory(&path);
        }
    }
    else
    {
        Str_Set(&path, "/");
        printVFDirectory(&path);
    }
    Str_Free(&path);
    return true;
}

D_CMD(DumpLump)
{
    if(inited)
    {
        lumpnum_t absoluteLumpNum = F_CheckLumpNumForName2(argv[1], true);
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
        int fileCount, i;
        abstractfile_t** ptr, **arr = FileList_ToArray(loadedFiles, &fileCount);
        if(!arr) return true;

        // Sort files so we get a nice alpha-numerical list.
        qsort(arr, fileCount, sizeof *arr, compareFileByFilePath);

        ptr = arr;
        for(i = 0; i < fileCount; ++i, ptr++)
        {
            size_t fileCount;
            uint crc;

            switch(AbstractFile_Type(*ptr))
            {
            case FT_UNKNOWNFILE:
                fileCount = 1;
                crc = 0;
                break;
            case FT_ZIPFILE:
                fileCount = ZipFile_LumpCount((zipfile_t*)*ptr);
                crc = 0;
                break;
            case FT_WADFILE: {
                wadfile_t* wad = (wadfile_t*)*ptr;
                crc = (AbstractFile_HasIWAD(*ptr)? WadFile_CalculateCRC(wad) : 0);
                fileCount = WadFile_LumpCount(wad);
                break;
              }
            case FT_LUMPFILE:
                fileCount = LumpFile_LumpCount((lumpfile_t*)*ptr);
                crc = 0;
                break;
            default:
                Con_Error("CCmdListLumps: Invalid file type %i.", AbstractFile_Type(*ptr));
                exit(1); // Unreachable.
            }

            Con_Printf("\"%s\" (%lu %s%s)", F_PrettyPath(Str_Text(AbstractFile_Path(*ptr))),
                (unsigned long) fileCount, fileCount != 1 ? "files" : "file",
                (AbstractFile_HasStartup(*ptr)? ", startup" : ""));
            if(0 != crc)
                Con_Printf(" [%08x]", crc);
            Con_Printf("\n");

            totalFiles += fileCount;
            ++totalPackages;
        }

        free(arr);
    }
    Con_Printf("Total: %lu files in %lu packages.\n", (unsigned long) totalFiles, (unsigned long)totalPackages);
    return true;
}
