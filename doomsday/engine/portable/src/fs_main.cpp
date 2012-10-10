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

#include "fileid.h"
#include "game.h"
#include "genericfile.h"
#include "lumpindex.h"
#include "lumpinfo.h"
#include "lumpfile.h"
//#include "m_md5.h"
#include "m_misc.h" // for M_FindWhite()
#include "wadfile.h"
#include "zipfile.h"

#include <QList>
#include <QtAlgorithms>

#include <de/Log>
#include <de/memory.h>

D_CMD(Dir);
D_CMD(DumpLump);
D_CMD(ListFiles);
D_CMD(ListLumps);

using de::FS;
using de::AbstractFile;         using de::DFile;
using de::DFileBuilder;         using de::FileId;
using de::GenericFile;          using de::LumpFile;
using de::LumpInfo;             using de::PathDirectory;
using de::PathDirectoryNode;    using de::WadFile;
using de::ZipFile;

static de::FS* fileSystem;

typedef QList<FileId> FileIds;

/**
 * Virtual (file) path => Lump name mapping.
 *
 * @todo We can't presently use a Map or Hash for these. Although the paths are
 *       unique, several of the existing algorithms which match using patterns
 *       assume they are sorted in a quasi load ordering.
 */
typedef QPair<QString, QString> LumpMapping;
typedef QList<LumpMapping> LumpMappings;

/**
 * Virtual file-directory mapping.
 * Maps one (absolute) path in the virtual file system to another.
 *
 * @todo We can't presently use a Map or Hash for these. Although the paths are
 *       unique, several of the existing algorithms which match using patterns
 *       assume they are sorted in a quasi load ordering.
 */
typedef QPair<QString, QString> PathMapping;
typedef QList<PathMapping> PathMappings;

static bool applyPathMapping(ddstring_t* path, PathMapping const& vdm);

struct FS::Instance
{
    /// Base for indicies in the auxiliary lump index.
    static int const AUXILIARY_BASE = 100000000;

    FS* self;

    /// @c true= Flag newly opened files as "startup".
    bool loadingForStartup;

    /// List of currently opened files.
    FileList openFiles;

    /// List of all loaded files present in the file system.
    FileList loadedFiles;

    /// Database of unique identifiers for all loaded/opened files.
    FileIds fileIds;

    LumpIndex zipLumpIndex;

    LumpIndex primaryWadLumpIndex;
    LumpIndex auxiliaryWadLumpIndex;
    // @c true = one or more files have been opened using the auxiliary index.
    bool auxiliaryWadLumpIndexInUse;

    // Currently selected lump index.
    LumpIndex* ActiveWadLumpIndex;

    /// Virtual (file) path => Lump name mapping.
    LumpMappings lumpMappings;

    /// Virtual file-directory mapping.
    PathMappings pathMappings;

    Instance(FS* d) : self(d), loadingForStartup(true),
        openFiles(), loadedFiles(), fileIds(),
        zipLumpIndex(LIF_UNIQUE_PATHS), primaryWadLumpIndex(),
        auxiliaryWadLumpIndex(), auxiliaryWadLumpIndexInUse(false),
        ActiveWadLumpIndex(&primaryWadLumpIndex)
    {}

    ~Instance()
    {
        clearLoadedFiles();
        clearOpenFiles();
        clearLumpIndexes();

        fileIds.clear(); // Should be null-op if bookkeeping is correct.

        pathMappings.clear();
        lumpMappings.clear();
    }

    /// @return  @c true if the FileId associated with @a path was released.
    bool releaseFileId(char const* path)
    {
        if(path && path[0])
        {
            FileId fileId = FileId::fromPath(path);
            FileIds::iterator place = qLowerBound(fileIds.begin(), fileIds.end(), fileId);
            if(place != fileIds.end() && *place == fileId)
            {
#if _DEBUG
                LOG_VERBOSE("Released FileId %s - \"%s\"") << *place << F_PrettyPath(path);
#endif
                fileIds.erase(place);
                return true;
            }
        }
        return false;
    }

    void clearLoadedFiles(de::LumpIndex* index = 0)
    {
        // Unload in reverse load order.
        for(int i = loadedFiles.size() - 1; i >= 0; i--)
        {
            DFile* hndl = loadedFiles[i];
            if(!index || index->catalogues(*hndl->file()))
            {
                self->unlinkFile(hndl->file());
                loadedFiles.removeAt(i);
                self->deleteFile(hndl);
            }
        }
    }

    void clearOpenFiles()
    {
        while(!openFiles.empty())
        { self->deleteFile(openFiles.back()); }
    }

    int pruneLumpsFromIndexesByFile(AbstractFile& file)
    {
        int pruned = zipLumpIndex.pruneByFile(file)
                   + primaryWadLumpIndex.pruneByFile(file);
        if(auxiliaryWadLumpIndexInUse)
            pruned += auxiliaryWadLumpIndex.pruneByFile(file);
        return pruned;
    }

    /**
     * Handles conversion to a logical index that is independent of the lump index currently in use.
     */
    inline lumpnum_t logicalLumpNum(lumpnum_t lumpNum)
    {
        return (lumpNum < 0 ? -1 :
                ActiveWadLumpIndex == &auxiliaryWadLumpIndex? lumpNum += AUXILIARY_BASE : lumpNum);
    }

    void clearLumpIndexes()
    {
        primaryWadLumpIndex.clear();
        auxiliaryWadLumpIndex.clear();
        zipLumpIndex.clear();

        ActiveWadLumpIndex = 0;
    }

    de::LumpIndex* lumpIndexForFileType(filetype_t fileType)
    {
        switch(fileType)
        {
        case FT_ZIPFILE:    return &zipLumpIndex;
        case FT_LUMPFILE:
        case FT_WADFILE:    return ActiveWadLumpIndex;
        default: return NULL;
        }
    }

    void usePrimaryWadLumpIndex()
    {
        ActiveWadLumpIndex = &primaryWadLumpIndex;
    }

    bool useAuxiliaryWadLumpIndex()
    {
        if(!auxiliaryWadLumpIndexInUse) return false;
        ActiveWadLumpIndex = &auxiliaryWadLumpIndex;
        return true;
    }

