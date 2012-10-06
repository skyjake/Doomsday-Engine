/**
 * @file fs_main.cpp
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @author Copyright &copy; 1993-1996 by id Software, Inc.
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

#include <cstdlib>
#include <cctype>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "game.h"
#include "genericfile.h"
#include "lumpinfo.h"
#include "lumpdirectory.h"
#include "lumpfile.h"
#include "m_md5.h"
#include "m_misc.h" // for M_FindWhite()
#include "wadfile.h"
#include "zipfile.h"

#include <de/Log>
#include <de/memory.h>

D_CMD(Dir);
D_CMD(DumpLump);
D_CMD(ListFiles);
D_CMD(ListLumps);

using de::FS;
using de::AbstractFile;     using de::DFile;
using de::DFileBuilder;     using de::GenericFile;
using de::LumpDirectory;    using de::LumpFile;
using de::PathDirectory;    using de::PathDirectoryNode;
using de::WadFile;          using de::ZipFile;

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
#define FILEIDENTIFIERID_T_MAXLEN       16
#define FILEIDENTIFIERID_T_LASTINDEX    15
typedef byte fileidentifierid_t[FILEIDENTIFIERID_T_MAXLEN];

typedef struct fileidentifier_s
{
    fileidentifierid_t hash;
} fileidentifier_t;

static bool inited = false;
static bool loadingForStartup;

static FileList* openFiles;
static FileList* loadedFiles;

static uint fileIdentifiersCount;
static uint fileIdentifiersMax;
static fileidentifier_t* fileIdentifiers;

static LumpDirectory* zipLumpDirectory;

static LumpDirectory* primaryWadLumpDirectory;
static LumpDirectory* auxiliaryWadLumpDirectory;
// @c true = one or more files have been opened using the auxiliary directory.
static bool auxiliaryWadLumpDirectoryInUse;

// Currently selected lump directory.
static LumpDirectory* ActiveWadLumpDirectory;

// Lump directory mappings.
static uint ldMappingsCount;
static uint ldMappingsMax;
static ldmapping_t* ldMappings;

// Virtual(-File) directory mappings.
static uint vdMappingsCount;
static uint vdMappingsMax;
static vdmapping_t* vdMappings;

static void clearLDMappings();
static void clearVDMappings();
static bool applyVDMapping(ddstring_t* path, vdmapping_t* vdm);

static FILE* findRealFile(char const* path, char const* mymode, ddstring_t** foundPath);

/// @return  @c true if the FileId associated with @a path was released.
static bool releaseFileId(char const* path);

/**
 * Calculate an identifier for the file based on its full path name.
 * The identifier is the MD5 hash of the path.
 */
static void generateFileId(char const* str, byte identifier[16]);

static void printFileId(byte identifier[16]);

