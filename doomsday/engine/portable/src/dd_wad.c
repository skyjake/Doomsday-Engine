/**\file dd_wad.c
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
#include "de_misc.h" // For M_LimitedStrCat

#include "lumpdirectory.h"
#include "lumpfile.h"
#include "wadfile.h"

D_CMD(DumpLump);
D_CMD(ListFiles);

typedef struct filelist_node_s {
    boolean loadedForStartup; // For reset.
    abstractfile_t* fsObject;
    struct filelist_node_s* next;
} filelist_node_t;
typedef filelist_node_t* filelist_t;

static boolean inited = false;
static boolean loadingForStartup;

// Head of the llist of wadfile nodes.
static filelist_t fileList;

static lumpdirectory_t* zipLumpDirectory;

static lumpdirectory_t* primaryWadLumpDirectory;
static lumpdirectory_t* auxiliaryWadLumpDirectory;
// @c true = one or more files have been opened using the auxiliary directory.
static boolean auxiliaryWadLumpDirectoryInUse;

// Currently selected lump directory.
static lumpdirectory_t* ActiveWadLumpDirectory;

void W_Register(void)
{
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

static zipfile_t* newZipFile(DFILE* handle, const char* absolutePath, lumpdirectory_t* directory)
{
    return (zipfile_t*) linkFile((abstractfile_t*)ZipFile_New(handle, absolutePath, directory),
        loadingForStartup);
}

static wadfile_t* newWadFile(DFILE* handle, const char* absolutePath, lumpdirectory_t* directory)
{
    return (wadfile_t*) linkFile((abstractfile_t*)WadFile_New(handle, absolutePath, directory),
        loadingForStartup);
}

static lumpfile_t* newLumpFile(DFILE* handle, const char* absolutePath, lumpdirectory_t* directory,
    lumpname_t name, size_t lumpSize)
{
    return (lumpfile_t*) linkFile((abstractfile_t*)LumpFile_New(handle, absolutePath, directory, name, lumpSize),
        loadingForStartup);
}

extern lumpdirectory_t* zipLumpDirectory;

zipfile_t* W_AddZipFile(const char* path, DFILE* handle)
{
    zipfile_t* zip = NULL;
    errorIfNotInited("W_AddZipFile");
    if(ZipFile_Recognise(handle))
    {
        // Get a new file record.
        zip = newZipFile(handle, path, zipLumpDirectory);
    }
    return zip;
}

wadfile_t* W_AddWadFile(const char* path, DFILE* handle)
{
    wadfile_t* wad = NULL;
    errorIfNotInited("W_AddWadFile");
    if(WadFile_Recognise(handle))
    {
        // Get a new file record.
        wad = newWadFile(handle, path, ActiveWadLumpDirectory);

        // Print the 'CRC' number of the IWAD, so it can be identified.
        /// \todo Do not do this here.
        if(WadFile_IsIWAD(wad))
            Con_Message("  IWAD identification: %08x\n", WadFile_CalculateCRC(wad));
    }
    return wad;
}

lumpfile_t* W_AddLumpFile(const char* path, DFILE* handle, boolean isDehackedPatch)
{
    lumpfile_t* lump = NULL;
    lumpname_t name;
    errorIfNotInited("W_AddWadFile");
    // Prepare the name of this single-file lump.
    if(isDehackedPatch)
    {
        strncpy(name, "DEHACKED", LUMPNAME_T_MAXLEN);
    }
    else
    {
        // Is there a prefix to be omitted in the name?
        char* slash = strrchr(path, DIR_SEP_CHAR);
        int offset = 0;
        // The slash must not be too early in the string.
        if(slash && slash >= path + 2)
        {
            // Good old negative indices.
            if(slash[-2] == '.' && slash[-1] >= '1' && slash[-1] <= '9')
            {
                offset = slash[-1] - '1' + 1;
            }
        }
        F_ExtractFileBase2(name, path, LUMPNAME_T_MAXLEN, offset);
    }

    lump = newLumpFile(handle, path, ActiveWadLumpDirectory, name, F_Length(handle));

    // Insert the lump into it's rightful place in the directory.
    LumpDirectory_Append(ActiveWadLumpDirectory, &lump->_info, 1, (abstractfile_t*)lump);

    return lump;
}

static boolean removeFile(filelist_node_t* node)
{
    assert(NULL != node && NULL != node->fsObject);
    switch(AbstractFile_Type(node->fsObject))
    {
    case FT_ZIPFILE: {
        zipfile_t* zip = (zipfile_t*)node->fsObject;

        ZipFile_ClearLumpCache(zip);
        LumpDirectory_PruneByFile(AbstractFile_Directory(node->fsObject), node->fsObject);
        unlinkFile(node);
        // Close the file; we don't need it any more.
        ZipFile_Close(zip);
        ZipFile_Delete(zip);
        break;
      }
    case FT_WADFILE: {
        wadfile_t* wad = (wadfile_t*)node->fsObject;

        WadFile_ClearLumpCache(wad);
        LumpDirectory_PruneByFile(AbstractFile_Directory(node->fsObject), node->fsObject);
        unlinkFile(node);
        // Close the file; we don't need it any more.
        WadFile_Close(wad);
        WadFile_Delete(wad);
        break;
      }
    case FT_LUMPFILE: {
        lumpfile_t* lump = (lumpfile_t*)node->fsObject;

        LumpFile_ClearLumpCache(lump);
        LumpDirectory_PruneByFile(AbstractFile_Directory(node->fsObject), node->fsObject);
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

/**
 * Handles conversion to a logical index that is independent of the lump directory currently in use.
 */