    /**
     * Selects which lump index to use, given a logical lump number.
     * The lump number is then translated into range for the selected index.
     * This should be called in all functions that access lumps by logical lump number.
     */
    void selectWadLumpIndex(lumpnum_t& lumpNum)
    {
        if(lumpNum >= AUXILIARY_BASE)
        {
            useAuxiliaryWadLumpIndex();
            lumpNum -= AUXILIARY_BASE;
        }
        else
        {
            usePrimaryWadLumpIndex();
        }
    }
};

FS::FS()
{
    d = new Instance(this);
    DFileBuilder::init();
}

FS::~FS()
{
    closeAuxiliary();
    delete d;

    DFileBuilder::shutdown();
}

void FS::consoleRegister()
{
    C_CMD("dir", "", Dir);
    C_CMD("ls", "", Dir); // Alias
    C_CMD("dir", "s*", Dir);
    C_CMD("ls", "s*", Dir); // Alias

    C_CMD("dump", "s", DumpLump);
    C_CMD("listfiles", "", ListFiles);
    C_CMD("listlumps", "", ListLumps);
}

/**
 * @note Performance is O(n).
 * @return @c iterator pointing to list->end() if not found.
 */
static de::FileList::iterator findListFileByPath(de::FileList& list, char const* path_)
{
    if(list.empty()) return list.end();
    if(!path_ || !path_[0]) return list.end();

    // Transform the path into one we can process.
    AutoStr* path = Str_Set(AutoStr_NewStd(), path_);
    F_FixSlashes(path, path);

    // Perform the search.
    de::FileList::iterator i;
    for(i = list.begin(); i != list.end(); ++i)
    {
        AbstractFile* file = (*i)->file();
        if(!Str_CompareIgnoreCase(file->path(), Str_Text(path)))
        {
            break; // This is the node we are looking for.
        }
    }
    return i;
}

void FS::unlinkFile(AbstractFile* file)
{
    if(!file) return;
    d->releaseFileId(Str_Text(file->path()));
    d->pruneLumpsFromIndexesByFile(*file);
}

bool FS::unloadFile(char const* path, bool permitRequired, bool quiet)
{
    de::FileList::iterator found = findListFileByPath(d->loadedFiles, path);
    if(found == d->loadedFiles.end()) return false;

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
    d->loadedFiles.erase(found);
    deleteFile(hndl);
    return true;
}

bool FS::checkFileId(char const* path)
{
    DENG_ASSERT(path);
    if(!accessFile(path)) return false;

    // Calculate the identifier.
    FileId fileId = FileId::fromPath(path);
    FileIds::iterator place = qLowerBound(d->fileIds.begin(), d->fileIds.end(), fileId);
    if(place != d->fileIds.end() && *place == fileId) return false;

#if _DEBUG
    LOG_VERBOSE("Added FileId %s - \"%s\"") << fileId << F_PrettyPath(path);
#endif

    d->fileIds.insert(place, fileId);
    return true;
}

void FS::resetFileIds()
{
    d->fileIds.clear();
}

void FS::endStartup()
{
    d->loadingForStartup = false;
    d->usePrimaryWadLumpIndex();
}