void F_Register(void)
{
    C_CMD("dir", "", Dir);
    C_CMD("ls", "", Dir); // Alias
    C_CMD("dir", "s*", Dir);
    C_CMD("ls", "s*", Dir); // Alias

    C_CMD("dump", "s", DumpLump);
    C_CMD("listfiles", "", ListFiles);
    C_CMD("listlumps", "", ListLumps);
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
    fileidentifier_t* found = 0;
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

/**
 * @note Performance is O(n).
 * @return @c iterator pointing to list->end() if not found.
 */
static FileList::iterator findListFileByPath(FileList* list, char const* path_)
{
    if(!list || list->empty()) return list->end();
    if(!path_ || !path_[0]) return list->end();

    // Transform the path into one we can process.
    AutoStr* path = Str_Set(AutoStr_NewStd(), path_);
    F_FixSlashes(path, path);

    // Perform the search.
    FileList::iterator i;
    for(i = list->begin(); i != list->end(); ++i)
    {
        AbstractFile* file = (*i)->file();
        if(!Str_CompareIgnoreCase(file->path(), Str_Text(path)))
        {
            break; // This is the node we are looking for.
        }
    }
    return i;
}

static int pruneLumpsFromDirectorysByFile(AbstractFile& file)
{
    int pruned = zipLumpDirectory->pruneByFile(file)
               + primaryWadLumpDirectory->pruneByFile(file);
    if(auxiliaryWadLumpDirectoryInUse)
        pruned += auxiliaryWadLumpDirectory->pruneByFile(file);
    return pruned;
}

static void unlinkFile(AbstractFile* file)
{
    if(!file) return;
    releaseFileId(Str_Text(file->path()));
    pruneLumpsFromDirectorysByFile(*file);
}

static void clearLoadedFiles(LumpDirectory* directory = 0)
{
    if(!loadedFiles) return;

    // Unload in reverse load order.
    for(int i = loadedFiles->size() - 1; i >= 0; i--)
    {
        DFile* hndl = (*loadedFiles)[i];
        if(!directory || directory->catalogues(*hndl->file()))
        {
            unlinkFile(hndl->file());
            loadedFiles->removeAt(i);
            FS::deleteFile(hndl);
        }
    }
}

/**
 * Handles conversion to a logical index that is independent of the lump directory currently in use.
 */
static inline lumpnum_t logicalLumpNum(lumpnum_t lumpNum)
{
    return (lumpNum < 0 ? -1 :
            ActiveWadLumpDirectory == auxiliaryWadLumpDirectory? lumpNum += AUXILIARY_BASE : lumpNum);
}

static void usePrimaryWadLumpDirectory()
{
    ActiveWadLumpDirectory = primaryWadLumpDirectory;
}

static bool useAuxiliaryWadLumpDirectory()
{
    if(!auxiliaryWadLumpDirectoryInUse) return false;
    ActiveWadLumpDirectory = auxiliaryWadLumpDirectory;
    return true;
}

static void clearLumpDirectorys()
{
    delete primaryWadLumpDirectory; primaryWadLumpDirectory = 0;
    delete auxiliaryWadLumpDirectory; auxiliaryWadLumpDirectory = 0;
    delete zipLumpDirectory; zipLumpDirectory = 0;

    ActiveWadLumpDirectory = 0;
}

static void clearOpenFiles()
{
    while(!openFiles->empty())
    { FS::deleteFile(openFiles->back()); }
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

static bool unloadFile(char const* path, bool permitRequired = false, bool quiet = false)
{
    errorIfNotInited("unloadFile2");

    if(!loadedFiles) return false;
    FileList::iterator found = findListFileByPath(loadedFiles, path);
    if(found == loadedFiles->end()) return false;

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

    DFile* hndl = *found;
    unlinkFile(hndl->file());
    loadedFiles->erase(found);
    FS::deleteFile(hndl);
    return true;
}

static void clearFileIds()
{
    if(fileIdentifiers)
    {
        M_Free(fileIdentifiers); fileIdentifiers = 0;
    }
    if(fileIdentifiersCount)
    {
        fileIdentifiersCount = fileIdentifiersMax = 0;
    }
}

static void printFileId(byte identifier[16])
{
    if(!identifier) return;
    for(uint i = 0; i < 16; ++i)
    {
        Con_Printf("%02x", identifier[i]);
    }
}

static void generateFileId(char const* str, byte identifier[16])
{
    // First normalize the name.
    ddstring_t absPath; Str_Init(&absPath);
    Str_Set(&absPath, str);
    F_MakeAbsolute(&absPath, &absPath);
    F_FixSlashes(&absPath, &absPath);

#if defined(WIN32) || defined(MACOSX)
    // This is a case insensitive operation.
    strupr(Str_Text(&absPath));
#endif

    md5_ctx_t context; md5_init(&context);
    md5_update(&context, (byte*) Str_Text(&absPath), (unsigned int) Str_Length(&absPath));
    md5_final(&context, identifier);

    Str_Free(&absPath);
}

boolean F_CheckFileId(char const* path)
{
    DENG_ASSERT(path);

    if(!F_Access(path)) return false;

    // Calculate the identifier.
    fileidentifierid_t id;
    generateFileId(path, id);

    if(findFileIdentifierForId(id)) return false;

    // Allocate a new entry.
    fileIdentifiersCount++;
    if(fileIdentifiersCount > fileIdentifiersMax)
    {
        if(!fileIdentifiersMax)
            fileIdentifiersMax = 16;
        else
            fileIdentifiersMax *= 2;

        fileIdentifiers = (fileidentifier_t*) M_Realloc(fileIdentifiers, fileIdentifiersMax * sizeof *fileIdentifiers);
        if(!fileIdentifiers) Con_Error("F_CheckFileId: Failed on (re)allocation of %lu bytes while enlarging fileIdentifiers.", (unsigned long) (fileIdentifiersMax * sizeof *fileIdentifiers));

        memset(fileIdentifiers + fileIdentifiersCount, 0, (fileIdentifiersMax - fileIdentifiersCount) * sizeof *fileIdentifiers);
    }

#if _DEBUG
    VERBOSE(
        Con_Printf("Added file identifier ");
        printFileId(id);
        Con_Printf(" - \"%s\"\n", F_PrettyPath(path)) )
#endif

    memcpy(fileIdentifiers[fileIdentifiersCount - 1].hash, id, sizeof(id));
    return true;
}

static bool releaseFileId(char const* path)
{
    if(path && path[0])
    {
        fileidentifierid_t id;
        generateFileId(path, id);

        fileidentifier_t* fileIdentifier = findFileIdentifierForId(id);
        if(fileIdentifier)
        {
            size_t index = fileIdentifier - fileIdentifiers;
            if(index < fileIdentifiersCount-1)
            {
                memmove(fileIdentifiers + index, fileIdentifiers + index + 1, fileIdentifiersCount - index - 1);
            }
            memset(fileIdentifiers + fileIdentifiersCount, 0, sizeof *fileIdentifiers);
            --fileIdentifiersCount;

#if _DEBUG
            VERBOSE(
                Con_Printf("Released file identifier ");
                printFileId(id);
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
    DENG_ASSERT(info);
    info->lumpIdx = 0;
    info->size = 0;
    info->compressedSize = 0;
    info->lastModified = 0;
    info->container = NULL;
}

void F_CopyLumpInfo(LumpInfo* dst, LumpInfo const* src)
{
    DENG_ASSERT(dst && src);
    dst->lumpIdx = src->lumpIdx;
    dst->size = src->size;
    dst->compressedSize = src->compressedSize;
    dst->lastModified = src->lastModified;
    dst->container = src->container;
}

void F_DestroyLumpInfo(LumpInfo* /*info*/)
{
    // Nothing to do.
}

void F_Init(void)
{
    if(inited) return; // Already been here.

    // This'll force the loader NOT to flag new files as "runtime".
    loadingForStartup = true;

    DFileBuilder::init();

    openFiles     = new FileList();
    loadedFiles   = new FileList();

    zipLumpDirectory          = new LumpDirectory(LDF_UNIQUE_PATHS);
    primaryWadLumpDirectory   = new LumpDirectory();
    auxiliaryWadLumpDirectory = new LumpDirectory();
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

    clearLoadedFiles();
    delete loadedFiles; loadedFiles = 0;
    clearOpenFiles();
    delete openFiles; openFiles = 0;

    clearFileIds(); // Should be null-op if bookkeeping is correct.
    clearLumpDirectorys();

    DFileBuilder::shutdown();

    inited = false;
}

void F_EndStartup(void)
{
    errorIfNotInited("F_EndStartup");
    loadingForStartup = false;
    usePrimaryWadLumpDirectory();
}

static int unloadListFiles(FileList* list, bool nonStartup)
{
    int unloaded = 0;
    if(list)
    {
        // Unload in reverse load order.
        for(int i = list->size() - 1; i >= 0; i--)
        {
            DFile* hndl = (*list)[i];
            AbstractFile* file = hndl->file();
            if(!nonStartup || !file->hasStartup())
            {
                if(unloadFile(Str_Text(file->path()),
                              true/*allow unloading game resources*/, true/*quiet please*/))
                {
                    ++unloaded;
                }
            }
        }
    }
    return unloaded;
}

#if _DEBUG
static void logOrphanedFileIdentifiers()
{
    fileidentifierid_t nullId;
    memset(nullId, 0, sizeof nullId);

    uint orphanCount = 0;
    for(uint i = 0; i < fileIdentifiersCount; ++i)
    {
        fileidentifier_t* id = fileIdentifiers + i;
        if(!memcmp(id->hash, &nullId, FILEIDENTIFIERID_T_LASTINDEX)) continue;

        if(!orphanCount)
        {
            Con_Printf("Warning: Orphan file identifiers:\n");
        }

        Con_Printf("  %u - ", orphanCount);
        printFileId(id->hash);
        Con_Printf("\n");

        orphanCount++;
    }
}
#endif

#if _DEBUG
static void printFileList(FileList* list)
{
    byte id[16];
    if(!list) return;
    for(int i = 0; i < list->size(); ++i)
    {
        DFile* hndl = (*list)[i];
        AbstractFile* file = hndl->file();

        Con_Printf(" %c%u: ", file->hasStartup()? '*':' ', i);
        generateFileId(Str_Text(file->path()), id);
        printFileId(id);
        Con_Printf(" - \"%s\" [handle: %p]\n", F_PrettyPath(Str_Text(file->path())), (void*)&hndl);
    }
}
#endif

int F_Reset(void)
{
    if(!inited) return 0;

#if _DEBUG
    // List all open files with their identifiers.
    VERBOSE(
        Con_Printf("Open files at reset:\n");
        printFileList(openFiles);
        Con_Printf("End\n")
    )
#endif

    // Perform non-startup file unloading...
    int unloaded = unloadListFiles(loadedFiles, true/*non-startup*/);

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
    errorIfNotInited("F_IsValidLumpNum");
    lumpnum_t lumpNum = chooseWadLumpDirectory(absoluteLumpNum);
    return ActiveWadLumpDirectory->isValidIndex(lumpNum);
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
    DENG_ASSERT(pCond != 0);
    DENG_ASSERT(pSize != 0);

    *pCond = LSCOND_NONE;
    *pSize = 0;

    int i, argPos = 0;
    int len = Str_Length(name);
    char const* txt = Str_Text(name);
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

lumpnum_t F_CheckLumpNumForName2(char const* name, boolean silent)
{
    errorIfNotInited("F_CheckLumpNumForName");

    lumpnum_t lumpNum = -1;

    if(name && name[0])
    {
        lumpsizecondition_t sizeCond;
        size_t lumpSize = 0, refSize;
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
            lumpNum = ActiveWadLumpDirectory->indexForPath(Str_Text(&searchPath));

            if(lumpNum >= 0 && sizeCond != LSCOND_NONE)
            {
                // Get the size as well for the condition check.
                lumpSize = ActiveWadLumpDirectory->lumpInfo(lumpNum)->size;
            }
        }

        // Found it yet?
        if(lumpNum < 0)
        {
            usePrimaryWadLumpDirectory();
            lumpNum = ActiveWadLumpDirectory->indexForPath(Str_Text(&searchPath));

            if(lumpNum >= 0 && sizeCond != LSCOND_NONE)
            {
                // Get the size as well for the condition check.
                lumpSize = ActiveWadLumpDirectory->lumpInfo(lumpNum)->size;
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

lumpnum_t F_CheckLumpNumForName(char const* name)
{
    return F_CheckLumpNumForName2(name, false);
}

LumpInfo const* F_FindInfoForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx)
{
    lumpnum_t translated = chooseWadLumpDirectory(absoluteLumpNum);
    LumpInfo const* lumpInfo = ActiveWadLumpDirectory->lumpInfo(translated);
    if(lumpIdx) *lumpIdx = (lumpInfo? lumpInfo->lumpIdx : -1);
    return lumpInfo;
}

LumpInfo const* F_FindInfoForLumpNum(lumpnum_t absoluteLumpNum)
{
    return F_FindInfoForLumpNum2(absoluteLumpNum, NULL);
}

char const* F_LumpName(lumpnum_t absoluteLumpNum)
{
    LumpInfo const* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(info)
    {
        AbstractFile* container = reinterpret_cast<AbstractFile*>(info->container);
        PathDirectoryNode* node = container->lumpDirectoryNode(info->lumpIdx);
        return Str_Text(node->pathFragment());
    }
    return "";
}

size_t F_LumpLength(lumpnum_t absoluteLumpNum)
{
    LumpInfo const* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(info) return info->size;
    return 0;
}

uint F_LumpLastModified(lumpnum_t absoluteLumpNum)
{
    LumpInfo const* info = F_FindInfoForLumpNum(absoluteLumpNum);
    if(info) return info->lastModified;
    return 0;
}

AbstractFile* FS::findFileForLumpNum(lumpnum_t absoluteLumpNum, int* lumpIdx)
{
    LumpInfo const* lumpInfo = F_FindInfoForLumpNum2(absoluteLumpNum, lumpIdx);
    if(!lumpInfo) return NULL;
    return reinterpret_cast<AbstractFile*>(lumpInfo->container);
}

char const* F_LumpSourceFile(lumpnum_t absoluteLumpNum)
{
    AbstractFile* file = FS::findFileForLumpNum(absoluteLumpNum);
    if(file) return Str_Text(file->path());
    return "";
}

boolean F_LumpIsCustom(lumpnum_t absoluteLumpNum)
{
    AbstractFile* file = FS::findFileForLumpNum(absoluteLumpNum);
    if(file) return file->hasCustom();
    return false;
}

int F_LumpCount(void)
{
    if(inited) return ActiveWadLumpDirectory->size();
    return 0;
}

lumpnum_t F_OpenAuxiliary3(char const* path, size_t baseOffset, boolean silent)
{
    errorIfNotInited("F_OpenAuxiliary");

    if(!path || !path[0]) return -1;

    // Make it a full path.
    ddstring_t searchPath;
    Str_Init(&searchPath); Str_Set(&searchPath, path);
    F_ExpandBasePath(&searchPath, &searchPath);
    // We must have an absolute path, so prepend the current working directory if necessary.
    F_PrependWorkPath(&searchPath, &searchPath);

    /// @todo Allow opening WAD/ZIP files from lumps in other containers.
    ddstring_t* foundPath = 0;
    FILE* file = findRealFile(Str_Text(&searchPath), "rb", &foundPath);
    Str_Free(&searchPath);
    if(!file)
    {
        if(foundPath)
        {
            Str_Delete(foundPath);
        }
        if(!silent)
        {
            Con_Message("Warning: F_OpenAuxiliary: Resource \"%s\" not found, aborting.\n", path);
        }
        return -1;
    }

    DFile* hndl = DFileBuilder::fromNativeFile(*file, baseOffset);

    if(hndl && WadFile::recognise(*hndl))
    {
        if(auxiliaryWadLumpDirectoryInUse)
        {
            F_CloseAuxiliary();
        }
        ActiveWadLumpDirectory = auxiliaryWadLumpDirectory;
        auxiliaryWadLumpDirectoryInUse = true;

        // Prepare the temporary info descriptor.
        LumpInfo info; F_InitLumpInfo(&info);
        info.lastModified = F_LastModified(Str_Text(foundPath));

        WadFile* wad = new WadFile(*hndl, Str_Text(foundPath), info);
        hndl = DFileBuilder::fromFile(*wad);
        openFiles->push_back(hndl); hndl->setList(reinterpret_cast<struct filelist_s*>(openFiles));

        DFile* loadedFilesHndl = DFileBuilder::dup(*hndl);
        loadedFiles->push_back(loadedFilesHndl); loadedFilesHndl->setList(reinterpret_cast<struct filelist_s*>(loadedFiles));
        wad->publishLumpsToDirectory(ActiveWadLumpDirectory);

        Str_Delete(foundPath);

        // We're done with the descriptor.
        F_DestroyLumpInfo(&info);
        return AUXILIARY_BASE;
    }

    if(!silent)
    {
        if(path && path[0])
            Con_Message("Warning: F_OpenAuxiliary: Resource \"%s\" cannot be found or does not appear to be of recognised format.\n", path);
        else
            Con_Message("Warning: F_OpenAuxiliary: Cannot open a resource with neither a path nor a handle to it.\n");
    }

    if(foundPath) Str_Delete(foundPath);
    if(hndl) delete hndl;
    return -1;
}

lumpnum_t F_OpenAuxiliary2(char const* path, size_t baseOffset)
{
    return F_OpenAuxiliary3(path, baseOffset, false);
}

lumpnum_t F_OpenAuxiliary(char const* path)
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

void FS::releaseFile(AbstractFile* file)
{
    if(!file || !openFiles) return;
    for(int i = openFiles->size() - 1; i >= 0; i--)
    {
        DFile* hndl = (*openFiles)[i];
        if(hndl->file() == file)
        {
            openFiles->removeAt(i);
        }
    }
}

void FS::closeFile(DFile* hndl)
{
    if(!hndl) return;
    hndl->close();
}

void FS::deleteFile(DFile* hndl)
{
    if(!hndl) return;
    hndl->close();
    delete hndl->file();
    delete hndl;
}

boolean F_Dump(void const* data, size_t size, char const* path)
{
    DENG_ASSERT(data);
    DENG_ASSERT(path);

    if(!size) return false;

    AutoStr* nativePath = AutoStr_NewStd();
    Str_Set(nativePath, path);
    F_ToNativeSlashes(nativePath, nativePath);

    FILE* outFile = fopen(Str_Text(nativePath), "wb");
    if(!outFile)
    {
        Con_Message("Warning: Failed to open \"%s\" for writing (error: %s), aborting.\n",
                    F_PrettyPath(Str_Text(nativePath)), strerror(errno));
        return false;
    }

    fwrite(data, 1, size, outFile);
    fclose(outFile);
    return true;
}

boolean F_DumpLump(lumpnum_t absoluteLumpNum, char const* path)
{
    int lumpIdx;
    AbstractFile* file = FS::findFileForLumpNum(absoluteLumpNum, &lumpIdx);
    if(!file) return false;

    LumpInfo const* info = file->lumpInfo(lumpIdx);
    if(!info) return false;

    char const* lumpName = F_LumpName(absoluteLumpNum);
    char const* fname;
    if(path && path[0])
    {
        fname = path;
    }
    else
    {
        fname = lumpName;
    }

    bool dumpedOk = F_Dump(file->cacheLump(lumpIdx), info->size, fname);
    file->unlockLump(lumpIdx);
    if(!dumpedOk) return false;

    LOG_VERBOSE("%s dumped to \"%s\"") << lumpName << F_PrettyPath(fname);
    return true;
}

/// @return @c NULL= Not found.
static WadFile* findFirstWadFile(FileList* list, bool custom)
{
    if(!list || list->empty()) return 0;
    DENG2_FOR_EACH(i, *list, FileList::iterator)
    {
        AbstractFile* file = (*i)->file();
        if(custom != file->hasCustom()) continue;

        WadFile* wad = dynamic_cast<WadFile*>(file);
        if(wad) return wad;
    }
    return 0;
}

uint F_CRCNumber(void)
{
    errorIfNotInited("F_CRCNumber");
    if(!loadedFiles) return 0;

    /**
     * We define the CRC as that of the lump directory of the first loaded IWAD.
     * @todo Really kludgy...
     */
    WadFile* iwad = findFirstWadFile(loadedFiles, false/*not-custom*/);
    if(!iwad) return 0;
    return iwad->calculateCRC();
}

/**
 * @defgroup pathToStringFlags  Path To String Flags.
 */
///@{
#define PTSF_QUOTED                 0x1 ///< Add double quotes around the path.
#define PTSF_TRANSFORM_EXCLUDE_DIR  0x2 ///< Exclude the directory; e.g., c:/doom/myaddon.wad -> myaddon.wad
#define PTSF_TRANSFORM_EXCLUDE_EXT  0x4 ///< Exclude the extension; e.g., c:/doom/myaddon.wad -> c:/doom/myaddon
///@}

#define DEFAULT_PATHTOSTRINGFLAGS (PTSF_QUOTED)

/**
 * @param flags         @ref pathToStringFlags
 * @param delimiter     If not @c NULL, path fragments in the resultant string
 *                      will be delimited by this.
 * @param predicate     If not @c NULL, this predicate evaluator callback must
 *                      return @c true for a given path to be included in the
 *                      resultant string.
 * @param parameters    Passed to the predicate evaluator callback.
 *
 * @return  New string containing a concatenated, possibly delimited set of all
 *      file paths in the list. Ownership of the string passes to the caller who
 *      should ensure to release it with Str_Delete() when no longer needed.
 *      Always returns a valid (but perhaps zero-length) string object.
 */
static ddstring_t* composeFileList(FileList* fl, int flags = DEFAULT_PATHTOSTRINGFLAGS,
    char const* delimiter = " ", bool (*predicate)(DFile* hndl, void* parameters) = 0, void* parameters = 0)
{
    DENG_ASSERT(fl);

    int maxLength, delimiterLength = (delimiter? (int)strlen(delimiter) : 0);
    int n, pLength, pathCount = 0;
    ddstring_t const* path;
    ddstring_t* str, buf;
    char const* p, *ext;

    // Do we need a working buffer to process this request?
    /// @todo We can do this without the buffer; implement cleverer algorithm.
    if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
        Str_Init(&buf);

    // Determine the maximum number of characters we'll need.
    maxLength = 0;
    DENG2_FOR_EACH(i, *fl, FileList::const_iterator)
    {
        if(predicate && !predicate(*i, parameters))
            continue; // Caller isn't interested in this...

        // One more path will be composited.
        ++pathCount;
        path = (*i)->file()->path();

        if(!(flags & (PTSF_TRANSFORM_EXCLUDE_DIR|PTSF_TRANSFORM_EXCLUDE_EXT)))
        {
            // Caller wants the whole path plus name and extension (if present).
            maxLength += Str_Length(path);
        }
        else
        {
            if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
            {
                // Caller does not want the directory hierarchy.
                F_FileNameAndExtension(&buf, Str_Text(path));
                p = Str_Text(&buf);
                pLength = Str_Length(&buf);
            }
            else
            {
                p = Str_Text(path);
                pLength = Str_Length(path);
            }

            if(flags & PTSF_TRANSFORM_EXCLUDE_EXT)
            {
                // Caller does not want the file extension.
                ext = F_FindFileExtension(p);
                if(ext) ext -= 1/*dot separator*/;
            }
            else
            {
                ext = NULL;
            }

            if(ext)
            {
                maxLength += pLength - (pLength - (ext - p));
            }
            else
            {
                maxLength += pLength;
            }
        }

        if(flags & PTSF_QUOTED)
            maxLength += sizeof(char);
    }
    maxLength += pathCount * delimiterLength;

    // Composite final string.
    str = Str_New();
    Str_Reserve(str, maxLength);
    n = 0;
    DENG2_FOR_EACH(i, *fl, FileList::const_iterator)
    {
        if(predicate && !predicate(*i, parameters))
            continue; // Caller isn't interested in this...

        path = (*i)->file()->path();

        if(flags & PTSF_QUOTED)
            Str_AppendChar(str, '"');

        if(!(flags & (PTSF_TRANSFORM_EXCLUDE_DIR|PTSF_TRANSFORM_EXCLUDE_EXT)))
        {
            // Caller wants the whole path plus name and extension (if present).
            Str_Append(str, Str_Text(path));
        }
        else
        {
            if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
            {
                // Caller does not want the directory hierarchy.
                F_FileNameAndExtension(&buf, Str_Text(path));
                p = Str_Text(&buf);
                pLength = Str_Length(&buf);
            }
            else
            {
                p = Str_Text(path);
                pLength = Str_Length(path);
            }

            if(flags & PTSF_TRANSFORM_EXCLUDE_EXT)
            {
                // Caller does not want the file extension.
                ext = F_FindFileExtension(p);
                if(ext) ext -= 1/*dot separator*/;
            }
            else
            {
                ext = NULL;
            }

            if(ext)
            {
                Str_PartAppend(str, p, 0, pLength - (pLength - (ext - p)));
                maxLength += pLength - (pLength - (ext - p));
            }
            else
            {
                Str_Append(str, p);
            }
        }

        if(flags & PTSF_QUOTED)
            Str_AppendChar(str, '"');

        ++n;
        if(n != pathCount)
            Str_Append(str, delimiter);
    }

    if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
        Str_Free(&buf);

    return str;
}

typedef struct {
    filetype_t type; // Only
    bool includeOriginal;
    bool includeCustom;
} compositepathpredicateparamaters_t;

bool compositePathPredicate(DFile* hndl, void* parameters)
{
    DENG_ASSERT(parameters);
    compositepathpredicateparamaters_t* p = (compositepathpredicateparamaters_t*)parameters;
    AbstractFile* file = hndl->file();
    if((!VALID_FILETYPE(p->type) || p->type == file->type()) &&
       ((p->includeOriginal && !file->hasCustom()) ||
        (p->includeCustom   &&  file->hasCustom())))
    {
        ddstring_t const* path = file->path();
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
        ddstring_t* str = composeFileList(loadedFiles, PTSF_TRANSFORM_EXCLUDE_DIR,
                                          delimiter, compositePathPredicate, (void*)&p);
        strncpy(outBuf, Str_Text(str), outBufSize);
        Str_Delete(str);
    }
}

struct PathListItem
{
    de::String path;
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
static PathList collectLocalPaths(Str const* searchPath, bool includeSearchPath)
{
    PathList foundEntries;
    finddata_t fd;

    Str origWildPath; Str_InitStd(&origWildPath);
    Str_Appendf(&origWildPath, "%s*", Str_Text(searchPath));

    ddstring_t wildPath; Str_Init(&wildPath);
    for(int i = -1; i < (int)vdMappingsCount; ++i)
    {
        if(i == -1)
        {
            Str_Copy(&wildPath, &origWildPath);
        }
        else
        {
            // Possible mapping?
            Str_Copy(&wildPath, searchPath);
            if(!applyVDMapping(&wildPath, &vdMappings[i])) continue;

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
                    QString foundPath = QString("%1%2").arg((includeSearchPath ? Str_Text(searchPath) : "")).arg(Str_Text(&fd.name));
                    foundEntries.push_back(PathListItem(foundPath, fd.attrib));
                }
            } while(!myfindnext(&fd));
        }
        myfindend(&fd);
    }

    // Sort all the foundPaths entries.
    qSort(foundEntries.begin(), foundEntries.end());

    Str_Free(&origWildPath);
    Str_Free(&wildPath);

    return foundEntries;
}

int F_AllResourcePaths2(char const* rawSearchPattern, int flags,
    int (*callback) (char const* path, PathDirectoryNodeType type, void* parameters),
    void* parameters)
{
    // First normalize the raw search pattern into one we can process.
    AutoStr* searchPattern = AutoStr_NewStd();
    Str_Set(searchPattern, rawSearchPattern);
    Str_Strip(searchPattern);
    F_FixSlashes(searchPattern, searchPattern);
    F_ExpandBasePath(searchPattern, searchPattern);

    // An absolute path is required so resolve relative to the base path.
    // if not already absolute.
    F_PrependBasePath(searchPattern, searchPattern);

    PathMap patternMap;
    PathMap_Initialize(&patternMap, PathDirectory::hashPathFragment, Str_Text(searchPattern));

    // Check the Zip directory.
    {
    int result = 0;
    DENG2_FOR_EACH(i, zipLumpDirectory->lumps(), LumpDirectory::LumpInfos::const_iterator)
    {
        LumpInfo const* lumpInfo = *i;
        AbstractFile* container = reinterpret_cast<AbstractFile*>(lumpInfo->container);
        DENG_ASSERT(container);
        PathDirectoryNode* node = container->lumpDirectoryNode(lumpInfo->lumpIdx);
        DENG_ASSERT(node);

        AutoStr* filePath = 0;
        bool patternMatched;
        if(!(flags & SPF_NO_DESCEND))
        {
            filePath       = container->composeLumpPath(lumpInfo->lumpIdx);
            patternMatched = F_MatchFileName(Str_Text(filePath), Str_Text(searchPattern));
        }
        else
        {
            patternMatched = node->matchDirectory(PCF_MATCH_FULL, &patternMap);
            if(patternMatched)
            {
                filePath   = container->composeLumpPath(lumpInfo->lumpIdx);
            }
        }

        if(!patternMatched) continue;

        result = callback(Str_Text(filePath), PT_LEAF, parameters);
        if(result) break;
    }

    PathMap_Destroy(&patternMap);
    if(result) return result;
    }

    // Check the dir/WAD direcs.
    if(ldMappingsCount)
    {
        for(uint i = 0; i < ldMappingsCount; ++i)
        {
            ldmapping_t* rec = &ldMappings[i];
            bool patternMatched = F_MatchFileName(Str_Text(&rec->path), Str_Text(searchPattern));

            if(!patternMatched) continue;

            int result = callback(Str_Text(&rec->path), PT_LEAF, parameters);
            if(result) return result;
        }
    }

    /**
     * Check real files on the search path.
     * Our existing normalized search pattern cannot be used as-is due to the
     * interface of the search algorithm requiring that the name and directory of
     * the pattern be specified separately.
     */

    // Extract the directory path.
    AutoStr* searchDirectory = AutoStr_NewStd();
    F_FileDir(searchDirectory, searchPattern);

    if(!Str_IsEmpty(searchDirectory))
    {
        PathList foundPaths = collectLocalPaths(searchDirectory, true/*include the searchDirectory in paths*/);
        DENG2_FOR_EACH(i, foundPaths, PathList::const_iterator)
        {
            PathListItem const& found = *i;
            QByteArray foundPathAscii = found.path.toAscii();
            bool patternMatched = F_MatchFileName(foundPathAscii.constData(), Str_Text(searchPattern));

            if(!patternMatched) continue;

            int result = callback(foundPathAscii.constData(), (found.attrib & A_SUBDIR)? PT_BRANCH : PT_LEAF, parameters);
            if(result) return result;
        }
    }

    return 0; // Not found.
}

int F_AllResourcePaths(char const* searchPath, int flags,
    int (*callback) (char const* path, PathDirectoryNodeType type, void* parameters))
{
    return F_AllResourcePaths2(searchPath, flags, callback, 0);
}

static FILE* findRealFile(char const* path, char const* mymode, ddstring_t** foundPath)
{
    char mode[8];
    strcpy(mode, "r"); // Open for reading.
    if(strchr(mymode, 't'))
        strcat(mode, "t");
    if(strchr(mymode, 'b'))
        strcat(mode, "b");

    ddstring_t nativePath;
    Str_Init(&nativePath); Str_Set(&nativePath, path);
    F_ToNativeSlashes(&nativePath, &nativePath);

    // Try opening as a real file.
    FILE* file = fopen(Str_Text(&nativePath), mode);
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
        return 0;
    }

    ddstring_t* mapped = Str_New();
    for(uint i = 0; i < vdMappingsCount; ++i)
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

    return 0;
}

AbstractFile* FS::findLumpFile(char const* path, int* lumpIdx)
{
    if(!inited || !path || !path[0]) return 0;

    /**
     * First check the Zip directory.
     */

    // Convert to an absolute path.
    ddstring_t absSearchPath;
    Str_Init(&absSearchPath); Str_Set(&absSearchPath, path);
    F_PrependBasePath(&absSearchPath, &absSearchPath);

    // Perform the search.
    lumpnum_t lumpNum = zipLumpDirectory->indexForPath(Str_Text(&absSearchPath));
    if(lumpNum >= 0)
    {
        Str_Free(&absSearchPath);

        LumpInfo const* lumpInfo = zipLumpDirectory->lumpInfo(lumpNum);
        DENG_ASSERT(lumpInfo);
        if(lumpIdx) *lumpIdx = lumpInfo->lumpIdx;
        return reinterpret_cast<AbstractFile*>(lumpInfo->container);
    }

    /**
     * Next try the dir/WAD redirects.
     */
    if(ldMappingsCount)
    {
        // We must have an absolute path, so prepend the current working directory if necessary.
        Str_Set(&absSearchPath, path);
        F_PrependWorkPath(&absSearchPath, &absSearchPath);

        for(uint i = 0; i < ldMappingsCount; ++i)
        {
            ldmapping_t* rec = &ldMappings[i];
            lumpnum_t absoluteLumpNum;

            if(Str_CompareIgnoreCase(&rec->path, Str_Text(&absSearchPath))) continue;

            absoluteLumpNum = F_CheckLumpNumForName2(rec->lumpName, true);
            if(absoluteLumpNum < 0) continue;

            Str_Free(&absSearchPath);
            return findFileForLumpNum(absoluteLumpNum, lumpIdx);
        }
    }

    Str_Free(&absSearchPath);
    return NULL;
}

static DFile* tryOpenFile3(DFile* file, const char* path, const LumpInfo* info);

static DFile* openAsLumpFile(AbstractFile* container, int lumpIdx,
    const char* _absPath, bool isDehackedPatch, bool /*dontBuffer*/)
{
    ddstring_t absPath; Str_Init(&absPath);

    // Prepare the name of this single-lump file.
    if(isDehackedPatch)
    {
        // Copy the path up to and including the last directory separator if present.
        char const* slash = strrchr(_absPath, '/');
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
    /// @todo The way this buffering works is nonsensical it should not be done here
    ///        but should instead be deferred until the content of the lump is read.
    DFile* hndl = DFileBuilder::fromFileLump(*container, lumpIdx, false/*dontBuffer*/);

    // Prepare the temporary info descriptor.
    LumpInfo info; F_InitLumpInfo(&info);
    F_CopyLumpInfo(&info, container->lumpInfo(lumpIdx));

    // Try to open the referenced file as a specialised file type.
    DFile* file = tryOpenFile3(hndl, Str_Text(&absPath), &info);

    // If not opened; assume its a generic LumpFile.
    if(!file)
    {
        LumpFile* lump = new LumpFile(*hndl, Str_Text(&absPath), info);
        file = DFileBuilder::fromFile(*lump);
    }
    DENG_ASSERT(file);

    // We're done with the descriptor.
    F_DestroyLumpInfo(&info);

    Str_Free(&absPath);

    return file;
}

static DFile* tryOpenAsZipFile(DFile* hndl, char const* path, LumpInfo const* info)
{
    if(inited && ZipFile::recognise(*hndl))
    {
        ZipFile* zip = new ZipFile(*hndl, path, *info);
        return DFileBuilder::fromFile(*zip);
    }
    return NULL;
}

static DFile* tryOpenAsWadFile(DFile* hndl, char const* path, LumpInfo const* info)
{
    if(inited && WadFile::recognise(*hndl))
    {
        WadFile* wad = new WadFile(*hndl, path, *info);
        return DFileBuilder::fromFile(*wad);
    }
    return NULL;
}

static DFile* tryOpenFile3(DFile* file, char const* path, LumpInfo const* info)
{
    static const struct filehandler_s {
        resourcetype_t resourceType;
        DFile* (*tryOpenFile)(DFile* file, char const* path, LumpInfo const* info);
    } handlers[] = {
        { RT_ZIP,  tryOpenAsZipFile },
        { RT_WAD,  tryOpenAsWadFile },
        { RT_NONE, NULL }
    }, *hdlr = NULL;
    DENG_ASSERT(file && path && path[0] && info);

    resourcetype_t resourceType = F_GuessResourceTypeByName(path);

    // Firstly try the expected format given the file name.
    DFile* hndl = 0;
    for(hdlr = handlers; hdlr->tryOpenFile; hdlr++)
    {
        if(hdlr->resourceType != resourceType) continue;

        hndl = hdlr->tryOpenFile(file, path, info);
        break;
    }

    // If not yet loaded; try each recognisable format.
    /// @todo Order here should be determined by the resource locator.
    int n = 0;
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
static DFile* tryOpenFile2(char const* path, char const* mode, size_t baseOffset,
    bool allowDuplicate)
{
    if(!path || !path[0]) return 0;

    if(!mode) mode = "";
    bool dontBuffer  = !!strchr(mode, 'x');
    bool reqRealFile = !!strchr(mode, 'f');

    // Make it a full path.
    ddstring_t searchPath; Str_Init(&searchPath);
    Str_Set(&searchPath, path);
    F_FixSlashes(&searchPath, &searchPath);
    F_ExpandBasePath(&searchPath, &searchPath);

    DEBUG_VERBOSE2_Message(("tryOpenFile2: trying to open %s\n", Str_Text(&searchPath)));

    // First check for lumps?
    if(!reqRealFile)
    {
        int lumpIdx;
        AbstractFile* container = FS::findLumpFile(Str_Text(&searchPath), &lumpIdx);
        if(container)
        {
            // Do not read files twice.
            if(!allowDuplicate && !F_CheckFileId(Str_Text(&searchPath))) return 0;

            // DeHackEd patch files require special handling...
            resourcetype_t type = F_GuessResourceTypeByName(path);

            DFile* dfile = openAsLumpFile(container, lumpIdx, Str_Text(&searchPath), (type == RT_DEH), dontBuffer);
            Str_Free(&searchPath);
            return dfile;
        }
    }

    // Try to open as a real file then. We must have an absolute path, so
    // prepend the current working directory if necessary.
    F_PrependWorkPath(&searchPath, &searchPath);
    ddstring_t* foundPath = 0;
    FILE* nativeFile = findRealFile(Str_Text(&searchPath), mode, &foundPath);
    if(!nativeFile)
    {
        Str_Free(&searchPath);
        return 0;
    }

    // Do not read files twice.
    if(!allowDuplicate && !F_CheckFileId(Str_Text(foundPath)))
    {
        fclose(nativeFile);
        Str_Delete(foundPath);
        Str_Free(&searchPath);
        return NULL;
    }

    // Acquire a handle on the file we intend to open.
    DFile* hndl = DFileBuilder::fromNativeFile(*nativeFile, baseOffset);

    // Prepare the temporary info descriptor.
    LumpInfo info; F_InitLumpInfo(&info);
    info.lastModified = F_LastModified(Str_Text(foundPath));

    // Search path is used here rather than found path as the latter may have
    // been mapped to another location. We want the file to be attributed with
    // the path it is to be known by throughout the virtual file system.

    DFile* dfile = tryOpenFile3(hndl, Str_Text(&searchPath), &info);
    // If still not loaded; this an unknown format.
    if(!dfile)
    {
        GenericFile* file = new GenericFile(*hndl, Str_Text(&searchPath), info);
        dfile = DFileBuilder::fromFile(*file);
    }
    DENG_ASSERT(dfile);

    // We're done with the descriptor.
    F_DestroyLumpInfo(&info);

    Str_Delete(foundPath);
    Str_Free(&searchPath);

    return dfile;
}

static DFile* tryOpenFile(char const* path, char const* mode, size_t baseOffset, bool allowDuplicate)
{
    DFile* file = tryOpenFile2(path, mode, baseOffset, allowDuplicate);
    if(file)
    {
        openFiles->push_back(file);
        return &file->setList(reinterpret_cast<struct filelist_s*>(openFiles));
    }
    return NULL;
}

DFile* FS::openFile(char const* path, char const* mode, size_t baseOffset, bool allowDuplicate)
{
#if _DEBUG
    DENG_ASSERT(path && mode);
    for(uint i = 0; mode[i]; ++i)
    {
        if(mode[i] != 'r' && mode[i] != 't' && mode[i] != 'b' && mode[i] != 'f')
            Con_Error("FS::openFile: Unsupported file open-op in mode string %s for path \"%s\"\n", mode, path);
    }
#endif
    return tryOpenFile(path, mode, baseOffset, allowDuplicate);
}

int F_Access(char const* path)
{
    DFile* file = tryOpenFile(path, "rx", 0, true);
    if(file)
    {
        FS::deleteFile(file);
        return true;
    }
    return false;
}

uint F_GetLastModified(char const* fileName)
{
    // Try to open the file, but don't buffer any contents.
    DFile* hndl = tryOpenFile(fileName, "rx", 0, true);
    uint modified = 0;
    if(hndl)
    {
        modified = hndl->file()->lastModified();
        FS::deleteFile(hndl);
    }
    return modified;
}

static LumpDirectory* lumpDirectoryForFileType(filetype_t fileType)
{
    switch(fileType)
    {
    case FT_ZIPFILE:    return zipLumpDirectory;
    case FT_LUMPFILE:
    case FT_WADFILE:    return ActiveWadLumpDirectory;
    default: return NULL;
    }
}

AbstractFile* FS::addFile(char const* path, size_t baseOffset)
{
    DFile* hndl = FS::openFile(path, "rb", baseOffset, false);
    if(!hndl)
    {
        if(F_Access(path))
        {
            Con_Message("\"%s\" already loaded.\n", F_PrettyPath(path));
        }
        return 0;
    }

    AbstractFile* file = hndl->file();
    VERBOSE( Con_Message("Loading \"%s\"...\n", F_PrettyPath(Str_Text(file->path()))) )

    if(loadingForStartup)
    {
        file->setStartup(true);
    }

    DFile* loadedFilesHndl = DFileBuilder::dup(*hndl);
    loadedFiles->push_back(loadedFilesHndl); loadedFilesHndl->setList(reinterpret_cast<struct filelist_s*>(loadedFiles));

    // Publish lumps to a directory?
    LumpDirectory* lumpDir = lumpDirectoryForFileType(file->type());
    if(lumpDir)
    {
        file->publishLumpsToDirectory(lumpDir);
    }
    return file;
}

int FS::addFiles(char const* const* paths, int num)
{
    if(!paths) return 0;

    int addedFileCount = 0;
    for(int i = 0; i < num; ++i)
    {
        if(FS::addFile(paths[i]))
        {
            VERBOSE2( Con_Message("Done loading %s\n", F_PrettyPath(paths[i])) )
            addedFileCount += 1;
        }
        else
        {
            Con_Message("Warning: Errors occured while loading %s\n", paths[i]);
        }
    }

    // A changed file list may alter the main lump directory.
    if(addedFileCount)
    {
        DD_UpdateEngineState();
    }

    return addedFileCount;
}

bool FS::removeFile(char const* path, bool permitRequired)
{
    bool unloadedResources = unloadFile(path, permitRequired);
    if(unloadedResources)
    {
        DD_UpdateEngineState();
    }
    return unloadedResources;
}

int FS::removeFiles(char const* const* filenames, int num, bool permitRequired)
{
    if(!filenames) return 0;

    int removedFileCount = 0;
    for(int i = 0; i < num; ++i)
    {
        if(unloadFile(filenames[i], permitRequired))
        {
            VERBOSE2( Con_Message("Done unloading %s\n", F_PrettyPath(filenames[i])) )
            removedFileCount += 1;
        }
    }

    // A changed file list may alter the main lump directory.
    if(removedFileCount)
    {
        DD_UpdateEngineState();
    }
    return removedFileCount;
}

DFile* FS::openLump(lumpnum_t absoluteLumpNum)
{
    int lumpIdx;
    AbstractFile* container = FS::findFileForLumpNum(absoluteLumpNum, &lumpIdx);
    if(!container) return 0;

    LumpFile* lump = new LumpFile(*DFileBuilder::fromFileLump(*container, lumpIdx, false),
                                  Str_Text(container->composeLumpPath(lumpIdx)),
                                  *container->lumpInfo(lumpIdx));
    if(!lump) return 0;

    DFile* openFileHndl = DFileBuilder::fromFile(*lump);
    openFiles->push_back(openFileHndl); openFileHndl->setList(reinterpret_cast<struct filelist_s*>(openFiles));
    return openFileHndl;
}

static void clearLDMappings()
{
    if(ldMappings)
    {
        for(uint i = 0; i < ldMappingsCount; ++i)
        {
            Str_Free(&ldMappings[i].path);
        }
        M_Free(ldMappings); ldMappings = 0;
    }
    ldMappingsCount = ldMappingsMax = 0;
}

static ldmapping_t* findLDMappingForPath(char const* path)
{
    ldmapping_t* ldm = 0;
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

void F_AddLumpDirectoryMapping(char const* lumpName, char const* symbolicPath)
{
    if(!lumpName || !lumpName[0] || !symbolicPath || !symbolicPath[0]) return;

    // Convert the symbolic path into a real path.
    ddstring_t path; Str_Init(&path);
    Str_Set(&path, symbolicPath);
    F_ResolveSymbolicPath(&path, &path);

    // Since the path might be relative, let's explicitly make the path absolute.
    ddstring_t fullPath;
    {
        char* full = _fullpath(0, Str_Text(&path), 0);
        Str_Set(Str_Init(&fullPath), full);
        F_FixSlashes(&fullPath, &fullPath);
        free(full);
    }
    Str_Free(&path);

    // Have already mapped this path?
    ldmapping_t* ldm = findLDMappingForPath(Str_Text(&fullPath));
    if(!ldm)
    {
        // No. Acquire another mapping.
        if(++ldMappingsCount > ldMappingsMax)
        {
            ldMappingsMax *= 2;
            if(ldMappingsMax < ldMappingsCount)
                ldMappingsMax = 2*ldMappingsCount;

            ldMappings = (ldmapping_t*) M_Realloc(ldMappings, ldMappingsMax * sizeof *ldMappings);
            if(!ldMappings) Con_Error("F_AddLumpDirectoryMapping: Failed on allocation of %lu bytes for mapping list.", (unsigned long) (ldMappingsMax * sizeof *ldMappings));
        }
        ldm = &ldMappings[ldMappingsCount - 1];

        Str_Set(Str_Init(&ldm->path), Str_Text(&fullPath));
    }

    Str_Free(&fullPath);

    // Set the lumpname.
    memcpy(ldm->lumpName, lumpName, sizeof ldm->lumpName);
    ldm->lumpName[LUMPNAME_T_LASTINDEX] = '\0';

    VERBOSE( Con_Message("F_AddLumpDirectoryMapping: \"%s\" -> %s\n", ldm->lumpName, F_PrettyPath(Str_Text(&ldm->path))) )
}

/// Skip all whitespace except newlines.
static inline char const* skipSpace(char const* ptr)
{
    DENG_ASSERT(ptr);
    while(*ptr && *ptr != '\n' && isspace(*ptr))
    { ptr++; }
    return ptr;
}

static bool parseLDMapping(lumpname_t lumpName, ddstring_t* path, char const* buffer)
{
    DENG_ASSERT(lumpName && path);

    // Find the start of the lump name.
    char const* ptr = skipSpace(buffer);

    // Just whitespace?
    if(!*ptr || *ptr == '\n') return false;

    // Find the end of the lump name.
    char const* end = (char const*)M_FindWhite((char*)ptr);
    if(!*end || *end == '\n') return false;

    size_t len = end - ptr;
    // Invalid lump name?
    if(len > 8) return false;

    memset(lumpName, 0, LUMPNAME_T_MAXLEN);
    strncpy(lumpName, ptr, len);
    strupr(lumpName);

    // Find the start of the file path.
    ptr = skipSpace(end);
    if(!*ptr || *ptr == '\n') return false; // Missing file path.

    // We're at the file path.
    Str_Set(path, ptr);
    // Get rid of any extra whitespace on the end.
    Str_StripRight(path);
    F_FixSlashes(path, path);
    return true;
}

/**
 * <pre>LUMPNAM0 \Path\In\The\Base.ext
 * LUMPNAM1 Path\In\The\RuntimeDir.ext
 *  :</pre>
 */
static bool parseLDMappingList(char const* buffer)
{
    DENG_ASSERT(buffer);

    bool successful = false;
    ddstring_t path; Str_Init(&path);
    ddstring_t line; Str_Init(&line);

    char const* ch = buffer;
    lumpname_t lumpName;
    do
    {
        ch = Str_GetLine(&line, ch);
        if(!parseLDMapping(lumpName, &path, Str_Text(&line)))
        {
            // Failure parsing the mapping.
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
    static bool inited = false;
    size_t bufSize = 0;
    uint8_t* buf = NULL;

    if(inited)
    {
        // Free old paths, if any.
        clearLDMappings();
    }

    if(DD_IsShuttingDown()) return;

    // Add the contents of all DD_DIREC lumps.
    for(lumpnum_t i = 0; i < F_LumpCount(); ++i)
    {
        if(strnicmp(F_LumpName(i), "DD_DIREC", 8)) continue;

        // Make a copy of it so we can ensure it ends in a null.
        LumpInfo const* info = F_FindInfoForLumpNum(i);
        size_t lumpLength = info->size;
        if(bufSize < lumpLength + 1)
        {
            bufSize = lumpLength + 1;
            buf = (uint8_t*) M_Realloc(buf, bufSize);
            if(!buf) Con_Error("F_InitLumpDirectoryMappings: Failed on (re)allocation of %lu bytes for temporary read buffer.", (unsigned long) bufSize);
        }

        int lumpIdx;
        AbstractFile* file = FS::findFileForLumpNum(i, &lumpIdx);
        DENG_ASSERT(file);
        file->readLump(lumpIdx, buf, 0, lumpLength);
        buf[lumpLength] = 0;
        parseLDMappingList(reinterpret_cast<char const*>(buf));
    }

    if(buf) M_Free(buf);
    inited = true;
}

static void clearVDMappings()
{
    if(vdMappings)
    {
        // Free the allocated memory.
        for(uint i = 0; i < vdMappingsCount; ++i)
        {
            Str_Free(&vdMappings[i].source);
            Str_Free(&vdMappings[i].destination);
        }
        M_Free(vdMappings); vdMappings = 0;
    }
    vdMappingsCount = vdMappingsMax = 0;
}

/// @return  @c true iff the mapping matched the path.
static bool applyVDMapping(ddstring_t* path, vdmapping_t* vdm)
{
    if(path && vdm && !strnicmp(Str_Text(path), Str_Text(&vdm->destination), Str_Length(&vdm->destination)))
    {
        // Replace the beginning with the source path.
        ddstring_t temp; Str_InitStd(&temp);
        Str_Set(&temp, Str_Text(&vdm->source));
        Str_PartAppend(&temp, Str_Text(path), Str_Length(&vdm->destination), Str_Length(path) - Str_Length(&vdm->destination));
        Str_Copy(path, &temp);
        Str_Free(&temp);
        return true;
    }
    return false;
}

static vdmapping_t* findVDMappingForSourcePath(char const* source)
{
    vdmapping_t* vdm = 0;
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

void F_AddVirtualDirectoryMapping(char const* source, char const* destination)
{
    if(!source || !source[0] || !destination || !destination[0]) return;

    // Make this an absolute path.
    ddstring_t src; Str_InitStd(&src);
    Str_Set(&src, source);
    Str_Strip(&src);
    F_FixSlashes(&src, &src);
    F_ExpandBasePath(&src, &src);
    F_PrependWorkPath(&src, &src);
    F_AppendMissingSlash(&src);

    // Have already mapped this source path?
    vdmapping_t* vdm = findVDMappingForSourcePath(Str_Text(&src));
    if(!vdm)
    {
        // No. Acquire another mapping.
        if(++vdMappingsCount > vdMappingsMax)
        {
            vdMappingsMax *= 2;
            if(vdMappingsMax < vdMappingsCount)
                vdMappingsMax = 2*vdMappingsCount;

            vdMappings = (vdmapping_t*) M_Realloc(vdMappings, vdMappingsMax * sizeof *vdMappings);
            if(!vdMappings) Con_Error("F_AddVirtualDirectoryMapping: Failed on allocation of %lu bytes for mapping list.", (unsigned long) (vdMappingsMax * sizeof *vdMappings));
        }
        vdm = &vdMappings[vdMappingsCount - 1];

        Str_Set(Str_Init(&vdm->source), Str_Text(&src));
        Str_Init(&vdm->destination);
    }

    Str_Free(&src);

    // Set the destination.
    Str_Set(&vdm->destination, destination);
    Str_Strip(&vdm->destination);
    F_FixSlashes(&vdm->destination, &vdm->destination);
    F_ExpandBasePath(&vdm->destination, &vdm->destination);
    F_PrependWorkPath(&vdm->destination, &vdm->destination);
    F_AppendMissingSlash(&vdm->destination);

    VERBOSE( Con_Message("Resources in \"%s\" now mapped to \"%s\"\n", F_PrettyPath(Str_Text(&vdm->source)), F_PrettyPath(Str_Text(&vdm->destination))) )
}

void F_InitVirtualDirectoryMappings(void)
{
    clearVDMappings();

    if(DD_IsShuttingDown()) return;

    // Create virtual directory mappings by processing all -vdmap options.
    int argC = CommandLine_Count();
    for(int i = 0; i < argC; ++i)
    {
        if(strnicmp("-vdmap", CommandLine_At(i), 6))
            continue; // This is not the option we're looking for.

        if(i < argC - 1 && !CommandLine_IsOption(i + 1) && !CommandLine_IsOption(i + 2))
        {
            F_AddVirtualDirectoryMapping(CommandLine_PathAt(i + 1), CommandLine_At(i + 2));
            i += 2;
        }
    }
}

static int compareFileByFilePath(void const* a_, void const* b_)
{
    AbstractFile* a = *((AbstractFile* const*)a_);
    AbstractFile* b = *((AbstractFile* const*)b_);
    return Str_CompareIgnoreCase(a->path(), Str_Text(b->path()));
}

/**
 * Prints the resource path to the console.
 * This is a f_allresourcepaths_callback_t.
 */
int printResourcePath(char const* fileName, PathDirectoryNodeType /*type*/,
    void* /*parameters*/)
{
    bool makePretty = CPP_BOOL( F_IsRelativeToBase(fileName, ddBasePath) );
    Con_Printf("  %s\n", makePretty? F_PrettyPath(fileName) : fileName);
    return 0; // Continue the listing.
}

static void printVFDirectory(ddstring_t const* path)
{
    ddstring_t dir; Str_InitStd(&dir);

    Str_Set(&dir, Str_Text(path));
    Str_Strip(&dir);
    F_FixSlashes(&dir, &dir);
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

/// Print contents of directories as Doomsday sees them.
D_CMD(Dir)
{
    DENG_UNUSED(src);

    ddstring_t path; Str_InitStd(&path);
    if(argc > 1)
    {
        for(int i = 1; i < argc; ++i)
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

/// Dump a copy of a virtual file to the runtime directory.
D_CMD(DumpLump)
{
    DENG_UNUSED(src);
    DENG_UNUSED(argc);

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

/**
 * Print a content listing of lumps in this directory to stdout (for debug).
 */
static void printLumpDirectory(LumpDirectory& ld)
{
    const int numRecords = ld.size();
    const int numIndexDigits = MAX_OF(3, M_NumDigits(numRecords));

    Con_Printf("LumpDirectory %p (%i records):\n", &ld, numRecords);

    int idx = 0;
    DENG2_FOR_EACH(i, ld.lumps(), LumpDirectory::LumpInfos::const_iterator)
    {
        LumpInfo const* lumpInfo = *i;
        AbstractFile* container = reinterpret_cast<AbstractFile*>(lumpInfo->container);
        AutoStr* lumpPath = container->composeLumpPath(lumpInfo->lumpIdx);
        Con_Printf("%0*i - \"%s:%s\" (size: %lu bytes%s)\n", numIndexDigits, idx++,
                   F_PrettyPath(Str_Text(container->path())),
                   F_PrettyPath(Str_Text(lumpPath)),
                   (unsigned long) lumpInfo->size,
                   (lumpInfo->compressedSize != lumpInfo->size? " compressed" : ""));
    }
    Con_Printf("---End of lumps---\n");
}

/// List virtual files inside containers.
D_CMD(ListLumps)
{
    DENG_UNUSED(src);
    DENG_UNUSED(argc);
    DENG_UNUSED(argv);

    if(inited)
    {
        // Always the primary directory.
        printLumpDirectory(*primaryWadLumpDirectory);
        return true;
    }
    Con_Printf("WAD module is not presently initialized.\n");
    return false;
}

/**
 * @param size  If not @c NULL the number of elements in the resultant
 *              array is written back here (for convenience).
 *
 * @return  Array of ptrs to files in this list or @c NULL if empty.
 *      Ownership of the array passes to the caller who should ensure to
 *      release it with free() when no longer needed.
 */
static AbstractFile** collectFiles(FileList* list, int* count)
{
    if(list && !list->empty())
    {
        AbstractFile** arr = (AbstractFile**) M_Malloc(list->size() * sizeof *arr);
        if(!arr) Con_Error("collectFiles: Failed on allocation of %lu bytes for file list.", (unsigned long) (list->size() * sizeof *arr));

        for(int i = 0; i < list->size(); ++i)
        {
            DFile const* hndl = (*list)[i];
            arr[i] = hndl->file();
        }
        if(count) *count = list->size();
        return arr;
    }

    if(count) *count = 0;
    return NULL;
}

/// List the "real" files presently loaded in original load order.
D_CMD(ListFiles)
{
    DENG_UNUSED(src);
    DENG_UNUSED(argc);
    DENG_UNUSED(argv);

    size_t totalFiles = 0, totalPackages = 0;
    if(inited)
    {
        int fileCount;
        AbstractFile** arr = collectFiles(loadedFiles, &fileCount);
        if(!arr) return true;

        // Sort files so we get a nice alpha-numerical list.
        qsort(arr, fileCount, sizeof *arr, compareFileByFilePath);

        AbstractFile** ptr = arr;
        for(int i = 0; i < fileCount; ++i, ptr++)
        {
            AbstractFile* file = *ptr;
            uint crc;

            int fileCount = file->lumpCount();
            switch(file->type())
            {
            case FT_GENERICFILE:
                crc = 0;
                break;
            case FT_ZIPFILE:
                crc = 0;
                break;
            case FT_WADFILE:
                crc = (!file->hasCustom()? reinterpret_cast<WadFile*>(file)->calculateCRC() : 0);
                break;
            case FT_LUMPFILE:
                crc = 0;
                break;
            default:
                Con_Error("CCmdListLumps: Invalid file type %i.", file->type());
                exit(1); // Unreachable.
            }

            Con_Printf("\"%s\" (%i %s%s)", F_PrettyPath(Str_Text(file->path())),
                       fileCount, fileCount != 1 ? "files" : "file",
                       (file->hasStartup()? ", startup" : ""));
            if(0 != crc)
                Con_Printf(" [%08x]", crc);
            Con_Printf("\n");

            totalFiles += size_t(fileCount);
            ++totalPackages;
        }

        M_Free(arr);
    }
    Con_Printf("Total: %lu files in %lu packages.\n", (unsigned long) totalFiles, (unsigned long)totalPackages);
    return true;
}

/**
 * C Wrapper API
 */

struct abstractfile_s* F_AddFile2(char const* path, size_t baseOffset)
{
    return reinterpret_cast<struct abstractfile_s*>(FS::addFile(path, baseOffset));
}

struct abstractfile_s* F_AddFile(char const* path)
{
    return reinterpret_cast<struct abstractfile_s*>(FS::addFile(path));
}

boolean F_RemoveFile2(char const* path, boolean permitRequired)
{
    return FS::removeFile(path, CPP_BOOL(permitRequired));
}

boolean F_RemoveFile(char const* path)
{
    return FS::removeFile(path);
}

int F_AddFiles(char const* const* paths, int num)
{
    return FS::addFiles(paths, num);
}

int F_RemoveFiles2(char const* const* filenames, int num, boolean permitRequired)
{
    return FS::removeFiles(filenames, num, CPP_BOOL(permitRequired));
}

int F_RemoveFiles(char const* const* filenames, int num)
{
    return FS::removeFiles(filenames, num);
}

void F_ReleaseFile(struct abstractfile_s* file)
{
    FS::releaseFile(reinterpret_cast<AbstractFile*>(file));
}

struct dfile_s* F_Open3(char const* path, char const* mode, size_t baseOffset, boolean allowDuplicate)
{
    return reinterpret_cast<struct dfile_s*>(FS::openFile(path, mode, baseOffset, CPP_BOOL(allowDuplicate)));
}

struct dfile_s* F_Open2(char const* path, char const* mode, size_t baseOffset)
{
    return reinterpret_cast<struct dfile_s*>(FS::openFile(path, mode, baseOffset));
}

struct dfile_s* F_Open(char const* path, char const* mode)
{
    return reinterpret_cast<struct dfile_s*>(FS::openFile(path, mode));
}

struct dfile_s* F_OpenLump(lumpnum_t absoluteLumpNum)
{
    return reinterpret_cast<struct dfile_s*>(FS::openLump(absoluteLumpNum));
}

void F_Close(struct dfile_s* hndl)
{
    FS::closeFile(reinterpret_cast<DFile*>(hndl));
}

void F_Delete(struct dfile_s* hndl)
{
    FS::deleteFile(reinterpret_cast<DFile*>(hndl));
}

LumpInfo const* F_LumpInfo(struct abstractfile_s* _file, int lumpIdx)
{
    if(!_file) return 0;
    AbstractFile* file = reinterpret_cast<AbstractFile*>(_file);
    return file->lumpInfo(lumpIdx);
}

size_t F_ReadLump(struct abstractfile_s* _file, int lumpIdx, uint8_t* buffer)
{
    if(!_file) return 0;
    AbstractFile* file = reinterpret_cast<AbstractFile*>(_file);
    return file->readLump(lumpIdx, buffer);
}

size_t F_ReadLumpSection(struct abstractfile_s* _file, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    if(!_file) return 0;
    AbstractFile* file = reinterpret_cast<AbstractFile*>(_file);
    return file->readLump(lumpIdx, buffer, startOffset, length);
}

uint8_t const* F_CacheLump(struct abstractfile_s* _file, int lumpIdx)
{
    if(!_file) return 0;
    AbstractFile* file = reinterpret_cast<AbstractFile*>(_file);
    return file->cacheLump(lumpIdx);
}

void F_UnlockLump(struct abstractfile_s* _file, int lumpIdx)
{
    if(!_file) return;
    AbstractFile* file = reinterpret_cast<AbstractFile*>(_file);
    file->unlockLump(lumpIdx);
}

struct abstractfile_s* F_FindFileForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx)
{
    return reinterpret_cast<struct abstractfile_s*>(FS::findFileForLumpNum(absoluteLumpNum, lumpIdx));
}

struct abstractfile_s* F_FindFileForLumpNum(lumpnum_t absoluteLumpNum)
{
    return reinterpret_cast<struct abstractfile_s*>(FS::findFileForLumpNum(absoluteLumpNum, 0));
}

AutoStr* F_ComposeLumpPath2(struct abstractfile_s* _file, int lumpIdx, char delimiter)
{
    if(!_file) return AutoStr_NewStd();
    AbstractFile* file = reinterpret_cast<AbstractFile*>(_file);
    return file->composeLumpPath(lumpIdx, delimiter);
}

AutoStr* F_ComposeLumpPath(struct abstractfile_s* file, int lumpIdx)
{
    return F_ComposeLumpPath2(file, lumpIdx, '/');
}
