/**\file fs_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include <ctype.h>

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

static LumpDirectory* zipLumpDirectory;

static LumpDirectory* primaryWadLumpDirectory;
static LumpDirectory* auxiliaryWadLumpDirectory;
// @c true = one or more files have been opened using the auxiliary directory.
static boolean auxiliaryWadLumpDirectoryInUse;

// Currently selected lump directory.
static LumpDirectory* ActiveWadLumpDirectory;

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
    if(path_ && path_[0] && !FileList_Empty(list))
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

static abstractfile_t* newUnknownFile(DFile* file, const char* path, const LumpInfo* info)
{
    abstractfile_t* af = (abstractfile_t*)malloc(sizeof *af);
    if(!af) Con_Error("newUnknownFile: Failed on allocation of %lu bytes for new abstractfile_t.",
        (unsigned long) sizeof *af);
    return AbstractFile_Init(af, FT_UNKNOWNFILE, path, file, info);
}

static abstractfile_t* newZipFile(DFile* file, const char* path, const LumpInfo* info)
{
    return (abstractfile_t*)ZipFile_New(file, path, info);
}

static abstractfile_t* newWadFile(DFile* file, const char* path, const LumpInfo* info)
{
    return (abstractfile_t*)WadFile_New(file, path, info);
}

static abstractfile_t* newLumpFile(DFile* file, const char* path, const LumpInfo* info)
{
    return (abstractfile_t*)LumpFile_New(file, path, info);
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
    case FT_ZIPFILE:  ZipFile_ClearLumpCache((ZipFile*)af); break;
    case FT_WADFILE:  WadFile_ClearLumpCache((WadFile*)af); break;
    default: break;
    }

    F_ReleaseFileId(Str_Text(AbstractFile_Path(af)));
    pruneLumpsFromDirectorysByFile(af);

    F_Delete(FileList_RemoveAt(loadedFiles, loadedFilesNodeIndex));
    return true;
}

static void clearLoadedFiles(LumpDirectory* directory)
{
    abstractfile_t* file;
    int i;
    for(i = FileList_Size(loadedFiles) - 1; i >= 0; i--)
    {
        file = FileList_GetFile(loadedFiles, i);
        if(!directory || LumpDirectory_Catalogues(directory, file))
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

static boolean unloadFile2(const char* path, boolean permitRequired, boolean quiet)
{
    int idx;
    errorIfNotInited("unloadFile2");
    idx = findFileNodeIndexForPath(loadedFiles, path);
    if(idx >= 0)
    {
        // Do not attempt to unload a resource required by the current game.
        if(!permitRequired && Game_IsRequiredResource(theGame, path))
        {
            if(!quiet)
            {
                Con_Message("\"%s\" is required by the current game.\n"
                            "Required game files cannot be unloaded in isolation.\n", F_PrettyPath(path));
            }
            return false;
        }

        if(!quiet && verbose >= 1)
        {
            Con_Message("Unloading \"%s\"...\n", F_PrettyPath(path));
        }

        return removeLoadedFile(idx);
    }
    return false; // No such file loaded.
}

static boolean unloadFile(const char* path, boolean permitRequired)
{
    return unloadFile2(path, permitRequired, false/*do log issues*/);
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
    uint i;
    if(!identifier) return;
    for(i = 0; i < 16; ++i)
    {
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

void F_InitLumpInfo(LumpInfo* info)
{
    assert(info);
    info->lumpIdx = 0;
    info->size = 0;
    info->compressedSize = 0;
    info->lastModified = 0;
    info->container = NULL;
}

void F_CopyLumpInfo(LumpInfo* dst, const LumpInfo* src)
{
    assert(dst && src);
    dst->lumpIdx = src->lumpIdx;
    dst->size = src->size;
    dst->compressedSize = src->compressedSize;
    dst->lastModified = src->lastModified;
    dst->container = src->container;
}

void F_DestroyLumpInfo(LumpInfo* info)
{
    // Nothing to do.
}

void F_Init(void)
{
    if(inited) return; // Already been here.

    // This'll force the loader NOT to flag new files as "runtime".
    loadingForStartup = true;

    DFileBuilder_Init();

    openFiles     = FileList_New();
    loadedFiles   = FileList_New();

    zipLumpDirectory          = LumpDirectory_NewWithFlags(LDF_UNIQUE_PATHS);
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
    abstractfile_t* file;
    int i, unloaded = 0;
    assert(list);

    for(i = FileList_Size(list) - 1; i >= 0; i--)
    {
        file = FileList_GetFile(list, i);
        if(!nonStartup || !AbstractFile_HasStartup(file))
        {
            if(unloadFile2(Str_Text(AbstractFile_Path(file)),
                           true/*allow unloading game resources*/, true/*quiet please*/))
            {
                ++unloaded;
            }
        }
    }
    return unloaded;
}

#if _DEBUG
static void logOrphanedFileIdentifiers(void)
{
    fileidentifierid_t nullId;
    uint i, orphanCount = 0;

    memset(nullId, 0, sizeof nullId);

    for(i = 0; i < fileIdentifiersCount; ++i)
    {
        fileidentifier_t* id = fileIdentifiers + i;
        if(!memcmp(id->hash, &nullId, FILEIDENTIFIERID_T_LASTINDEX)) continue;

        if(!orphanCount)
        {
            Con_Printf("Warning: Orphan file identifiers:\n");
        }

        Con_Printf("  %u - ", orphanCount);
        F_PrintFileId(id->hash);
        Con_Printf("\n");

        orphanCount++;
    }
}
#endif

int F_Reset(void)
{
    int unloaded = 0;
    if(!inited) return 0;

#if _DEBUG
    // List all open files with their identifiers.
    VERBOSE(
        Con_Printf("Open files at reset:\n");
        FileList_Print(openFiles);
        Con_Printf("End\n")
    )
#endif

    // Perform non-startup file unloading...
    unloaded = unloadListFiles(loadedFiles, true/*non-startup*/);

#if _DEBUG
    // Sanity check: look for orphaned identifiers.
    logOrphanedFileIdentifiers();
#endif

    // Reset file IDs so previously seen files can be processed again.
    /// @todo this releases the ID of startup files too but given the
    /// only startup file is doomsday.pk3 which we never attempt to load
    /// again post engine startup, this isn't an immediate problem.
    F_ResetFileIds();

    // Update the dir/WAD translations.
    F_InitLumpDirectoryMappings();
    F_InitVirtualDirectoryMappings();

    return unloaded;
}

boolean F_IsValidLumpNum(lumpnum_t absoluteLumpNum)
{
    lumpnum_t lumpNum;
    errorIfNotInited("F_IsValidLumpNum");
    lumpNum = chooseWadLumpDirectory(absoluteLumpNum);
    return LumpDirectory_IsValidIndex(ActiveWadLumpDirectory, lumpNum);
}

typedef enum lumpsizecondition_e {
    LSCOND_NONE,
    LSCOND_EQUAL,
    LSCOND_GREATER_OR_EQUAL,
    LSCOND_LESS_OR_EQUAL
} lumpsizecondition_t;

/**
 * Modifies the name so that the size condition is removed.
 */
static void checkSizeConditionInName(ddstring_t* name, lumpsizecondition_t* pCond, size_t* pSize)
{
    int i;
    int len = Str_Length(name);
    const char* txt = Str_Text(name);
    int argPos = 0;

    assert(pCond != 0);
    assert(pSize != 0);

    *pCond = LSCOND_NONE;
    *pSize = 0;

    for(i = 0; i < len - 2; ++i, ++txt)
    {
        if(!strncmp(txt, "==", 2))
        {
            *pCond = LSCOND_EQUAL;
            argPos = i + 2;
            break;
        }
        if(!strncmp(txt, ">=", 2))
        {
            *pCond = LSCOND_GREATER_OR_EQUAL;
            argPos = i + 2;
            break;
        }
        if(!strncmp(txt, "<=", 2))
        {
            *pCond = LSCOND_LESS_OR_EQUAL;
            argPos = i + 2;
            break;
        }
    }
    if(!argPos) return;

    // Get the argument.
    *pSize = strtoul(txt + 2, NULL, 10);

    // Remove it from the name.
    Str_Truncate(name, i);
}

lumpnum_t F_CheckLumpNumForName2(const char* name, boolean silent)
{
    lumpnum_t lumpNum = -1;
    lumpsizecondition_t sizeCond;
    size_t lumpSize = 0;
    size_t refSize;

    errorIfNotInited("F_CheckLumpNumForName");

    if(name && name[0])
    {
        ddstring_t searchPath;

        Str_Init(&searchPath); Str_Set(&searchPath, name);

        // The name may contain a size condition (==, >=, <=).
        checkSizeConditionInName(&searchPath, &sizeCond, &refSize);

        // Append a .lmp extension if none is specified.
        if(!F_FindFileExtension(name))
        {
            Str_Append(&searchPath, ".lmp");
        }

        // We have to check both the primary and auxiliary caches because
        // we've only got a name and don't know where it is located. Start with
        // the auxiliary lumps because they take precedence.
        if(useAuxiliaryWadLumpDirectory())
        {
            lumpNum = LumpDirectory_IndexForPath(ActiveWadLumpDirectory, Str_Text(&searchPath));

            if(lumpNum >= 0 && sizeCond != LSCOND_NONE)
            {
                // Get the size as well for the condition check.
                lumpSize = LumpDirectory_LumpInfo(ActiveWadLumpDirectory, lumpNum)->size;
            }
        }

        // Found it yet?
        if(lumpNum < 0)
        {
            usePrimaryWadLumpDirectory();
            lumpNum = LumpDirectory_IndexForPath(ActiveWadLumpDirectory, Str_Text(&searchPath));

            if(lumpNum >= 0 && sizeCond != LSCOND_NONE)
            {
                // Get the size as well for the condition check.
                lumpSize = LumpDirectory_LumpInfo(ActiveWadLumpDirectory, lumpNum)->size;
            }
        }

        // Check the condition.
        switch(sizeCond)
        {
        case LSCOND_EQUAL:
            if(lumpSize != refSize) lumpNum = -1;
            break;

        case LSCOND_GREATER_OR_EQUAL:
            if(lumpSize < refSize) lumpNum = -1;
            break;

        case LSCOND_LESS_OR_EQUAL:
            if(lumpSize > refSize) lumpNum = -1;
            break;

        default:
            break;
        }

        // If still not found, warn the user.
        if(!silent && lumpNum < 0)
        {
            if(sizeCond == LSCOND_NONE)
            {
                Con_Message("Warning: F_CheckLumpNumForName: Lump \"%s\" not found.\n", name);
            }
            else
            {
                Con_Message("Warning: F_CheckLumpNumForName: Lump \"%s\" with size%s%i not found.\n",
                            Str_Text(&searchPath), sizeCond==LSCOND_EQUAL? "==" :
                            sizeCond==LSCOND_GREATER_OR_EQUAL? ">=" : "<=", (int)refSize);
            }
        }

        Str_Free(&searchPath);
    }
    else if(!silent)
    {
        Con_Message("Warning: F_CheckLumpNumForName: Empty name, returning invalid lumpnum.\n");
    }
    return logicalLumpNum(lumpNum);
}

lumpnum_t F_CheckLumpNumForName(const char* name)
{
    return F_CheckLumpNumForName2(name, false);
}

const LumpInfo* F_FindInfoForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx)
{
    lumpnum_t translated = chooseWadLumpDirectory(absoluteLumpNum);
    const LumpInfo* lumpInfo = LumpDirectory_LumpInfo(ActiveWadLumpDirectory, translated);
    if(lumpIdx) *lumpIdx = (lumpInfo? lumpInfo->lumpIdx : -1);
    return lumpInfo;
}