int FS::unloadListFiles(de::FileList& list, bool nonStartup)
{
    int unloaded = 0;
    // Unload in reverse load order.
    for(int i = list.size() - 1; i >= 0; i--)
    {
        DFile* hndl = list[i];
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
    return unloaded;
}

#if _DEBUG
static void printFileIds(FileIds const& fileIds)
{
    uint idx = 0;
    DENG2_FOR_EACH(i, fileIds, FileIds::const_iterator)
    {
        LOG_MSG("  %u - %s") << idx << *i;
        ++idx;
    }
}
#endif

#if _DEBUG
static void printFileList(de::FileList& list)
{
    for(int i = 0; i < list.size(); ++i)
    {
        DFile* hndl = list[i];
        AbstractFile* file = hndl->file();
        FileId fileId = FileId::fromPath(Str_Text(file->path()));
        LOG_MSG(" %c%d: %s - \"%s\" [handle: %p]")
            << (file->hasStartup()? '*' : ' ') << i
            << fileId << F_PrettyPath(Str_Text(file->path())) << (void*)&hndl;
    }
}
#endif

int FS::reset()
{
#if _DEBUG
    // List all open files with their identifiers.
    VERBOSE(
        Con_Printf("Open files at reset:\n");
        printFileList(d->openFiles);
        Con_Printf("End\n")
    )
#endif

    // Perform non-startup file unloading...
    int unloaded = unloadListFiles(d->loadedFiles, true/*non-startup*/);

#if _DEBUG
    // Sanity check: look for orphaned identifiers.
    if(!d->fileIds.empty())
    {
        LOG_MSG("Warning: Orphan FileIds:");
        printFileIds(d->fileIds);
    }
#endif

    // Reset file IDs so previously seen files can be processed again.
    /// @todo this releases the ID of startup files too but given the
    /// only startup file is doomsday.pk3 which we never attempt to load
    /// again post engine startup, this isn't an immediate problem.
    resetFileIds();

    // Update the dir/WAD translations.
    initLumpDirectoryMappings();
    initPathMap();

    return unloaded;
}

bool FS::isValidLumpNum(lumpnum_t absoluteLumpNum)
{
    d->selectWadLumpIndex(absoluteLumpNum); // No longer absolute after this call.
    return d->ActiveWadLumpIndex->isValidIndex(absoluteLumpNum);
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

lumpnum_t FS::lumpNumForName(char const* name, bool silent)
{
    lumpnum_t lumpNum = -1;

    if(name && name[0])
    {
        lumpsizecondition_t sizeCond;
        size_t lumpSize = 0, refSize;
        ddstring_t searchPath;

        Str_InitStd(&searchPath); Str_Set(&searchPath, name);

        // The name may contain a size condition (==, >=, <=).
        checkSizeConditionInName(&searchPath, &sizeCond, &refSize);

        // Append a .lmp extension if none is specified.
        if(!F_FindFileExtension(name))
        {
            Str_Append(&searchPath, ".lmp");
        }

        // We have to check both the primary and auxiliary lump indexes
        // because we've only got a name and don't know where it is located.
        // Start with the auxiliary lumps because they have precedence.
        if(d->useAuxiliaryWadLumpIndex())
        {
            lumpNum = d->ActiveWadLumpIndex->indexForPath(Str_Text(&searchPath));

            if(lumpNum >= 0 && sizeCond != LSCOND_NONE)
            {
                // Get the size as well for the condition check.
                lumpSize = d->ActiveWadLumpIndex->lumpInfo(lumpNum).size;
            }
        }

        // Found it yet?
        if(lumpNum < 0)
        {
            d->usePrimaryWadLumpIndex();
            lumpNum = d->ActiveWadLumpIndex->indexForPath(Str_Text(&searchPath));

            if(lumpNum >= 0 && sizeCond != LSCOND_NONE)
            {
                // Get the size as well for the condition check.
                lumpSize = d->ActiveWadLumpIndex->lumpInfo(lumpNum).size;
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
                Con_Message("Warning: FS::lumpNumForName: Lump \"%s\" not found.\n", name);
            }
            else
            {
                Con_Message("Warning: FS::lumpNumForName: Lump \"%s\" with size%s%i not found.\n",
                            Str_Text(&searchPath), sizeCond==LSCOND_EQUAL? "==" :
                            sizeCond==LSCOND_GREATER_OR_EQUAL? ">=" : "<=", (int)refSize);
            }
        }

        Str_Free(&searchPath);
    }
    else if(!silent)
    {
        Con_Message("Warning: FS::lumpNumForName: Empty name, returning invalid lumpnum.\n");
    }
    return d->logicalLumpNum(lumpNum);
}

LumpInfo const* FS::lumpInfo(lumpnum_t absoluteLumpNum, int* lumpIdx)
{
    d->selectWadLumpIndex(absoluteLumpNum); // No longer absolute after this call.
    if(!d->ActiveWadLumpIndex->isValidIndex(absoluteLumpNum))
    {
        if(lumpIdx) *lumpIdx = -1;
        return 0;
    }
    LumpInfo const& info = d->ActiveWadLumpIndex->lumpInfo(absoluteLumpNum);
    if(lumpIdx) *lumpIdx = info.lumpIdx;
    return &info;
}

AbstractFile* FS::lumpFile(lumpnum_t absoluteLumpNum, int* lumpIdx)
{
    LumpInfo const* info = lumpInfo(absoluteLumpNum, lumpIdx);
    if(!info) return 0;
    return reinterpret_cast<AbstractFile*>(info->container);
}

char const* FS::lumpName(lumpnum_t absoluteLumpNum)
{
    int lumpIdx;
    AbstractFile* file = lumpFile(absoluteLumpNum, &lumpIdx);
    if(!file) return "";
    PathDirectoryNode const& node = file->lumpDirectoryNode(lumpIdx);
    return Str_Text(node.pathFragment());
}

int FS::lumpCount()
{
    return d->ActiveWadLumpIndex->size();
}

lumpnum_t FS::openAuxiliary(char const* path, size_t baseOffset)
{
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
        return -1;
    }

    DFile* hndl = DFileBuilder::fromNativeFile(*file, baseOffset);

    if(hndl && WadFile::recognise(*hndl))
    {
        if(d->auxiliaryWadLumpIndexInUse)
        {
            closeAuxiliary();
        }
        d->ActiveWadLumpIndex = &d->auxiliaryWadLumpIndex;
        d->auxiliaryWadLumpIndexInUse = true;

        // Prepare the temporary info descriptor.
        LumpInfo info = LumpInfo(lastModified(Str_Text(foundPath)));

        WadFile* wad = new WadFile(*hndl, Str_Text(foundPath), info);
        hndl = DFileBuilder::fromFile(*wad);
        d->openFiles.push_back(hndl); hndl->setList(reinterpret_cast<struct filelist_s*>(&d->openFiles));

        DFile* loadedFilesHndl = DFileBuilder::dup(*hndl);
        d->loadedFiles.push_back(loadedFilesHndl); loadedFilesHndl->setList(reinterpret_cast<struct filelist_s*>(&d->loadedFiles));
        wad->publishLumpsToIndex(*d->ActiveWadLumpIndex);

        Str_Delete(foundPath);

        return Instance::AUXILIARY_BASE;
    }

    if(foundPath) Str_Delete(foundPath);
    if(hndl) delete hndl;
    return -1;
}

void FS::closeAuxiliary()
{
    if(d->useAuxiliaryWadLumpIndex())
    {
        d->clearLoadedFiles(&d->auxiliaryWadLumpIndex);
        d->auxiliaryWadLumpIndexInUse = false;
    }
    d->usePrimaryWadLumpIndex();
}

void FS::releaseFile(AbstractFile* file)
{
    if(!file) return;
    for(int i = d->openFiles.size() - 1; i >= 0; i--)
    {
        DFile* hndl = d->openFiles[i];
        if(hndl->file() == file)
        {
            d->openFiles.removeAt(i);
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

/// @return @c NULL= Not found.
static WadFile* findFirstWadFile(de::FileList& list, bool custom)
{
    if(list.empty()) return 0;
    DENG2_FOR_EACH(i, list, de::FileList::iterator)
    {
        AbstractFile* file = (*i)->file();
        if(custom != file->hasCustom()) continue;

        WadFile* wad = dynamic_cast<WadFile*>(file);
        if(wad) return wad;
    }
    return 0;
}

uint FS::loadedFilesCRC()
{
    /**
     * We define the CRC as that of the lump directory of the first loaded IWAD.
     * @todo Really kludgy...
     */
    WadFile* iwad = findFirstWadFile(d->loadedFiles, false/*not-custom*/);
    if(!iwad) return 0;
    return iwad->calculateCRC();
}

FS::PathList FS::collectLocalPaths(ddstring_t const* searchPath, bool includeSearchPath)
{
    PathList foundEntries;
    finddata_t fd;

    ddstring_t origWildPath; Str_InitStd(&origWildPath);
    Str_Appendf(&origWildPath, "%s*", Str_Text(searchPath));

    ddstring_t wildPath; Str_Init(&wildPath);
    for(int i = -1; i < (int)d->pathMappings.count(); ++i)
    {
        if(i == -1)
        {
            Str_Copy(&wildPath, &origWildPath);
        }
        else
        {
            // Possible mapping?
            Str_Copy(&wildPath, searchPath);
            if(!applyPathMapping(&wildPath, d->pathMappings[i])) continue;

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

FS::PathList FS::collectPaths(bool (*predicate)(de::DFile* hndl, void* parameters), void* parameters)
{
    PathList paths;
    DENG2_FOR_EACH(i, d->loadedFiles, de::FileList::const_iterator)
    {
        // Interested in this file?
        if(predicate && !predicate(*i, parameters)) continue; // Nope.

        AbstractFile* file = (*i)->file();
        paths.push_back(PathListItem(QString(Str_Text(file->path()))));
    }
    return paths;
}

int FS::allResourcePaths(char const* rawSearchPattern, int flags,
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
    DENG2_FOR_EACH(i, d->zipLumpIndex.lumps(), LumpIndex::Lumps::const_iterator)
    {
        LumpInfo const* lumpInfo = *i;
        AbstractFile* container = reinterpret_cast<AbstractFile*>(lumpInfo->container);
        DENG_ASSERT(container);
        PathDirectoryNode const& node = container->lumpDirectoryNode(lumpInfo->lumpIdx);

        AutoStr* filePath = 0;
        bool patternMatched;
        if(!(flags & SPF_NO_DESCEND))
        {
            filePath       = container->composeLumpPath(lumpInfo->lumpIdx);
            patternMatched = F_MatchFileName(Str_Text(filePath), Str_Text(searchPattern));
        }
        else
        {
            patternMatched = node.matchDirectory(PCF_MATCH_FULL, &patternMap);
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
    if(!d->lumpMappings.empty())
    {
        DENG2_FOR_EACH(i, d->lumpMappings, LumpMappings::const_iterator)
        {
            LumpMapping const& found = *i;
            QByteArray foundPathUtf8 = found.first.toUtf8();
            bool patternMatched = F_MatchFileName(foundPathUtf8.constData(), Str_Text(searchPattern));

            if(!patternMatched) continue;

            int result = callback(foundPathUtf8.constData(), PT_LEAF, parameters);
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
            QByteArray foundPathUtf8 = found.path.toUtf8();
            bool patternMatched = F_MatchFileName(foundPathUtf8.constData(), Str_Text(searchPattern));

            if(!patternMatched) continue;

            int result = callback(foundPathUtf8.constData(), (found.attrib & A_SUBDIR)? PT_BRANCH : PT_LEAF, parameters);
            if(result) return result;
        }
    }

    return 0; // Not found.
}

FILE* FS::findRealFile(char const* path, char const* mymode, ddstring_t** foundPath)
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
    if(d->pathMappings.empty())
    {
        Str_Free(&nativePath);
        return 0;
    }

    ddstring_t* mapped = Str_NewStd();
    DENG2_FOR_EACH(i, d->pathMappings, PathMappings::const_iterator)
    {
        Str_Set(mapped, path);
        if(!applyPathMapping(mapped, *i)) continue;
        // The mapping was successful.

        F_ToNativeSlashes(&nativePath, mapped);
        file = fopen(Str_Text(&nativePath), mode);
        if(file) break;
    }

    Str_Free(&nativePath);

    if(file)
    {
        VERBOSE( Con_Message("findRealFile: \"%s\" opened as %s.\n", F_PrettyPath(Str_Text(mapped)), path) )
        if(foundPath) *foundPath = mapped;
    }
    else
    {
        Str_Free(mapped);
    }
    return file;
}

AbstractFile* FS::findLumpFile(char const* path, int* lumpIdx)
{
    if(lumpIdx) *lumpIdx = -1;
    if(!path || !path[0]) return 0;

    /**
     * First check the Zip directory.
     */

    // Convert to an absolute path.
    ddstring_t absSearchPath;
    Str_Init(&absSearchPath); Str_Set(&absSearchPath, path);
    F_PrependBasePath(&absSearchPath, &absSearchPath);

    // Perform the search.
    lumpnum_t lumpNum = d->zipLumpIndex.indexForPath(Str_Text(&absSearchPath));
    if(lumpNum >= 0)
    {
        Str_Free(&absSearchPath);

        LumpInfo const& lumpInfo = d->zipLumpIndex.lumpInfo(lumpNum);
        if(lumpIdx) *lumpIdx = lumpInfo.lumpIdx;
        return reinterpret_cast<AbstractFile*>(lumpInfo.container);
    }

    /**
     * Next try the dir/WAD redirects.
     */
    if(!d->lumpMappings.empty())
    {
        // We must have an absolute path - prepend the CWD if necessary.
        Str_Set(&absSearchPath, path);
        F_PrependWorkPath(&absSearchPath, &absSearchPath);

        DENG2_FOR_EACH(i, d->lumpMappings, LumpMappings::const_iterator)
        {
            LumpMapping const& found = *i;
            QByteArray foundPathUtf8 = found.first.toUtf8();
            if(qstricmp(foundPathUtf8.constData(), Str_Text(&absSearchPath))) continue;

            QByteArray foundLumpNameUtf8 = found.second.toUtf8();
            lumpnum_t absoluteLumpNum = lumpNumForName(foundLumpNameUtf8.constData());
            if(absoluteLumpNum < 0) continue;

            Str_Free(&absSearchPath);
            return lumpFile(absoluteLumpNum, lumpIdx);
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

    // Prepare a the temporary info descriptor.
    LumpInfo info = container->lumpInfo(lumpIdx);

    // Try to open the referenced file as a specialised file type.
    DFile* file = tryOpenFile3(hndl, Str_Text(&absPath), &info);

    // If not opened; assume its a generic LumpFile.
    if(!file)
    {
        LumpFile* lump = new LumpFile(*hndl, Str_Text(&absPath), info);
        file = DFileBuilder::fromFile(*lump);
    }
    DENG_ASSERT(file);

    Str_Free(&absPath);
    return file;
}

static DFile* tryOpenAsZipFile(DFile* hndl, char const* path, LumpInfo const* info)
{
    if(ZipFile::recognise(*hndl))
    {
        ZipFile* zip = new ZipFile(*hndl, path, *info);
        return DFileBuilder::fromFile(*zip);
    }
    return NULL;
}

static DFile* tryOpenAsWadFile(DFile* hndl, char const* path, LumpInfo const* info)
{
    if(WadFile::recognise(*hndl))
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

DFile* FS::tryOpenFile2(char const* path, char const* mode, size_t baseOffset, bool allowDuplicate)
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
        AbstractFile* container = findLumpFile(Str_Text(&searchPath), &lumpIdx);
        if(container)
        {
            // Do not read files twice.
            if(!allowDuplicate && !checkFileId(Str_Text(&searchPath))) return 0;

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
    if(!allowDuplicate && !checkFileId(Str_Text(foundPath)))
    {
        fclose(nativeFile);
        Str_Delete(foundPath);
        Str_Free(&searchPath);
        return NULL;
    }

    // Acquire a handle on the file we intend to open.
    DFile* hndl = DFileBuilder::fromNativeFile(*nativeFile, baseOffset);

    // Prepare the temporary info descriptor.
    LumpInfo info = LumpInfo(lastModified(Str_Text(foundPath)));

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

    Str_Delete(foundPath);
    Str_Free(&searchPath);
    return dfile;
}

DFile* FS::tryOpenFile(char const* path, char const* mode, size_t baseOffset, bool allowDuplicate)
{
    DFile* file = tryOpenFile2(path, mode, baseOffset, allowDuplicate);
    if(file)
    {
        d->openFiles.push_back(file);
        return &file->setList(reinterpret_cast<struct filelist_s*>(&d->openFiles));
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

bool FS::accessFile(char const* path)
{
    DFile* hndl = tryOpenFile(path, "rx", 0, true);
    if(!hndl) return false;
    deleteFile(hndl);
    return true;
}

uint FS::lastModified(char const* fileName)
{
    // Try to open the file, but don't buffer any contents.
    DFile* hndl = tryOpenFile(fileName, "rx", 0, true);
    uint modified = 0;
    if(hndl)
    {
        modified = hndl->file()->lastModified();
        deleteFile(hndl);
    }
    return modified;
}

AbstractFile* FS::addFile(char const* path, size_t baseOffset)
{
    DFile* hndl = openFile(path, "rb", baseOffset, false);
    if(!hndl)
    {
        if(accessFile(path))
        {
            Con_Message("\"%s\" already loaded.\n", F_PrettyPath(path));
        }
        return 0;
    }

    AbstractFile* file = hndl->file();
    VERBOSE( Con_Message("Loading \"%s\"...\n", F_PrettyPath(Str_Text(file->path()))) )

    if(d->loadingForStartup)
    {
        file->setStartup(true);
    }

    DFile* loadedFilesHndl = DFileBuilder::dup(*hndl);
    d->loadedFiles.push_back(loadedFilesHndl); loadedFilesHndl->setList(reinterpret_cast<struct filelist_s*>(&d->loadedFiles));

    // Publish lumps to an index?
    LumpIndex* lumpIndex = d->lumpIndexForFileType(file->type());
    if(lumpIndex)
    {
        file->publishLumpsToIndex(*lumpIndex);
    }
    return file;
}

int FS::addFiles(char const* const* paths, int num)
{
    if(!paths) return 0;

    int addedFileCount = 0;
    for(int i = 0; i < num; ++i)
    {
        if(addFile(paths[i]))
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
    AbstractFile* container = lumpFile(absoluteLumpNum, &lumpIdx);
    if(!container) return 0;

    LumpFile* lump = new LumpFile(*DFileBuilder::fromFileLump(*container, lumpIdx, false),
                                  Str_Text(container->composeLumpPath(lumpIdx)),
                                  container->lumpInfo(lumpIdx));
    if(!lump) return 0;

    DFile* openFileHndl = DFileBuilder::fromFile(*lump);
    d->openFiles.push_back(openFileHndl); openFileHndl->setList(reinterpret_cast<struct filelist_s*>(&d->openFiles));
    return openFileHndl;
}

void FS::addLumpDirectoryMapping(char const* lumpName, char const* symbolicPath)
{
    if(!lumpName || !lumpName[0] || !symbolicPath || !symbolicPath[0]) return;

    // Convert the symbolic path into a real path.
    AutoStr* path = Str_Set(AutoStr_NewStd(), symbolicPath);
    F_ResolveSymbolicPath(path, path);

    // Since the path might be relative, let's explicitly make the path absolute.
    char* full = _fullpath(0, Str_Text(path), 0);
    de::String fullPath = de::String::fromNativePath(full);
    free(full);

    // Have already mapped this path?
    LumpMappings::iterator found = d->lumpMappings.begin();
    for(; found != d->lumpMappings.end(); ++found)
    {
        LumpMapping const& ldm = *found;
        if(!ldm.first.compare(fullPath, Qt::CaseInsensitive))
            break;
    }

    LumpMapping* ldm;
    if(found == d->lumpMappings.end())
    {
        // No. Acquire another mapping.
        d->lumpMappings.push_back(LumpMapping(fullPath, lumpName));
        ldm = &d->lumpMappings.back();
    }
    else
    {
        // Remap to another lump.
        ldm = &*found;
        ldm->second = lumpName;
    }

    QByteArray pathUtf8 = ldm->first.toUtf8();
    LOG_VERBOSE("Path \"%s\" now mapped to lump \"%s\"") << F_PrettyPath(pathUtf8.constData()) << ldm->second;
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
static bool parseLDMappingList(de::FS& fileSystem, char const* buffer)
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
            fileSystem.addLumpDirectoryMapping(lumpName, Str_Text(&path));
        }
    } while(*ch);

    // Success.
    successful = true;

//parseEnded:
    Str_Free(&line);
    Str_Free(&path);
    return successful;
}

void FS::initLumpDirectoryMappings(void)
{
    static bool inited = false;
    size_t bufSize = 0;
    uint8_t* buf = NULL;

    if(inited)
    {
        // Free old paths, if any.
        d->lumpMappings.clear();
    }

    if(DD_IsShuttingDown()) return;

    // Add the contents of all DD_DIREC lumps.
    int numLumps = lumpCount();
    for(lumpnum_t i = 0; i < numLumps; ++i)
    {
        if(strnicmp(lumpName(i), "DD_DIREC", 8)) continue;

        // Make a copy of it so we can ensure it ends in a null.
        LumpInfo const* info = lumpInfo(i);
        size_t lumpLength = info->size;
        if(bufSize < lumpLength + 1)
        {
            bufSize = lumpLength + 1;
            buf = (uint8_t*) M_Realloc(buf, bufSize);
            if(!buf) Con_Error("FS::initLumpDirectoryMappings: Failed on (re)allocation of %lu bytes for temporary read buffer.", (unsigned long) bufSize);
        }

        int lumpIdx;
        AbstractFile* file = lumpFile(i, &lumpIdx);
        DENG_ASSERT(file);
        file->readLump(lumpIdx, buf, 0, lumpLength);
        buf[lumpLength] = 0;
        parseLDMappingList(*this, reinterpret_cast<char const*>(buf));
    }

    if(buf) M_Free(buf);
    inited = true;
}

/// @return  @c true iff the mapping matched the path.
static bool applyPathMapping(ddstring_t* path, PathMapping const& pm)
{
    if(!path) return false;
    QByteArray destUtf8 = pm.first.toUtf8();
    AutoStr* dest = Str_Set(AutoStr_NewStd(), destUtf8.constData());
    if(qstrnicmp(Str_Text(path), Str_Text(dest), Str_Length(dest))) return false;

    // Replace the beginning with the source path.
    QByteArray sourceUtf8 = pm.second.toUtf8();
    AutoStr* temp = Str_Set(AutoStr_NewStd(), sourceUtf8.constData());
    Str_PartAppend(temp, Str_Text(path), pm.first.length(), Str_Length(path) - pm.first.length());
    Str_Copy(path, temp);
    return true;
}

void FS::mapPath(char const* source, char const* destination)
{
    if(!source || !source[0] || !destination || !destination[0]) return;

    AutoStr* dest = Str_Set(AutoStr_NewStd(), destination);
    Str_Strip(dest);
    F_FixSlashes(dest, dest);
    F_ExpandBasePath(dest, dest);
    F_PrependWorkPath(dest, dest);
    F_AppendMissingSlash(dest);

    AutoStr* src = Str_Set(AutoStr_NewStd(), source);
    Str_Strip(src);
    F_FixSlashes(src, src);
    F_ExpandBasePath(src, src);
    F_PrependWorkPath(src, src);
    F_AppendMissingSlash(src);

    // Have already mapped this source path?
    PathMappings::iterator found = d->pathMappings.begin();
    for(; found != d->pathMappings.end(); ++found)
    {
        PathMapping const& pm = *found;
        if(!pm.second.compare(Str_Text(src), Qt::CaseInsensitive))
            break;
    }

    PathMapping* pm;
    if(found == d->pathMappings.end())
    {
        // No. Acquire another mapping.
        d->pathMappings.push_back(PathMapping(Str_Text(dest), Str_Text(src)));
        pm = &d->pathMappings.back();
    }
    else
    {
        // Remap to another destination.
        pm = &*found;
        pm->first = Str_Text(dest);
    }

    QByteArray sourceUtf8 = pm->second.toUtf8();
    QByteArray destUtf8   = pm->first.toUtf8();
    LOG_VERBOSE("Path \"%s\" now mapped to \"%s\"")
            << F_PrettyPath(sourceUtf8.constData()) << F_PrettyPath(destUtf8.constData());
}

void FS::initPathMap()
{
    d->pathMappings.clear();

    if(DD_IsShuttingDown()) return;

    // Create virtual directory mappings by processing all -vdmap options.
    int argC = CommandLine_Count();
    for(int i = 0; i < argC; ++i)
    {
        if(strnicmp("-vdmap", CommandLine_At(i), 6)) continue;

        if(i < argC - 1 && !CommandLine_IsOption(i + 1) && !CommandLine_IsOption(i + 2))
        {
            mapPath(CommandLine_PathAt(i + 1), CommandLine_At(i + 2));
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
static int printResourcePath(char const* fileName, PathDirectoryNodeType /*type*/,
                             void* /*parameters*/)
{
    bool makePretty = CPP_BOOL( F_IsRelativeToBase(fileName, ddBasePath) );
    Con_Printf("  %s\n", makePretty? F_PrettyPath(fileName) : fileName);
    return 0; // Continue the listing.
}

void FS::printDirectory(ddstring_t const* path)
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
    allResourcePaths(Str_Text(&dir), 0, printResourcePath);

    Str_Free(&dir);
}

static void printLumpIndex(de::LumpIndex const& index)
{
    const int numRecords = index.size();
    const int numIndexDigits = MAX_OF(3, M_NumDigits(numRecords));

    Con_Printf("LumpIndex %p (%i records):\n", &index, numRecords);

    int idx = 0;
    DENG2_FOR_EACH(i, index.lumps(), de::LumpIndex::Lumps::const_iterator)
    {
        LumpInfo const* lumpInfo = *i;
        AbstractFile* container = reinterpret_cast<AbstractFile*>(lumpInfo->container);
        Con_Printf("%0*i - \"%s:%s\" (size: %lu bytes%s)\n", numIndexDigits, idx++,
                   F_PrettyPath(Str_Text(container->path())),
                   F_PrettyPath(Str_Text(container->composeLumpPath(lumpInfo->lumpIdx))),
                   (unsigned long) lumpInfo->size,
                   (lumpInfo->isCompressed()? " compressed" : ""));
    }
    Con_Printf("---End of lumps---\n");
}

void FS::printIndex()
{
    // Always the primary index.
    printLumpIndex(d->primaryWadLumpIndex);
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
            App_FileSystem()->printDirectory(&path);
        }
    }
    else
    {
        Str_Set(&path, "/");
        App_FileSystem()->printDirectory(&path);
    }
    Str_Free(&path);
    return true;
}

/// Dump a copy of a virtual file to the runtime directory.
D_CMD(DumpLump)
{
    DENG_UNUSED(src);
    DENG_UNUSED(argc);

    if(fileSystem)
    {
        lumpnum_t absoluteLumpNum = App_FileSystem()->lumpNumForName(argv[1]);
        if(absoluteLumpNum >= 0)
        {
            return F_DumpLump(absoluteLumpNum);
        }
        Con_Printf("No such lump.\n");
        return false;
    }
    Con_Printf("WAD module is not presently initialized.\n");
    return false;
}

/// List virtual files inside containers.
D_CMD(ListLumps)
{
    DENG_UNUSED(src);
    DENG_UNUSED(argc);
    DENG_UNUSED(argv);

    if(fileSystem)
    {
        App_FileSystem()->printIndex();
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
AbstractFile** FS::collectFiles(int* count)
{
    FileList& list = d->loadedFiles;
    if(!list.empty())
    {
        AbstractFile** arr = (AbstractFile**) M_Malloc(list.size() * sizeof *arr);
        if(!arr) Con_Error("collectFiles: Failed on allocation of %lu bytes for file list.", (unsigned long) (list.size() * sizeof *arr));

        for(int i = 0; i < list.size(); ++i)
        {
            DFile const* hndl = list[i];
            arr[i] = hndl->file();
        }
        if(count) *count = list.size();
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
    if(fileSystem)
    {
        int fileCount;
        AbstractFile** arr = App_FileSystem()->collectFiles(&fileCount);
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

de::FS* App_FileSystem()
{
    if(!fileSystem) throw de::Error("App_FileSystem", "File system not yet initialized");
    return fileSystem;
}

void F_Register(void)
{
    FS::consoleRegister();
}

void F_Init(void)
{
    DENG_ASSERT(!fileSystem);
    fileSystem = new de::FS();
}

void F_Shutdown(void)
{
    if(!fileSystem) return;
    delete fileSystem; fileSystem = 0;
}

void F_EndStartup(void)
{
    App_FileSystem()->endStartup();
}

int F_Reset(void)
{
    return App_FileSystem()->reset();
}

void F_InitVirtualDirectoryMappings(void)
{
    App_FileSystem()->initPathMap();
}

void F_AddVirtualDirectoryMapping(char const* source, char const* destination)
{
    App_FileSystem()->mapPath(source, destination);
}

void F_InitLumpDirectoryMappings(void)
{
    App_FileSystem()->initLumpDirectoryMappings();
}

void F_AddLumpDirectoryMapping(char const* lumpName, char const* symbolicPath)
{
    App_FileSystem()->addLumpDirectoryMapping(lumpName, symbolicPath);
}

void F_ResetFileIds(void)
{
    App_FileSystem()->resetFileIds();
}

boolean F_CheckFileId(char const* path)
{
    return App_FileSystem()->checkFileId(path);
}

int F_LumpCount(void)
{
    return App_FileSystem()->lumpCount();
}

int F_Access(char const* path)
{
    return App_FileSystem()->accessFile(path)? 1 : 0;
}

uint F_GetLastModified(char const* path)
{
    return App_FileSystem()->lastModified(path);
}

struct abstractfile_s* F_AddFile2(char const* path, size_t baseOffset)
{
    return reinterpret_cast<struct abstractfile_s*>(App_FileSystem()->addFile(path, baseOffset));
}

struct abstractfile_s* F_AddFile(char const* path)
{
    return reinterpret_cast<struct abstractfile_s*>(App_FileSystem()->addFile(path));
}

boolean F_RemoveFile2(char const* path, boolean permitRequired)
{
    return App_FileSystem()->removeFile(path, CPP_BOOL(permitRequired));
}

boolean F_RemoveFile(char const* path)
{
    return App_FileSystem()->removeFile(path);
}

int F_AddFiles(char const* const* paths, int num)
{
    return App_FileSystem()->addFiles(paths, num);
}

int F_RemoveFiles2(char const* const* filenames, int num, boolean permitRequired)
{
    return App_FileSystem()->removeFiles(filenames, num, CPP_BOOL(permitRequired));
}

int F_RemoveFiles(char const* const* filenames, int num)
{
    return App_FileSystem()->removeFiles(filenames, num);
}

void F_ReleaseFile(struct abstractfile_s* file)
{
    App_FileSystem()->releaseFile(reinterpret_cast<AbstractFile*>(file));
}

struct dfile_s* F_Open3(char const* path, char const* mode, size_t baseOffset, boolean allowDuplicate)
{
    return reinterpret_cast<struct dfile_s*>(App_FileSystem()->openFile(path, mode, baseOffset, CPP_BOOL(allowDuplicate)));
}

struct dfile_s* F_Open2(char const* path, char const* mode, size_t baseOffset)
{
    return reinterpret_cast<struct dfile_s*>(App_FileSystem()->openFile(path, mode, baseOffset));
}

struct dfile_s* F_Open(char const* path, char const* mode)
{
    return reinterpret_cast<struct dfile_s*>(App_FileSystem()->openFile(path, mode));
}

struct dfile_s* F_OpenLump(lumpnum_t absoluteLumpNum)
{
    return reinterpret_cast<struct dfile_s*>(App_FileSystem()->openLump(absoluteLumpNum));
}

boolean F_IsValidLumpNum(lumpnum_t absoluteLumpNum)
{
    return App_FileSystem()->isValidLumpNum(absoluteLumpNum);
}

lumpnum_t F_LumpNumForName(char const* name)
{
    return App_FileSystem()->lumpNumForName(name);
}

char const* F_LumpName(lumpnum_t absoluteLumpNum)
{
    return App_FileSystem()->lumpName(absoluteLumpNum);
}

size_t F_LumpLength(lumpnum_t absoluteLumpNum)
{
    return App_FileSystem()->lumpLength(absoluteLumpNum);
}

uint F_LumpLastModified(lumpnum_t absoluteLumpNum)
{
    return App_FileSystem()->lumpLastModified(absoluteLumpNum);
}

void F_Close(struct dfile_s* hndl)
{
    App_FileSystem()->closeFile(reinterpret_cast<DFile*>(hndl));
}

void F_Delete(struct dfile_s* hndl)
{
    App_FileSystem()->deleteFile(reinterpret_cast<DFile*>(hndl));
}

Str const* F_Path(struct abstractfile_s const* file)
{
    if(file) return reinterpret_cast<AbstractFile const*>(file)->path();
    static de::Str zeroLengthString;
    return zeroLengthString;
}

void F_SetCustom(struct abstractfile_s* file, boolean yes)
{
    if(!file) return;
    reinterpret_cast<AbstractFile*>(file)->setCustom(CPP_BOOL(yes));
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
    return reinterpret_cast<struct abstractfile_s*>(App_FileSystem()->lumpFile(absoluteLumpNum, lumpIdx));
}

struct abstractfile_s* F_FindFileForLumpNum(lumpnum_t absoluteLumpNum)
{
    return reinterpret_cast<struct abstractfile_s*>(App_FileSystem()->lumpFile(absoluteLumpNum, 0));
}

char const* F_LumpSourceFile(lumpnum_t absoluteLumpNum)
{
    return App_FileSystem()->lumpFilePath(absoluteLumpNum);
}

boolean F_LumpIsCustom(lumpnum_t absoluteLumpNum)
{
    return App_FileSystem()->lumpFileHasCustom(absoluteLumpNum);
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
 *
 * @return  New string containing a concatenated, possibly delimited set of all
 *      file paths in the list. Ownership of the string passes to the caller who
 *      should ensure to release it with Str_Delete() when no longer needed.
 *      Always returns a valid (but perhaps zero-length) string object.
 */
static Str* composePathListString(de::FS::PathList& paths, int flags = DEFAULT_PATHTOSTRINGFLAGS,
                                  char const* delimiter = " ")
{
    int maxLength, delimiterLength = (delimiter? (int)strlen(delimiter) : 0);
    int n, pLength;
    Str* str, buf;
    char const* p, *ext;

    Str_Init(&buf);

    // Determine the maximum number of characters we'll need.
    maxLength = 0;
    DENG2_FOR_EACH(i, paths, de::FS::PathList::const_iterator)
    {
        if(!(flags & (PTSF_TRANSFORM_EXCLUDE_DIR|PTSF_TRANSFORM_EXCLUDE_EXT)))
        {
            // Caller wants the whole path plus name and extension (if present).
            maxLength += i->path.length();
        }
        else
        {
            QByteArray pathUtf8 = i->path.toUtf8();

            if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
            {
                // Caller does not want the directory hierarchy.
                F_FileNameAndExtension(&buf, pathUtf8.constData());
                p = Str_Text(&buf);
                pLength = Str_Length(&buf);
            }
            else
            {
                p = pathUtf8.constData();
                pLength = pathUtf8.length();
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
            maxLength += 1;
    }
    maxLength += paths.count() * delimiterLength;

    // Composite final string.
    str = Str_New();
    Str_Reserve(str, maxLength);
    n = 0;
    DENG2_FOR_EACH(i, paths, de::FS::PathList::const_iterator)
    {
        QByteArray pathUtf8 = i->path.toUtf8();
        char const* path = pathUtf8.constData();

        if(flags & PTSF_QUOTED)
            Str_AppendChar(str, '"');

        if(!(flags & (PTSF_TRANSFORM_EXCLUDE_DIR|PTSF_TRANSFORM_EXCLUDE_EXT)))
        {
            // Caller wants the whole path plus name and extension (if present).
            Str_Append(str, path);
        }
        else
        {
            if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
            {
                // Caller does not want the directory hierarchy.
                F_FileNameAndExtension(&buf, path);
                p = Str_Text(&buf);
                pLength = Str_Length(&buf);
            }
            else
            {
                p = path;
                pLength = pathUtf8.length();
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
        if(n != paths.count())
            Str_Append(str, delimiter);
    }

    Str_Free(&buf);

    return str;
}

typedef struct {
    filetype_t type; // Only
    bool markedCustom;
} compositepathpredicateparamaters_t;

static bool collectPathsPredicate(DFile* hndl, void* parameters)
{
    DENG_ASSERT(parameters);
    compositepathpredicateparamaters_t* p = (compositepathpredicateparamaters_t*)parameters;
    AbstractFile* file = hndl->file();
    if((!VALID_FILETYPE(p->type) || p->type == file->type()) &&
       ((p->markedCustom == file->hasCustom())))
    {
        Str const* path = file->path();
        if(stricmp(Str_Text(path) + Str_Length(path) - 3, "lmp"))
            return true; // Include this.
    }
    return false; // Not this.
}

/**
 * Compiles a list of file names, separated by @a delimiter.
 */
void F_ComposeFileList(filetype_t type, boolean markedCustom, char* outBuf, size_t outBufSize, const char* delimiter)
{
    if(!outBuf || 0 == outBufSize) return;
    memset(outBuf, 0, outBufSize);

    compositepathpredicateparamaters_t p = { type, CPP_BOOL(markedCustom) };
    de::FS::PathList paths = App_FileSystem()->collectPaths(collectPathsPredicate, (void*)&p);

    ddstring_t* str = composePathListString(paths, PTSF_TRANSFORM_EXCLUDE_DIR, delimiter);
    strncpy(outBuf, Str_Text(str), outBufSize);
    Str_Delete(str);
}

int F_AllResourcePaths2(char const* searchPath, int flags, int (*callback) (char const* path, PathDirectoryNodeType type, void* parameters), void* parameters)
{
    return App_FileSystem()->allResourcePaths(searchPath, flags, callback, parameters);
}

int F_AllResourcePaths(char const* searchPath, int flags, int (*callback) (char const* path, PathDirectoryNodeType type, void* parameters)/*, parameters = 0 */)
{
    return App_FileSystem()->allResourcePaths(searchPath, flags, callback);
}

uint F_CRCNumber(void)
{
    return App_FileSystem()->loadedFilesCRC();
}

lumpnum_t F_OpenAuxiliary2(char const* path, size_t baseOffset)
{
    return App_FileSystem()->openAuxiliary(path, baseOffset);
}

lumpnum_t F_OpenAuxiliary(char const* path)
{
    return App_FileSystem()->openAuxiliary(path);
}

void F_CloseAuxiliary(void)
{
    App_FileSystem()->closeAuxiliary();
}