static __inline lumpnum_t logicalLumpNum(lumpnum_t lumpNum)
{
    return (ActiveWadLumpDirectory == auxiliaryWadLumpDirectory? lumpNum += AUXILIARY_BASE : lumpNum);
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

/**
 * Selects which lump directory to use, given a logical lump index.
 * This should be called in all functions that access directories by lump index.
 */
static lumpnum_t chooseDirectory(lumpnum_t lumpNum)
{
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

static void clearFileList(lumpdirectory_t* directory)
{
    filelist_node_t* next, *node = fileList;
    while(node)
    {
        next = node->next;
        if(NULL == directory || AbstractFile_Directory(node->fsObject) == directory)
        {
            removeFile(node);
        }
        node = next;
    }
}

void W_Init(void)
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

void W_Shutdown(void)
{
    if(!inited) return;

    W_CloseAuxiliary();
    clearFileList(0);

    LumpDirectory_Delete(primaryWadLumpDirectory), primaryWadLumpDirectory = NULL;
    LumpDirectory_Delete(auxiliaryWadLumpDirectory), auxiliaryWadLumpDirectory = NULL;
    ActiveWadLumpDirectory = NULL;

    LumpDirectory_Delete(zipLumpDirectory), zipLumpDirectory = NULL;

    inited = false;
}

void W_EndStartup(void)
{
    errorIfNotInited("W_EndStartup");
    loadingForStartup = false;
    usePrimaryDirectory();
}

int W_Reset(void)
{
    int unloadedResources = 0;
    if(inited)
    {
        filelist_node_t* node = fileList, *next;
        while(node)
        {
            next = node->next;
            if(!node->loadedForStartup)
            {
                if(W_RemoveFile(Str_Text(AbstractFile_AbsolutePath(node->fsObject))))
                {
                    ++unloadedResources;
                }
            }
            node = next;
        }
    }
    return unloadedResources;
}

int W_LumpCount(void)
{
    if(inited)
        return LumpDirectory_NumLumps(ActiveWadLumpDirectory);
    return 0;
}

boolean W_RemoveFile(const char* path)
{
    filelist_node_t* node;
    errorIfNotInited("W_RemoveFile");
    node = findFileListNodeForName(path);
    if(NULL != node)
        return removeFile(node);
    return false; // No such file loaded.
}

lumpnum_t W_OpenAuxiliary3(const char* path, DFILE* prevOpened, boolean silent)
{
    DFILE* handle;

    errorIfNotInited("W_OpenAuxiliary3");

    if(prevOpened == NULL)
    {   // Try to open the file.
        if((handle = F_Open(path, "rb")) == NULL)
        {
            if(!silent)
            {
                Con_Message("Warning:W_OpenAuxiliary: Resource \"%s\" not found, aborting.\n", path);
            }
            return -1;
        }
    }
    else
    {   // Use the previously opened file.
        handle = prevOpened;
    }

    if(WadFile_Recognise(handle))
    {
        if(auxiliaryWadLumpDirectoryInUse)
        {
            W_CloseAuxiliary();
        }
        ActiveWadLumpDirectory = auxiliaryWadLumpDirectory;
        auxiliaryWadLumpDirectoryInUse = true;

        // Get a new file record.
        newWadFile(handle, path, ActiveWadLumpDirectory);
        return AUXILIARY_BASE;
    }

    if(!silent)
    {
        Con_Message("Warning:W_OpenAuxiliary: Resource \"%s\" does not appear to be a WAD archive.\n", path);
    }
    return -1;
}

lumpnum_t W_OpenAuxiliary2(const char* path, DFILE* prevOpened)
{
    return W_OpenAuxiliary3(path, prevOpened, false);
}

lumpnum_t W_OpenAuxiliary(const char* path)
{
    return W_OpenAuxiliary2(path, NULL);
}

void W_CloseAuxiliary(void)
{
    errorIfNotInited("W_CloseAuxiliary");
    if(useAuxiliaryDirectory())
    {
        clearFileList(auxiliaryWadLumpDirectory);
        auxiliaryWadLumpDirectoryInUse = false;
    }
    usePrimaryDirectory();
}

lumpnum_t W_CheckLumpNumForName2(const char* name, boolean silent)
{
    lumpnum_t idx;

    errorIfNotInited("W_CheckLumpNumForName2");

    if(!name || !name[0])
    {
        if(!silent)
            VERBOSE2( Con_Message("Warning:W_CheckLumpNumForName: Empty name, returning invalid lumpnum.\n") )
        return -1;
    }

    // We have to check both the primary and auxiliary caches because
    // we've only got a name and don't know where it is located. Start with
    // the auxiliary lumps because they take precedence.
    if(useAuxiliaryDirectory())
    {
        idx = LumpDirectory_IndexForName(ActiveWadLumpDirectory, name);
        if(idx != -1)
            return idx;
    }

    usePrimaryDirectory();
    idx = LumpDirectory_IndexForName(ActiveWadLumpDirectory, name);

    if(idx == -1 && !silent)
        VERBOSE2( Con_Message("Warning:W_CheckLumpNumForName: Lump \"%s\" not found.\n", name) )

    return idx;
}

lumpnum_t W_CheckLumpNumForName(const char* name)
{
    return W_CheckLumpNumForName2(name, false);
}

lumpnum_t W_GetLumpNumForName(const char* name)
{
    lumpnum_t idx = W_CheckLumpNumForName(name);
    if(idx != -1)
        return idx;
    Con_Error("W_GetLumpNumForName: Lump \"%s\" not found.", name);
    exit(1); // Unreachable.
}

size_t W_LumpLength(lumpnum_t lumpNum)
{
    errorIfNotInited("W_LumpLength");
    lumpNum = chooseDirectory(lumpNum);
    return LumpDirectory_LumpSize(ActiveWadLumpDirectory, lumpNum);
}

const char* W_LumpName(lumpnum_t lumpNum)
{
    errorIfNotInited("W_LumpName");
    lumpNum = chooseDirectory(lumpNum);
    return LumpDirectory_LumpName(ActiveWadLumpDirectory, lumpNum);
}

void W_ReadLumpSection(lumpnum_t lumpNum, char* buffer, size_t startOffset, size_t length)
{
    abstractfile_t* fsObject;
    errorIfNotInited("W_ReadLumpSection");
    lumpNum = chooseDirectory(lumpNum);
    fsObject = LumpDirectory_SourceFile(ActiveWadLumpDirectory, lumpNum);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:   ZipFile_ReadLumpSection( (zipfile_t*)fsObject, lumpNum, buffer, startOffset, length); break;
    case FT_WADFILE:   WadFile_ReadLumpSection( (wadfile_t*)fsObject, lumpNum, buffer, startOffset, length); break;
    case FT_LUMPFILE: LumpFile_ReadLumpSection((lumpfile_t*)fsObject, lumpNum, buffer, startOffset, length); break;
    default:
        Con_Error("W_ReadLumpSection: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

void W_ReadLump(lumpnum_t absoluteLumpNum, char* buffer)
{
    lumpnum_t lumpNum;
    errorIfNotInited("W_ReadLump");
    lumpNum = chooseDirectory(absoluteLumpNum);
    W_ReadLumpSection(absoluteLumpNum, buffer, 0, LumpDirectory_LumpSize(ActiveWadLumpDirectory, lumpNum));
}

boolean W_DumpLump(lumpnum_t lumpNum, const char* path)
{
    char buf[LUMPNAME_T_LASTINDEX + 4/*.ext*/ + 1];
    abstractfile_t* fsObject;
    const byte* lumpPtr;
    const char* fname;
    FILE* file;

    errorIfNotInited("W_DumpLump");
    lumpNum = chooseDirectory(lumpNum);
    if(LumpDirectory_IsValidIndex(ActiveWadLumpDirectory, lumpNum))
        return false;

    if(path && path[0])
    {
        fname = path;
    }
    else
    {
        memset(buf, 0, sizeof(buf));
        dd_snprintf(buf, 13, "%s.lmp", LumpDirectory_LumpName(ActiveWadLumpDirectory, lumpNum));
        fname = buf;
    }

    if(!(file = fopen(fname, "wb")))
    {
        Con_Printf("Warning: Failed to open %s for writing (%s), aborting.\n", fname, strerror(errno));
        return false;
    }

    fsObject = LumpDirectory_SourceFile(ActiveWadLumpDirectory, lumpNum);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_WADFILE:
        lumpPtr = (byte*)WadFile_CacheLump((wadfile_t*)fsObject, lumpNum, PU_APPSTATIC);
        fwrite(lumpPtr, 1, LumpDirectory_LumpSize(ActiveWadLumpDirectory, lumpNum), file);
        fclose(file);
        WadFile_ChangeLumpCacheTag((wadfile_t*)fsObject, lumpNum, PU_CACHE);
        break;
    case FT_LUMPFILE:
        lumpPtr = (byte*)LumpFile_CacheLump((lumpfile_t*)fsObject, lumpNum, PU_APPSTATIC);
        fwrite(lumpPtr, 1, LumpDirectory_LumpSize(ActiveWadLumpDirectory, lumpNum), file);
        fclose(file);
        LumpFile_ChangeLumpCacheTag((lumpfile_t*)fsObject, lumpNum, PU_CACHE);
        break;
    default:
        Con_Error("W_DumpLump: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }

    Con_Printf("%s dumped to %s.\n", LumpDirectory_LumpName(ActiveWadLumpDirectory, lumpNum), fname);
    return true;
}

const char* W_CacheLump(lumpnum_t lumpNum, int tag)
{
    abstractfile_t* fsObject;
    errorIfNotInited("W_CacheLump");
    lumpNum = chooseDirectory(lumpNum);
    fsObject = LumpDirectory_SourceFile(ActiveWadLumpDirectory, lumpNum);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_WADFILE:    return  WadFile_CacheLump( (wadfile_t*)fsObject, lumpNum, tag);
    case FT_LUMPFILE:   return LumpFile_CacheLump((lumpfile_t*)fsObject, lumpNum, tag);
    default:
        Con_Error("W_CacheLump: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

void W_CacheChangeTag(lumpnum_t lumpNum, int tag)
{
    abstractfile_t* fsObject;
    errorIfNotInited("W_CacheChangeTag");
    lumpNum = chooseDirectory(lumpNum);
    fsObject = LumpDirectory_SourceFile(ActiveWadLumpDirectory, lumpNum);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_WADFILE:    WadFile_ChangeLumpCacheTag( (wadfile_t*)fsObject, lumpNum, tag); break;
    case FT_LUMPFILE:  LumpFile_ChangeLumpCacheTag((lumpfile_t*)fsObject, lumpNum, tag); break;
    default:
        Con_Error("W_CacheChangeTag: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

uint W_LumpLastModified(lumpnum_t lumpNum)
{
    errorIfNotInited("W_LumpLastModified");
    lumpNum = chooseDirectory(lumpNum);
    return LumpDirectory_LumpInfo(ActiveWadLumpDirectory, lumpNum)->lastModified;
}

const char* W_LumpSourceFile(lumpnum_t lumpNum)
{
    errorIfNotInited("W_LumpSourceFile");
    lumpNum = chooseDirectory(lumpNum);
    { abstractfile_t* fsObject = LumpDirectory_SourceFile(ActiveWadLumpDirectory, lumpNum);
    if(NULL != fsObject)
    {
        return Str_Text(AbstractFile_AbsolutePath(fsObject));
    }}
    return "";
}

boolean W_LumpIsFromIWAD(lumpnum_t lumpNum)
{
    errorIfNotInited("W_LumpIsFromIWAD");
    lumpNum = chooseDirectory(lumpNum);
    if(LumpDirectory_IsValidIndex(ActiveWadLumpDirectory, lumpNum))
    {
        abstractfile_t* fsObject = LumpDirectory_SourceFile(ActiveWadLumpDirectory, lumpNum);
        switch(AbstractFile_Type(fsObject))
        {
        case FT_WADFILE:    return  WadFile_IsIWAD( (wadfile_t*)fsObject);
        case FT_LUMPFILE:   return LumpFile_IsIWAD((lumpfile_t*)fsObject);
        default:
            Con_Error("W_LumpIsFromIWAD: Invalid file type %i.", AbstractFile_Type(fsObject));
            exit(1); // Unreachable.
        }
    }
    return false;
}

uint W_CRCNumber(void)
{
    filelist_node_t* node;
    wadfile_t* wad;
    errorIfNotInited("W_CRCNumber");
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

void W_GetPWADFileNames(char* outBuf, size_t outBufSize, char delimiter)
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

void W_PrintLumpDirectory(void)
{
    if(!inited) return;
    // Always the primary directory.
    LumpDirectory_Print(primaryWadLumpDirectory);
}

size_t Zip_GetSize(lumpnum_t lumpNum)
{
    return LumpDirectory_LumpSize(zipLumpDirectory, lumpNum);
}

uint Zip_LastModified(lumpnum_t lumpNum)
{
    return LumpDirectory_LumpInfo(zipLumpDirectory, lumpNum)->lastModified;
}

const char* Zip_SourceFile(lumpnum_t lumpNum)
{
    return Str_Text(AbstractFile_AbsolutePath(LumpDirectory_SourceFile(zipLumpDirectory, lumpNum)));
}

void Zip_ReadFileSection(lumpnum_t lumpNum, char* buffer, size_t startOffset, size_t length)
{
    abstractfile_t* fsObject;
    errorIfNotInited("Zip_ReadFileSection");
    fsObject = LumpDirectory_SourceFile(zipLumpDirectory, lumpNum);
    switch(AbstractFile_Type(fsObject))
    {
    case FT_ZIPFILE:   ZipFile_ReadLumpSection( (zipfile_t*)fsObject, lumpNum, buffer, startOffset, length); break;
    case FT_WADFILE:   WadFile_ReadLumpSection( (wadfile_t*)fsObject, lumpNum, buffer, startOffset, length); break;
    case FT_LUMPFILE: LumpFile_ReadLumpSection((lumpfile_t*)fsObject, lumpNum, buffer, startOffset, length); break;
    default:
        Con_Error("Zip_ReadFileSection: Invalid file type %i.", AbstractFile_Type(fsObject));
        exit(1); // Unreachable.
    }
}

void Zip_ReadFile(lumpnum_t lumpNum, char* buffer)
{
    Zip_ReadFileSection(lumpNum, buffer, 0, LumpDirectory_LumpSize(zipLumpDirectory, lumpNum));
}

int Zip_Iterate2(int (*callback) (const lumpinfo_t*, void*), void* paramaters)
{
    return LumpDirectory_Iterate2(zipLumpDirectory, callback, paramaters);
}

int Zip_Iterate(int (*callback) (const lumpinfo_t*, void*))
{
    return Zip_Iterate2(callback, 0);
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

D_CMD(DumpLump)
{
    if(inited)
    {
        lumpnum_t lumpNum = W_CheckLumpNumForName(argv[1]);
        if(-1 != lumpNum)
        {
            return W_DumpLump(lumpNum, NULL);
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
        size_t fileCount;
        uint crc;
        filelist_node_t* node;
        for(node = fileList; NULL != node; node = node->next)
        {
            switch(AbstractFile_Type(node->fsObject))
            {
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
    }
    Con_Printf("Total: %lu files in %lu packages.\n", (unsigned long) totalFiles, (unsigned long)totalPackages);
    return true;
}