const LumpInfo* F_FindInfoForLumpNum(lumpnum_t absoluteLumpNum)
{
    return F_FindInfoForLumpNum2(absoluteLumpNum, NULL);
}

const char* F_LumpName(lumpnum_t absoluteLumpNum)
{
    const LumpInfo* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(info)
    {
        PathDirectoryNode* node = F_LumpDirectoryNode(info->container, info->lumpIdx);
        return Str_Text(PathDirectory_GetFragment(PathDirectoryNode_Directory(node), node));
    }
    return "";
}

size_t F_LumpLength(lumpnum_t absoluteLumpNum)
{
    const LumpInfo* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(info) return info->size;
    return 0;
}

uint F_LumpLastModified(lumpnum_t absoluteLumpNum)
{
    const LumpInfo* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(info) return info->lastModified;
    return 0;
}

abstractfile_t* F_FindFileForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx)
{
    const LumpInfo* lumpInfo = F_FindInfoForLumpNum2(absoluteLumpNum, lumpIdx);
    if(!lumpInfo) return NULL;
    return lumpInfo->container;
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
    if(fsObject) return AbstractFile_HasCustom(fsObject);
    return false;
}

int F_LumpCount(void)
{
    if(inited)
        return LumpDirectory_Size(ActiveWadLumpDirectory);
    return 0;
}

lumpnum_t F_OpenAuxiliary3(const char* path, size_t baseOffset, boolean silent)
{
    ddstring_t* foundPath = NULL;
    ddstring_t searchPath;
    DFile* dfile;
    FILE* file;

    errorIfNotInited("F_OpenAuxiliary");

    if(!path || !path[0]) return -1;

    // Make it a full path.
    Str_Init(&searchPath); Str_Set(&searchPath, path);
    F_ExpandBasePath(&searchPath, &searchPath);
    // We must have an absolute path, so prepend the current working directory if necessary.
    F_PrependWorkPath(&searchPath, &searchPath);

    /// @todo Allow opening WAD/ZIP files from lumps in other containers.
    file = findRealFile(Str_Text(&searchPath), "rb", &foundPath);
    Str_Free(&searchPath);
    if(!file)
    {
        if(foundPath)
        {
            Str_Delete(foundPath);
        }
        if(!silent)
        {
            Con_Message("Warning:F_OpenAuxiliary: Resource \"%s\" not found, aborting.\n", path);
        }
        return -1;
    }

    dfile = DFileBuilder_NewFromFile(file, baseOffset);

    if(WadFile_Recognise(dfile))
    {
        LumpInfo info;

        if(auxiliaryWadLumpDirectoryInUse)
        {
            F_CloseAuxiliary();
        }
        ActiveWadLumpDirectory = auxiliaryWadLumpDirectory;
        auxiliaryWadLumpDirectoryInUse = true;

        // Prepare the temporary info descriptor.
        F_InitLumpInfo(&info);
        info.lastModified = F_LastModified(Str_Text(foundPath));

        dfile = DFileBuilder_NewFromAbstractFile(newWadFile(dfile, Str_Text(foundPath), &info));
        FileList_AddBack(openFiles, dfile);
        FileList_AddBack(loadedFiles, DFileBuilder_NewCopy(dfile));
        WadFile_PublishLumpsToDirectory((WadFile*)DFile_File(dfile), ActiveWadLumpDirectory);

        Str_Delete(foundPath);

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

    if(foundPath) Str_Delete(foundPath);
    DFile_Delete(dfile, true);
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

    case FT_ZIPFILE:  ZipFile_Delete( ( ZipFile*)af); break;
    case FT_WADFILE:  WadFile_Delete( ( WadFile*)af); break;
    case FT_LUMPFILE: LumpFile_Delete((LumpFile*)af); break;
    default:
        Con_Error("F_Delete: Invalid file type %i.", AbstractFile_Type(af));
        exit(1); // Unreachable.
    }
    DFile_Delete(file, true);
}

const LumpInfo* F_LumpInfo(abstractfile_t* fsObject, int lumpIdx)
{
    assert(fsObject);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:    return  ZipFile_LumpInfo( (ZipFile*)fsObject, lumpIdx);
    case FT_WADFILE:    return  WadFile_LumpInfo( (WadFile*)fsObject, lumpIdx);
    case FT_LUMPFILE:   return LumpFile_LumpInfo((LumpFile*)fsObject, lumpIdx);
    default:
        Con_Error("F_LumpInfo: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

PathDirectoryNode* F_LumpDirectoryNode(abstractfile_t* fsObject, int lumpIdx)
{
    assert(fsObject);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE: return ZipFile_LumpDirectoryNode((ZipFile*)fsObject, lumpIdx);
    case FT_WADFILE: return WadFile_LumpDirectoryNode((WadFile*)fsObject, lumpIdx);
    case FT_LUMPFILE: return F_LumpDirectoryNode(AbstractFile_Container(fsObject),
                                                 LumpFile_LumpInfo((LumpFile*)fsObject, lumpIdx)->lumpIdx);
    default: return NULL;
    }
}

ddstring_t* F_ComposeLumpPath2(abstractfile_t* fsObject, int lumpIdx, char delimiter)
{
    assert(fsObject);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:  return ZipFile_ComposeLumpPath((ZipFile*)fsObject, lumpIdx, delimiter);
    case FT_WADFILE:  return WadFile_ComposeLumpPath((WadFile*)fsObject, lumpIdx, delimiter);
    case FT_LUMPFILE: return F_ComposeLumpPath2(AbstractFile_Container(fsObject),
                                                LumpFile_LumpInfo((LumpFile*)fsObject, lumpIdx)->lumpIdx, delimiter);
    default:
        Con_Error("F_ComposeLumpPath: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

ddstring_t* F_ComposeLumpPath(abstractfile_t* fsObject, int lumpIdx)
{
    return F_ComposeLumpPath2(fsObject, lumpIdx, '/');
}

size_t F_ReadLumpSection(abstractfile_t* fsObject, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    assert(fsObject);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:  return ZipFile_ReadLumpSection((ZipFile*)fsObject, lumpIdx, buffer, startOffset, length);
    case FT_WADFILE:  return WadFile_ReadLumpSection((WadFile*)fsObject, lumpIdx, buffer, startOffset, length);
    case FT_LUMPFILE: return F_ReadLumpSection(AbstractFile_Container(fsObject),
                                               LumpFile_LumpInfo((LumpFile*)fsObject, lumpIdx)->lumpIdx, buffer, startOffset, length);
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
    case FT_ZIPFILE:  return ZipFile_CacheLump((ZipFile*)fsObject, lumpIdx, tag);
    case FT_WADFILE:  return WadFile_CacheLump((WadFile*)fsObject, lumpIdx, tag);
    case FT_LUMPFILE: return F_CacheLump(AbstractFile_Container(fsObject),
                                         LumpFile_LumpInfo((LumpFile*)fsObject, lumpIdx)->lumpIdx, tag);
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
    case FT_ZIPFILE:  ZipFile_ChangeLumpCacheTag((ZipFile*)fsObject, lumpIdx, tag); break;
    case FT_WADFILE:  WadFile_ChangeLumpCacheTag((WadFile*)fsObject, lumpIdx, tag); break;
    case FT_LUMPFILE: F_CacheChangeTag(AbstractFile_Container(fsObject),
                                       LumpFile_LumpInfo((LumpFile*)fsObject, lumpIdx)->lumpIdx, tag); break;

    default:
        Con_Error("F_CacheChangeTag: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

boolean F_DumpLump(lumpnum_t absoluteLumpNum, const char* path)
{
    const LumpInfo* info;
    abstractfile_t* fsObject;
    ddstring_t nativePath;
    const char* lumpName;
    const char* fname;
    int lumpIdx;
    FILE* outFile;

    fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
    if(!fsObject) return false;

    info = F_LumpInfo(fsObject, lumpIdx);
    if(!info) return false;

    lumpName = F_LumpName(absoluteLumpNum);
    if(path && path[0])
    {
        fname = path;
    }
    else
    {
        fname = lumpName;
    }

    Str_Init(&nativePath); Str_Set(&nativePath, fname);
    F_ToNativeSlashes(&nativePath, &nativePath);

    outFile = fopen(Str_Text(&nativePath), "wb");
    if(!outFile)
    {
        Con_Printf("Warning: Failed to open \"%s\" for writing (error: %s), aborting.\n", F_PrettyPath(Str_Text(&nativePath)), strerror(errno));
        Str_Free(&nativePath);
        return false;
    }

    fwrite((void*)F_CacheLump(fsObject, lumpIdx, PU_APPSTATIC), 1, info->size, outFile);
    fclose(outFile);
    F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);

    Con_Printf("%s dumped to \"%s\"\n", lumpName, F_PrettyPath(Str_Text(&nativePath)));
    Str_Free(&nativePath);
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
            if(FT_WADFILE == AbstractFile_Type(file) && !AbstractFile_HasCustom(file))
            {
                found = file;
            }
        } while(!found && ++i < FileList_Size(loadedFiles));
        if(found)
        {
            return WadFile_CalculateCRC((WadFile*)found);
        }
    }
    return 0;
}

typedef struct {
    filetype_t type; // Only
    boolean includeOriginal;
    boolean includeCustom;
} compositepathpredicateparamaters_t;

boolean C_DECL compositePathPredicate(DFile* hndl, void* paramaters)
{
    compositepathpredicateparamaters_t* p = (compositepathpredicateparamaters_t*)paramaters;
    abstractfile_t* file = DFile_File(hndl);
    assert(p);
    if((!VALID_FILETYPE(p->type) || p->type == AbstractFile_Type(file)) &&
       ((p->includeOriginal && !AbstractFile_HasCustom(file)) ||
        (p->includeCustom   &&  AbstractFile_HasCustom(file))))
    {
        const ddstring_t* path = AbstractFile_Path(file);
        if(stricmp(Str_Text(path) + Str_Length(path) - 3, "lmp"))
            return true; // Include this.
    }
    return false; // Not this.
}

void F_GetPWADFileNames(char* outBuf, size_t outBufSize, const char* delimiter)
{
    if(!outBuf || 0 == outBufSize) return;
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
static foundentry_t* collectLocalPaths(const ddstring_t* searchPath, int* retCount,
    boolean includeSearchPath)
{
    ddstring_t wildPath, origWildPath;
    foundentry_t* foundEntries = NULL;
    int i, count = 0, max = 0;
    finddata_t fd;

    Str_Init(&origWildPath);
    Str_Appendf(&origWildPath, "%s*", Str_Text(searchPath));

    Str_Init(&wildPath);
    for(i = -1; i < (int)vdMappingsCount; ++i)
    {
        if(i == -1)
        {
            Str_Copy(&wildPath, &origWildPath);
        }
        else
        {
            // Possible mapping?
            Str_Copy(&wildPath, searchPath);
            if(!applyVDMapping(&wildPath, &vdMappings[i]))
                continue; // No.

            Str_AppendChar(&wildPath, '*');
        }

        if(!myfindfirst(Str_Text(&wildPath), &fd))
        {
            // First path found.
            do
            {
                // Ignore relative directory symbolics.
                if(Str_Compare(&fd.name, ".") && Str_Compare(&fd.name, ".."))
                {
                    foundentry_t* found;

                    if(count >= max)
                    {
                        if(0 == max)
                            max = 16;
                        else
                            max *= 2;

                        foundEntries = (foundentry_t*)realloc(foundEntries, max * sizeof(*foundEntries));
                        if(!foundEntries)
                            Con_Error("collectLocalPaths: Failed on (re)allocation of %lu bytes while "
                                "resizing found path collection.", (unsigned long) (max * sizeof(*foundEntries)));
                    }

                    found = &foundEntries[count];

                    Str_Init(&found->path);
                    if(includeSearchPath)
                        Str_Append(&found->path, Str_Text(searchPath));
                    Str_Append(&found->path, Str_Text(&fd.name));

                    found->attrib = fd.attrib;

                    ++count;
                }
            } while(!myfindnext(&fd));
        }
        myfindend(&fd);
    }

    Str_Free(&origWildPath);
    Str_Free(&wildPath);

    if(retCount) *retCount = count;

    if(0 != count)
        return foundEntries;
    return NULL;
}

static int iterateLocalPaths(const ddstring_t* searchDirectory, const ddstring_t* pattern,
    int (*callback) (const ddstring_t* path, pathdirectorynode_type_t type, void* paramaters),
    void* paramaters)
{
    int result = 0, count;
    foundentry_t* foundPaths;

    if(!callback || !searchDirectory || Str_IsEmpty(searchDirectory)) return 0;

    foundPaths = collectLocalPaths(searchDirectory, &count, true/*include the searchDirectory in paths*/);
    if(foundPaths)
    {
        ddstring_t localPattern;
        int i;

        // Sort all the foundPaths entries.
        qsort(foundPaths, count, sizeof(*foundPaths), compareFoundEntryByPath);

        Str_Init(&localPattern);
        Str_Appendf(&localPattern, "%s%s", Str_Text(searchDirectory), pattern? Str_Text(pattern) : "");

        for(i = 0; i < count; ++i)
        {
            foundentry_t* found = &foundPaths[i];

            // Is the caller's iteration still in progress?
            if(0 == result)
            {
                // Does this match the pattern?
                if(F_MatchFileName(Str_Text(&found->path), Str_Text(&localPattern)))
                {
                    // Pass this path to the caller.
                    result = callback(&found->path, (foundPaths[i].attrib & A_SUBDIR)? PT_BRANCH : PT_LEAF, paramaters);
                }
            }

            // We're done with this path.
            Str_Free(&found->path);
        }

        Str_Free(&localPattern);
        free(foundPaths);
    }

    return result;
}

typedef struct {
    /// Callback to make for each processed file.
    int (*callback) (const ddstring_t* path, pathdirectorynode_type_t type, void* paramaters);

    /// Data passed to the callback.
    void* paramaters;

    int flags; /// @see searchPathFlags

    /// Current search pattern.
    ddstring_t pattern;
    PathMap patternMap;
} findlumpworker_paramaters_t;

static int findLumpWorker(const LumpInfo* lumpInfo, void* paramaters)
{
    findlumpworker_paramaters_t* p = (findlumpworker_paramaters_t*)paramaters;
    PathDirectoryNode* node = F_LumpDirectoryNode(lumpInfo->container, lumpInfo->lumpIdx);
    ddstring_t* filePath = NULL;
    boolean patternMatched;
    int result = 0; // Continue iteration.
    assert(lumpInfo && p);

    if(!node) return 0; // Most irregular...

    if(!(p->flags & SPF_NO_DESCEND))
    {
        filePath = F_ComposeLumpPath(lumpInfo->container, lumpInfo->lumpIdx);
        patternMatched = F_MatchFileName(Str_Text(filePath), Str_Text(&p->pattern));
    }
    else
    {
        patternMatched = PathDirectoryNode_MatchDirectory(node, PCF_MATCH_FULL, &p->patternMap, NULL);
        if(patternMatched)
        {
            filePath = F_ComposeLumpPath(lumpInfo->container, lumpInfo->lumpIdx);
        }
    }

    if(patternMatched)
    {
        result = p->callback(filePath, PT_LEAF, p->paramaters);
    }

    if(filePath) Str_Delete(filePath);
    return result;
}

int F_AllResourcePaths2(const char* rawSearchPattern, int flags,
    int (*callback) (const ddstring_t* path, pathdirectorynode_type_t type, void* paramaters),
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
    p.flags = flags;
    Str_Init(&p.pattern); Str_Set(&p.pattern, Str_Text(&searchPattern));
    PathMap_Initialize(&p.patternMap, Str_Text(&searchPattern));

    result = LumpDirectory_Iterate2(zipLumpDirectory, NULL, findLumpWorker, (void*)&p);
    PathMap_Destroy(&p.patternMap);
    Str_Free(&p.pattern);

    if(result) goto searchEnded;
    }

    // Check the dir/WAD direcs.
    if(ldMappingsCount)
    {
        uint i;
        for(i = 0; i < ldMappingsCount; ++i)
        {
            ldmapping_t* rec = &ldMappings[i];
            if(!F_MatchFileName(Str_Text(&rec->path), Str_Text(&searchPattern))) continue;

            result = callback(&rec->path, PT_LEAF, paramaters);
            if(result) goto searchEnded;
        }
    }

    /**
     * Check real files on the search path.
     * Our existing normalized search pattern cannot be used as-is due to the
     * interface of the search algorithm requiring that the name and directory of
     * the pattern be specified separately.
     */

    // Extract the directory path.
    Str_Init(&searchDirectory);
    F_FileDir(&searchDirectory, &searchPattern);

    // Extract just the name and/or extension.
    Str_Init(&searchName);
    F_FileNameAndExtension(&searchName, Str_Text(&searchPattern));

    result = iterateLocalPaths(&searchDirectory, &searchName, callback, paramaters);

    Str_Free(&searchName);
    Str_Free(&searchDirectory);

searchEnded:
    Str_Free(&searchPattern);
    return result;
}

int F_AllResourcePaths(const char* searchPath, int flags,
    int (*callback) (const ddstring_t* path, pathdirectorynode_type_t type, void* paramaters))
{
    return F_AllResourcePaths2(searchPath, flags, callback, 0);
}

static FILE* findRealFile(const char* path, const char* mymode, ddstring_t** foundPath)
{
    ddstring_t* mapped, nativePath;
    char mode[8];
    FILE* file;
    uint i;

    strcpy(mode, "r"); // Open for reading.
    if(strchr(mymode, 't'))
        strcat(mode, "t");
    if(strchr(mymode, 'b'))
        strcat(mode, "b");

    Str_Init(&nativePath); Str_Set(&nativePath, path);
    F_ToNativeSlashes(&nativePath, &nativePath);

    // Try opening as a real file.
    file = fopen(Str_Text(&nativePath), mode);
    if(file)
    {
        if(foundPath)
        {
            *foundPath = Str_Set(Str_New(), path);
        }
        Str_Free(&nativePath);
        return file;
    }

    // Any applicable virtual directory mappings?
    if(vdMappingsCount == 0)
    {
        Str_Free(&nativePath);
        return NULL;
    }

    mapped = Str_New();
    for(i = 0; i < vdMappingsCount; ++i)
    {
        Str_Set(mapped, path);
        if(!applyVDMapping(mapped, &vdMappings[i])) continue;
        // The mapping was successful.

        F_ToNativeSlashes(&nativePath, mapped);
        file = fopen(Str_Text(&nativePath), mode);
        if(file)
        {
            VERBOSE( Con_Message("findRealFile: \"%s\" opened as %s.\n", F_PrettyPath(Str_Text(mapped)), path) )
            if(foundPath)
                *foundPath = mapped;
            else
                Str_Delete(mapped);
            Str_Free(&nativePath);
            return file;
        }
    }
    Str_Delete(mapped);
    Str_Free(&nativePath);
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
        const LumpInfo* lumpInfo = LumpDirectory_LumpInfo(zipLumpDirectory, lumpNum);
        assert(lumpInfo);
        if(lumpIdx) *lumpIdx = lumpInfo->lumpIdx;
        Str_Free(&absSearchPath);
        return lumpInfo->container;
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

static DFile* tryOpenFile3(DFile* file, const char* path, const LumpInfo* info);

static DFile* openAsLumpFile(abstractfile_t* container, int lumpIdx,
    const char* _absPath, boolean isDehackedPatch, boolean dontBuffer)
{
    DFile* file, *hndl;
    ddstring_t absPath;
    LumpInfo info;

    Str_Init(&absPath);
    // Prepare the name of this single-lump file.
    if(isDehackedPatch)
    {
        // Copy the path up to and including the last directory separator if present.
        char* slash = strrchr(_absPath, '/');
        if(slash)
        {
            Str_PartAppend(&absPath, _absPath, 0, (slash - _absPath) + 1);
        }
        Str_Append(&absPath, "DEHACKED.lmp");
    }
    else
    {
        Str_Append(&absPath, _absPath);
    }

    // Get a handle to the lump we intend to open.
    /// @fixme The way this buffering works is nonsensical it should not be done here
    ///        but should instead be deferred until the content of the lump is read.
    hndl = DFileBuilder_NewFromAbstractFileLump(container, lumpIdx, false/*dontBuffer*/);

    // Prepare the temporary info descriptor.
    F_InitLumpInfo(&info);
    F_CopyLumpInfo(&info, F_LumpInfo(container, lumpIdx));

    // Try to open the referenced file as a specialised file type.
    file = tryOpenFile3(hndl, Str_Text(&absPath), &info);

    // If not opened; assume its a generic LumpFile.
    if(!file)
    {
        file = DFileBuilder_NewFromAbstractFile(newLumpFile(hndl, Str_Text(&absPath), &info));
    }
    assert(file);

    // We're done with the descriptor.
    F_DestroyLumpInfo(&info);

    Str_Free(&absPath);

    return file;
}

static DFile* tryOpenAsZipFile(DFile* file, const char* path, const LumpInfo* info)
{
    if(inited && ZipFile_Recognise(file))
    {
        return DFileBuilder_NewFromAbstractFile(newZipFile(file, path, info));
    }
    return NULL;
}

static DFile* tryOpenAsWadFile(DFile* file, const char* path, const LumpInfo* info)
{
    if(inited && WadFile_Recognise(file))
    {
        return DFileBuilder_NewFromAbstractFile(newWadFile(file, path, info));
    }
    return NULL;
}

static DFile* tryOpenFile3(DFile* file, const char* path, const LumpInfo* info)
{
    static const struct filehandler_s {
        resourcetype_t resourceType;
        DFile* (*tryOpenFile)(DFile* file, const char* path, const LumpInfo* info);
    } handlers[] = {
        { RT_ZIP,  tryOpenAsZipFile },
        { RT_WAD,  tryOpenAsWadFile },
        { RT_NONE, NULL }
    }, *hdlr = NULL;
    resourcetype_t resourceType;
    DFile* hndl = NULL;
    int n = 0;
    assert(file && path && path[0] && info);

    resourceType = F_GuessResourceTypeByName(path);

    // Firstly try the expected format given the file name.
    for(hdlr = handlers; hdlr->tryOpenFile; hdlr++)
    {
        if(hdlr->resourceType != resourceType) continue;

        hndl = hdlr->tryOpenFile(file, path, info);
        break;
    }

    // If not yet loaded; try each recognisable format.
    /// @todo Order here should be determined by the resource locator.
    while(!hndl && handlers[n].tryOpenFile)
    {
        if(hdlr != &handlers[n]) // We already know its not in this format.
        {
            hndl = handlers[n].tryOpenFile(file, path, info);
        }
        ++n;
    }

    return hndl;
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
    DFile* dfile, *hndl;
    LumpInfo info;
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

    DEBUG_VERBOSE2_Message(("tryOpenFile2: trying to open %s\n", Str_Text(&searchPath)));

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
        Str_Delete(foundPath);
        Str_Free(&searchPath);
        return NULL;
    }

    // Acquire a handle on the file we intend to open.
    hndl = DFileBuilder_NewFromFile(file, baseOffset);

    // Prepare the temporary info descriptor.
    F_InitLumpInfo(&info);
    info.lastModified = F_LastModified(Str_Text(foundPath));

    // Search path is used here rather than found path as the latter may have
    // been mapped to another location. We want the file to be attributed with
    // the path it is to be known by throughout the virtual file system.

    dfile = tryOpenFile3(hndl, Str_Text(&searchPath), &info);
    // If still not loaded; this an unknown format.
    if(!dfile)
    {
        dfile = DFileBuilder_NewFromAbstractFile(newUnknownFile(hndl, Str_Text(&searchPath), &info));
    }
    assert(dfile);

    // We're done with the descriptor.
    F_DestroyLumpInfo(&info);

    Str_Delete(foundPath);
    Str_Free(&searchPath);

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

DFile* F_AddFile(const char* path, size_t baseOffset, boolean allowDuplicate)
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
        return NULL;
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
        ZipFile_PublishLumpsToDirectory((ZipFile*)fsObject, zipLumpDirectory);
        break;
    case FT_WADFILE:
        WadFile_PublishLumpsToDirectory((WadFile*)fsObject, ActiveWadLumpDirectory);
        break;
    /*case FT_LUMPFILE:
        LumpFile_PublishLumpsToDirectory((LumpFile*)fsObject, ActiveWadLumpDirectory);
        break;*/
    case FT_LUMPFILE: {
        const LumpInfo* info = AbstractFile_Info(fsObject);
        LumpDirectory_CatalogLumps(ActiveWadLumpDirectory, info->container, info->lumpIdx, 1);
        break;
      }
    default: break;
        /*Con_Error("F_AddFile: Invalid file type %i.", (int) AbstractFile_Type(fsObject));
        exit(1); // Unreachable.*/
    }
    return file;
}

boolean F_AddFiles(const char* const* paths, size_t num, boolean allowDuplicate)
{
    boolean succeeded = false;
    size_t i;

    for(i = 0; i < num; ++i)
    {
        if(F_AddFile(paths[i], 0, allowDuplicate))
        {
            VERBOSE2( Con_Message("Done loading %s\n", F_PrettyPath(paths[i])) );
            succeeded = true; // At least one has been loaded.
        }
        else
        {
            Con_Message("Warning: Errors occured while loading %s\n", paths[i]);
        }
    }

    // A changed file list may alter the main lump directory.
    if(succeeded)
    {
        DD_UpdateEngineState();
    }
    return succeeded;
}

boolean F_RemoveFile(const char* path, boolean permitRequired)
{
    boolean unloadedResources = unloadFile(path, permitRequired);
    if(unloadedResources)
        DD_UpdateEngineState();
    return unloadedResources;
}

boolean F_RemoveFiles(const char* const* filenames, size_t num, boolean permitRequired)
{
    boolean succeeded = false;
    size_t i;

    for(i = 0; i < num; ++i)
    {
        if(unloadFile(filenames[i], permitRequired))
        {
            VERBOSE2( Con_Message("Done unloading %s\n", F_PrettyPath(filenames[i])) )
            succeeded = true; // At least one has been unloaded.
        }
    }

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
        ddstring_t* path = F_ComposeLumpPath(container, lumpIdx);
        abstractfile_t* fsObject = newLumpFile(
            DFileBuilder_NewFromAbstractFileLump(container, lumpIdx, false),
            Str_Text(path), F_LumpInfo(container, lumpIdx));

        Str_Delete(path);
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
    F_FixSlashes(&fullPath, &fullPath);
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
    const char* ptr = buffer, *end;
    size_t len;
    assert(lumpName && NULL != path);

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

/**
 * LUMPNAM0 \Path\In\The\Base.ext
 * LUMPNAM1 Path\In\The\RuntimeDir.ext
 *  :
 */
static boolean parseLDMappingList(const char* buffer)
{
    boolean successful = false;
    lumpname_t lumpName;
    ddstring_t path, line;
    const char* ch;
    assert(buffer);

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

void F_InitLumpDirectoryMappings(void)
{
    static boolean inited = false;
    size_t bufSize = 0;
    uint8_t* buf = NULL;
    lumpnum_t i;

    if(inited)
    {   // Free old paths, if any.
        clearLDMappings();
    }

    if(DD_IsShuttingDown()) return;

    // Add the contents of all DD_DIREC lumps.
    for(i = 0; i < F_LumpCount(); ++i)
    {
        const LumpInfo* info;
        abstractfile_t* fsObject;
        size_t lumpLength;
        int lumpIdx;

        if(strnicmp(F_LumpName(i), "DD_DIREC", 8)) continue;

        // Make a copy of it so we can ensure it ends in a null.
        info = F_FindInfoForLumpNum(i);
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
    }

    if(buf) free(buf);
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

/// @return  @c true iff the mapping matched the path.
static boolean applyVDMapping(ddstring_t* path, vdmapping_t* vdm)
{
    if(path && vdm && !strnicmp(Str_Text(path), Str_Text(&vdm->destination), Str_Length(&vdm->destination)))
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
    Str_Init(&src); Str_Set(&src, source);
    Str_Strip(&src);
    F_FixSlashes(&src, &src);
    F_ExpandBasePath(&src, &src);
    F_PrependWorkPath(&src, &src);
    F_AppendMissingSlash(&src);

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

        Str_Init(&vdm->source); Str_Set(&vdm->source, Str_Text(&src));
        Str_Init(&vdm->destination);
        Str_Free(&src);
    }

    // Set the destination.
    Str_Set(&vdm->destination, destination);
    Str_Strip(&vdm->destination);
    F_FixSlashes(&vdm->destination, &vdm->destination);
    F_ExpandBasePath(&vdm->destination, &vdm->destination);
    F_PrependWorkPath(&vdm->destination, &vdm->destination);
    F_AppendMissingSlash(&vdm->destination);

    VERBOSE(Con_Message("Resources in \"%s\" now mapped to \"%s\"\n", F_PrettyPath(Str_Text(&vdm->source)), F_PrettyPath(Str_Text(&vdm->destination))) );
}

void F_InitVirtualDirectoryMappings(void)
{
    int i, argC = Argc();

    clearVDMappings();

    if(DD_IsShuttingDown()) return;

    // Create virtual directory mappings by processing all -vdmap options.
    for(i = 0; i < argC; ++i)
    {
        if(strnicmp("-vdmap", Argv(i), 6))
            continue; // This is not the option we're looking for.

        if(i < argC - 1 && !ArgIsOption(i + 1) && !ArgIsOption(i + 2))
        {
            F_AddVirtualDirectoryMapping(Argv(i + 1), Argv(i + 2));
            i += 2;
        }
    }
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
int printResourcePath(const ddstring_t* fileNameStr, pathdirectorynode_type_t type,
    void* paramaters)
{
    //assert(fileNameStr && VALID_PATHDIRECTORYNODE_TYPE(type));
    const char* fileName = Str_Text(fileNameStr);
    boolean makePretty = F_IsRelativeToBase(fileName, ddBasePath);
    Con_Printf("  %s\n", makePretty? F_PrettyPath(fileName) : fileName);
    return 0; // Continue the listing.
}

static void printVFDirectory(const ddstring_t* path)
{
    ddstring_t dir;

    Str_Init(&dir); Str_Set(&dir, Str_Text(path));
    Str_Strip(&dir);
    // Make sure it ends in a directory separator character.
    F_AppendMissingSlash(&dir);
    if(!F_ExpandBasePath(&dir, &dir))
        F_PrependBasePath(&dir, &dir);

    Con_Printf("Directory: %s\n", F_PrettyPath(Str_Text(&dir)));

    // Make the pattern.
    Str_AppendChar(&dir, '*');
    F_AllResourcePaths(Str_Text(&dir), 0, printResourcePath);

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
            return F_DumpLump(absoluteLumpNum, NULL);
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
                fileCount = ZipFile_LumpCount((ZipFile*)*ptr);
                crc = 0;
                break;
            case FT_WADFILE: {
                WadFile* wad = (WadFile*)*ptr;
                crc = (!AbstractFile_HasCustom(*ptr)? WadFile_CalculateCRC(wad) : 0);
                fileCount = WadFile_LumpCount(wad);
                break;
              }
            case FT_LUMPFILE:
                fileCount = LumpFile_LumpCount((LumpFile*)*ptr);
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
