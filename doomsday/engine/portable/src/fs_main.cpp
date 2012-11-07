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
#include <ctime>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "file.h"
#include "fileid.h"
#include "fileinfo.h"
#include "game.h"
#include "lumpindex.h"
#include "resourcenamespace.h"
#include "wad.h"
#include "zip.h"

#include <QList>
#include <QtAlgorithms>

#include <de/Log>
#include <de/memory.h>

using namespace de;

D_CMD(Dir);
D_CMD(DumpLump);
D_CMD(ListFiles);
D_CMD(ListLumps);

static FS1* fileSystem;

struct FileInterpreter
{
    resourcetype_t resourceType;
    de::File1* (*interpret)(de::FileHandle& hndl, char const* path, FileInfo const& info);
};

static de::File1* interpretAsZipFile(de::FileHandle& hndl, char const* path, FileInfo const& info)
{
    if(!Zip::recognise(hndl)) return 0;
    LOG_VERBOSE("Interpreted \"") << F_PrettyPath(path) << "\" as a Zip (archive)";
    return new Zip(hndl, path, info);
}

static de::File1* interpretAsWadFile(de::FileHandle& hndl, char const* path, FileInfo const& info)
{
    if(!Wad::recognise(hndl)) return 0;
    LOG_VERBOSE("Interpreted \"") << F_PrettyPath(path) << "\" as a Wad (archive)";
    return new Wad(hndl, path, info);
}

/// @todo Order here should be determined by the resource locator.
static FileInterpreter interpreters[] = {
    { RT_ZIP,  interpretAsZipFile },
    { RT_WAD,  interpretAsWadFile },
    { RT_NONE, NULL }
};

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

struct FS1::Instance
{
    /// Base for indicies in the auxiliary lump index.
    static int const AUXILIARY_BASE = 100000000;

    FS1* self;

    /// @c true= Flag newly opened files as "startup".
    bool loadingForStartup;

    /// List of currently opened files.
    FileList openFiles;

    /// List of all loaded files present in the file system.
    FileList loadedFiles;

    /// Database of unique identifiers for all loaded/opened files.
    FileIds fileIds;

    LumpIndex zipFileIndex;

    LumpIndex primaryIndex;
    LumpIndex auxiliaryPrimaryIndex;
    // @c true = one or more files have been opened using the auxiliary index.
    bool auxiliaryPrimaryIndexInUse;

    // Currently selected primary index.
    LumpIndex* activePrimaryIndex;

    /// Virtual (file) path => Lump name mapping.
    LumpMappings lumpMappings;

    /// Virtual file-directory mapping.
    PathMappings pathMappings;

    Instance(FS1* d) : self(d), loadingForStartup(true),
        openFiles(), loadedFiles(), fileIds(),
        zipFileIndex(LIF_UNIQUE_PATHS), primaryIndex(),
        auxiliaryPrimaryIndex(), auxiliaryPrimaryIndexInUse(false),
        activePrimaryIndex(&primaryIndex)
    {}

    ~Instance()
    {
        clearLoadedFiles();
        clearOpenFiles();
        clearIndexes();

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
                LOG_VERBOSE("Released FileId %s - \"%s\"") << *place << fileId.path();
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
            File1& file = loadedFiles[i]->file();
            if(!index || index->catalogues(file))
            {
                self->deindex(file);
                delete &file;
            }
        }
    }

    void clearOpenFiles()
    {
        while(!openFiles.empty())
        { delete openFiles.back(); }
    }

    /**
     * Handles conversion to a logical index that is independent of the lump index currently in use.
     */
    inline lumpnum_t logicalLumpNum(lumpnum_t lumpNum)
    {
        return (lumpNum < 0 ? -1 :
                activePrimaryIndex == &auxiliaryPrimaryIndex? lumpNum += AUXILIARY_BASE : lumpNum);
    }

    void clearIndexes()
    {
        primaryIndex.clear();
        auxiliaryPrimaryIndex.clear();
        zipFileIndex.clear();

        activePrimaryIndex = 0;
    }

    void usePrimaryIndex()
    {
        activePrimaryIndex = &primaryIndex;
    }

    bool useAuxiliaryPrimaryIndex()
    {
        if(!auxiliaryPrimaryIndexInUse) return false;
        activePrimaryIndex = &auxiliaryPrimaryIndex;
        return true;
    }

    /**
     * Selects which lump index to use, given a logical lump number.
     * The lump number is then translated into range for the selected index.
     * This should be called in all functions that access lumps by logical lump number.
     */
    void selectPrimaryIndex(lumpnum_t& lumpNum)
    {
        if(lumpNum >= AUXILIARY_BASE)
        {
            useAuxiliaryPrimaryIndex();
            lumpNum -= AUXILIARY_BASE;
        }
        else
        {
            usePrimaryIndex();
        }
    }

    de::FileHandle* tryOpenLump(char const* path, char const* /*mode*/, size_t /*baseOffset*/,
                                bool allowDuplicate, FileInfo& retInfo)
    {
        DENG_ASSERT(path && path[0]);

        // Convert to an absolute path.
        AutoStr* absSearchPath = AutoStr_FromTextStd(path);
        F_PrependBasePath(absSearchPath, absSearchPath);

        File1 const* found = 0;

        // First check the Zip lump index.
        lumpnum_t lumpNum = zipFileIndex.indexForPath(Str_Text(absSearchPath));
        if(lumpNum >= 0)
        {
            found = &zipFileIndex.lump(lumpNum);
        }
        // Nope. Any applicable dir/WAD redirects?
        else if(!lumpMappings.empty())
        {
            // We must have an absolute path - prepend the CWD if necessary.
            Str_Set(absSearchPath, path);
            F_PrependWorkPath(absSearchPath, absSearchPath);

            DENG2_FOR_EACH_CONST(LumpMappings, i, lumpMappings)
            {
                LumpMapping const& mapping = *i;
                QByteArray foundPathUtf8 = mapping.first.toUtf8();
                if(qstricmp(foundPathUtf8.constData(), Str_Text(absSearchPath))) continue;

                QByteArray foundLumpNameUtf8 = mapping.second.toUtf8();
                lumpnum_t absoluteLumpNum = self->lumpNumForName(foundLumpNameUtf8.constData());
                if(absoluteLumpNum < 0) continue;

                found = &self->nameIndexForLump(absoluteLumpNum).lump(absoluteLumpNum);
            }
        }

        // Nothing?
        if(!found) return 0;

        // Do not read files twice.
        if(!allowDuplicate && !self->checkFileId(path)) return 0;

        // Get a handle to the lump we intend to open.
        /// @todo The way this buffering works is nonsensical it should not be done here
        ///        but should instead be deferred until the content of the lump is read.
        FileHandle* hndl = FileHandleBuilder::fromFileLump(found->container(), found->info().lumpIdx, false/*dontBuffer*/);

        // Prepare a temporary info descriptor.
        retInfo = found->info();

        return hndl;
    }

    de::FileHandle* tryOpenNativeFile(char const* path, char const* mymode, size_t baseOffset,
                                      bool allowDuplicate, FileInfo& info)
    {
        DENG_ASSERT(path && path[0]);

        // Translate mymode to the C-lib's fopen() mode specifiers.
        char mode[8] = "";
        if(strchr(mymode, 'r'))      strcat(mode, "r");
        else if(strchr(mymode, 'w')) strcat(mode, "w");
        if(strchr(mymode, 'b'))      strcat(mode, "b");
        else if(strchr(mymode, 't')) strcat(mode, "t");

        AutoStr* nativePath = AutoStr_FromTextStd(path);
        F_ExpandBasePath(nativePath, nativePath);
        // We must have an absolute path - prepend the CWD if necessary.
        F_PrependWorkPath(nativePath, nativePath);
        F_ToNativeSlashes(nativePath, nativePath);

        AutoStr* foundPath = 0;

        // First try a real native file at this absolute path.
        FILE* nativeFile = fopen(Str_Text(nativePath), mode);
        if(nativeFile)
        {
            foundPath = nativePath;
        }
        // Nope. Any applicable virtual directory mappings?
        else if(!pathMappings.empty())
        {
            AutoStr* mapped = AutoStr_NewStd();
            DENG2_FOR_EACH_CONST(PathMappings, i, pathMappings)
            {
                Str_Set(mapped, path);
                if(!applyPathMapping(mapped, *i)) continue;
                // The mapping was successful.

                F_ToNativeSlashes(nativePath, mapped);
                nativeFile = fopen(Str_Text(nativePath), mode);
                if(nativeFile)
                {
                    LOG_DEBUG("FS::tryOpenNativeFile: \"%s\" opened as %s.")
                        << F_PrettyPath(Str_Text(mapped)) << path;
                    foundPath = mapped;
                    break;
                }
            }
        }

        // Nothing?
        if(!nativeFile) return 0;

        // Do not read files twice.
        if(!allowDuplicate && !self->checkFileId(Str_Text(foundPath)))
        {
            fclose(nativeFile);
            return 0;
        }

        // Acquire a handle on the file we intend to open.
        FileHandle* hndl = FileHandleBuilder::fromNativeFile(*nativeFile, baseOffset);

        // Prepare the temporary info descriptor.
        info = FileInfo(F_GetLastModified(Str_Text(foundPath)));

        return hndl;
    }

    de::File1* tryOpenFile(char const* path, char const* mode, size_t baseOffset,
                           bool allowDuplicate)
    {
        if(!path || !path[0]) return 0;
        if(!mode) mode = "";

        bool const reqNativeFile = !!strchr(mode, 'f');

        // Make it a full path.
        AutoStr* searchPath = AutoStr_FromTextStd(path);
        F_FixSlashes(searchPath, searchPath);
        F_ExpandBasePath(searchPath, searchPath);

        DEBUG_VERBOSE2_Message(("FS1::tryOpenFile: trying to open %s\n", Str_Text(searchPath)));

        FileHandle* hndl = 0;
        FileInfo info; // The temporary info descriptor.

        // First check for lumps?
        if(!reqNativeFile)
        {
            hndl = tryOpenLump(Str_Text(searchPath), mode, baseOffset, allowDuplicate, info);
        }

        // Not found? - try a native file.
        if(!hndl)
        {
            hndl = tryOpenNativeFile(Str_Text(searchPath), mode, baseOffset, allowDuplicate, info);
        }

        // Nothing?
        if(!hndl) return 0;

        // Search path is used here rather than found path as the latter may have
        // been mapped to another location. We want the file to be attributed with
        // the path it is to be known by throughout the virtual file system.

        File1& file = self->interpret(*hndl, Str_Text(searchPath), info);

        if(loadingForStartup)
        {
            file.setStartup(true);
        }

        return &file;
    }
};

FS1::FS1()
{
    d = new Instance(this);
    FileHandleBuilder::init();
}

FS1::~FS1()
{
    closeAuxiliaryPrimaryIndex();
    delete d;

    FileHandleBuilder::shutdown();
}

void FS1::consoleRegister()
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
static FS1::FileList::iterator findListFile(FS1::FileList& list, de::File1& file)
{
    if(list.empty()) return list.end();
    // Perform the search.
    FS1::FileList::iterator i;
    for(i = list.begin(); i != list.end(); ++i)
    {
        if(&file == &(*i)->file())
        {
            break; // This is the node we are looking for.
        }
    }
    return i;
}

/**
 * @note Performance is O(n).
 * @return @c iterator pointing to list->end() if not found.
 */
static FS1::FileList::iterator findListFileByPath(FS1::FileList& list, char const* path_)
{
    if(list.empty()) return list.end();
    if(!path_ || !path_[0]) return list.end();

    // Transform the path into one we can process.
    AutoStr* path = AutoStr_FromTextStd(path_);
    F_FixSlashes(path, path);

    // Perform the search.
    FS1::FileList::iterator i;
    for(i = list.begin(); i != list.end(); ++i)
    {
        de::File1& file = (*i)->file();
        if(!Str_CompareIgnoreCase(file.composePath(), Str_Text(path)))
        {
            break; // This is the node we are looking for.
        }
    }
    return i;
}

FS1& FS1::index(de::File1& file)
{
#ifdef DENG_DEBUG
    // Ensure this hasn't yet been indexed.
    FileList::const_iterator found = findListFile(d->loadedFiles, file);
    if(found != d->loadedFiles.end())
        throw de::Error("FS1::index", "File \"" + String(Str_Text(file.composePath())) + "\" has already been indexed");
#endif

    // Publish lumps to one or more indexes?
    if(Zip* zip = dynamic_cast<Zip*>(&file))
    {
        if(!zip->empty())
        {
            // Insert the lumps into their rightful places in the index.
            d->activePrimaryIndex->catalogLumps(*zip, 0, zip->lumpCount());

            // Zip files also go into a special ZipFile index as well.
            d->zipFileIndex.catalogLumps(*zip, 0, zip->lumpCount());
        }
    }
    else if(Wad* wad = dynamic_cast<Wad*>(&file))
    {
        if(!wad->empty())
        {
            // Insert the lumps into their rightful places in the index.
            d->activePrimaryIndex->catalogLumps(*wad, 0, wad->lumpCount());
        }
    }

    // Add a handle to the loaded files list.
    FileHandle* loadedFilesHndl = FileHandleBuilder::fromFile(file);
    d->loadedFiles.push_back(loadedFilesHndl); loadedFilesHndl->setList(reinterpret_cast<struct filelist_s*>(&d->loadedFiles));

    return *this;
}

FS1& FS1::deindex(de::File1& file)
{
    FileList::iterator found = findListFile(d->loadedFiles, file);
    if(found == d->loadedFiles.end()) return *this; // Most peculiar..

    d->releaseFileId(Str_Text(file.composePath()));

    d->zipFileIndex.pruneByFile(file);
    d->primaryIndex.pruneByFile(file);
    if(d->auxiliaryPrimaryIndexInUse)
        d->auxiliaryPrimaryIndex.pruneByFile(file);

    d->loadedFiles.erase(found);
    delete *found;

    return *this;
}

de::File1& FS1::find(char const* path)
{
    // Convert to an absolute path.
    AutoStr* absolutePath = AutoStr_FromTextStd(path);
    F_PrependBasePath(absolutePath, absolutePath);

    FileList::iterator found = findListFileByPath(d->loadedFiles, Str_Text(absolutePath));
    if(found == d->loadedFiles.end()) throw NotFoundError("FS1::findFile", "No files found matching '" + QString(path) + "'");
    DENG_ASSERT((*found)->hasFile());
    return (*found)->file();
}

#if _DEBUG
static void printFileIds(FileIds const& fileIds)
{
    uint idx = 0;
    DENG2_FOR_EACH_CONST(FileIds, i, fileIds)
    {
        LOG_MSG("  %u - %s : \"%s\"") << idx << *i << i->path();
        ++idx;
    }
}
#endif

#if _DEBUG
static void printFileList(FS1::FileList& list)
{
    uint idx = 0;
    DENG2_FOR_EACH_CONST(FS1::FileList, i, list)
    {
        de::FileHandle* hndl = *i;
        de::File1& file = hndl->file();
        FileId fileId = FileId::fromPath(Str_Text(file.composePath()));
        LOG_MSG(" %c%d: %s - \"%s\" (handle: %p)")
            << (file.hasStartup()? '*' : ' ') << idx
            << fileId << fileId.path() << (void*)&hndl;
        ++idx;
    }
}
#endif

FS1& FS1::unloadAllNonStartupFiles(int* retNumUnloaded)
{
#if _DEBUG
    // List all open files with their identifiers.
    if(verbose)
    {
        LOG_MSG("Open files at reset:");
        printFileList(d->openFiles);
        LOG_MSG("End\n");
    }
#endif

    // Perform non-startup file unloading (in reverse load order).
    int numUnloadedFiles = 0;
    for(int i = d->loadedFiles.size() - 1; i >= 0; i--)
    {
        File1& file = d->loadedFiles[i]->file();
        if(file.hasStartup()) continue;

        deindex(file);
        delete &file;
        numUnloadedFiles += 1;
    }

#if _DEBUG
    // Sanity check: look for orphaned identifiers.
    if(!d->fileIds.empty())
    {
        LOG_MSG("Warning: Orphan FileIds:");
        printFileIds(d->fileIds);
    }
#endif

    if(retNumUnloaded) *retNumUnloaded = numUnloadedFiles;
    return *this;
}

bool FS1::checkFileId(char const* path)
{
    DENG_ASSERT(path);
    if(!accessFile(path)) return false;

    // Calculate the identifier.
    FileId fileId = FileId::fromPath(path);
    FileIds::iterator place = qLowerBound(d->fileIds.begin(), d->fileIds.end(), fileId);
    if(place != d->fileIds.end() && *place == fileId) return false;

#if _DEBUG
    LOG_VERBOSE("Added FileId %s - \"%s\"") << fileId << fileId.path();
#endif

    d->fileIds.insert(place, fileId);
    return true;
}

void FS1::resetFileIds()
{
    d->fileIds.clear();
}

void FS1::endStartup()
{
    d->loadingForStartup = false;
    d->usePrimaryIndex();
}

LumpIndex const& FS1::nameIndex() const
{
    DENG_ASSERT(d->activePrimaryIndex);
    return *d->activePrimaryIndex;
}

LumpIndex const& FS1::nameIndexForLump(lumpnum_t& absoluteLumpNum) const
{
    if(absoluteLumpNum >= Instance::AUXILIARY_BASE)
    {
        absoluteLumpNum -= Instance::AUXILIARY_BASE;
        return d->auxiliaryPrimaryIndex;
    }
    return d->primaryIndex;
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

lumpnum_t FS1::lumpNumForName(char const* name, bool silent)
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
        if(d->useAuxiliaryPrimaryIndex())
        {
            lumpNum = d->activePrimaryIndex->indexForPath(Str_Text(&searchPath));

            if(lumpNum >= 0 && sizeCond != LSCOND_NONE)
            {
                // Get the size as well for the condition check.
                lumpSize = d->activePrimaryIndex->lump(lumpNum).info().size;
            }
        }

        // Found it yet?
        if(lumpNum < 0)
        {
            d->usePrimaryIndex();
            lumpNum = d->activePrimaryIndex->indexForPath(Str_Text(&searchPath));

            if(lumpNum >= 0 && sizeCond != LSCOND_NONE)
            {
                // Get the size as well for the condition check.
                lumpSize = d->activePrimaryIndex->lump(lumpNum).info().size;
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
                Con_Message("Warning: FS1::lumpNumForName: Lump \"%s\" not found.\n", name);
            }
            else
            {
                Con_Message("Warning: FS1::lumpNumForName: Lump \"%s\" with size%s%i not found.\n",
                            Str_Text(&searchPath), sizeCond==LSCOND_EQUAL? "==" :
                            sizeCond==LSCOND_GREATER_OR_EQUAL? ">=" : "<=", (int)refSize);
            }
        }

        Str_Free(&searchPath);
    }
    else if(!silent)
    {
        Con_Message("Warning: FS1::lumpNumForName: Empty name, returning invalid lumpnum.\n");
    }
    return d->logicalLumpNum(lumpNum);
}

lumpnum_t FS1::openAuxiliary(char const* filePath, size_t baseOffset)
{
    /// @todo Allow opening Zip files too.
    File1* file = d->tryOpenFile(filePath, "rb", baseOffset, true /*allow duplicates*/);
    if(Wad* wad = dynamic_cast<Wad*>(file))
    {
        // Add a handle to the open files list.
        FileHandle* openFilesHndl = FileHandleBuilder::fromFile(*wad);
        d->openFiles.push_back(openFilesHndl); openFilesHndl->setList(reinterpret_cast<struct filelist_s*>(&d->openFiles));

        // Select the auxiliary index.
        if(d->auxiliaryPrimaryIndexInUse)
        {
            closeAuxiliaryPrimaryIndex();
        }
        d->activePrimaryIndex = &d->auxiliaryPrimaryIndex;
        d->auxiliaryPrimaryIndexInUse = true;

        // Index this file into the file system.
        index(*wad);

        return Instance::AUXILIARY_BASE;
    }

    delete file;
    return -1;
}

void FS1::closeAuxiliaryPrimaryIndex()
{
    if(d->useAuxiliaryPrimaryIndex())
    {
        d->clearLoadedFiles(&d->auxiliaryPrimaryIndex);
        d->auxiliaryPrimaryIndexInUse = false;
    }
    d->usePrimaryIndex();
}

void FS1::releaseFile(de::File1& file)
{
    for(int i = d->openFiles.size() - 1; i >= 0; i--)
    {
        FileHandle& hndl = *(d->openFiles[i]);
        if(&hndl.file() == &file)
        {
            d->openFiles.removeAt(i);
        }
    }
}

/// @return @c NULL= Not found.
static Wad* findFirstWadFile(FS1::FileList& list, bool custom)
{
    if(list.empty()) return 0;
    DENG2_FOR_EACH(FS1::FileList, i, list)
    {
        de::File1& file = (*i)->file();
        if(custom != file.hasCustom()) continue;

        Wad* wad = dynamic_cast<Wad*>(&file);
        if(wad) return wad;
    }
    return 0;
}

uint FS1::loadedFilesCRC()
{
    /**
     * We define the CRC as that of the lump directory of the first loaded IWAD.
     * @todo Really kludgy...
     */
    Wad* iwad = findFirstWadFile(d->loadedFiles, false/*not-custom*/);
    if(!iwad) return 0;
    return iwad->calculateCRC();
}

int FS1::findAll(FS1::FileList& found) const
{
    int numFound = 0;
    DENG2_FOR_EACH_CONST(FS1::FileList, i, d->loadedFiles)
    {
        found.push_back(*i);
        numFound += 1;
    }
    return numFound;
}

int FS1::findAll(bool (*predicate)(de::File1& file, void* parameters), void* parameters,
                 FS1::FileList& found) const
{
    int numFound = 0;
    DENG2_FOR_EACH_CONST(FS1::FileList, i, d->loadedFiles)
    {
        // Interested in this file?
        if(predicate && !predicate((*i)->file(), parameters)) continue; // Nope.

        found.push_back(*i);
        numFound += 1;
    }
    return numFound;
}

int FS1::findAllPaths(char const* rawSearchPattern, int flags, FS1::PathList& found)
{
    int const numFoundSoFar = found.count();

    // First normalize the raw search pattern into one we can process.
    AutoStr* searchPattern = AutoStr_NewStd();
    Str_Set(searchPattern, rawSearchPattern);
    Str_Strip(searchPattern);
    F_FixSlashes(searchPattern, searchPattern);
    F_ExpandBasePath(searchPattern, searchPattern);

    // An absolute path is required so resolve relative to the base path.
    // if not already absolute.
    F_PrependBasePath(searchPattern, searchPattern);

    PathMap patternMap = PathMap(PathTree::hashPathFragment, Str_Text(searchPattern));

    /*
     * Check the Zip directory.
     */
    DENG2_FOR_EACH_CONST(LumpIndex::Lumps, i, d->zipFileIndex.lumps())
    {
        File1 const& lump = **i;
        PathTree::Node const& node = lump.directoryNode();

        AutoStr* filePath = 0;
        bool patternMatched;
        if(!(flags & SPF_NO_DESCEND))
        {
            filePath       = lump.composePath();
            patternMatched = F_MatchFileName(Str_Text(filePath), Str_Text(searchPattern));
        }
        else
        {
            patternMatched = node.comparePath(patternMap, PCF_MATCH_FULL);
        }

        if(!patternMatched) continue;

        // Not yet composed the path?
        if(!filePath)
        {
            filePath = lump.composePath();
        }

        found.push_back(PathListItem(Str_Text(filePath), !node.isLeaf()? A_SUBDIR : 0));
    }

    /*
     * Check the dir/WAD direcs.
     */
    if(!d->lumpMappings.empty())
    {
        DENG2_FOR_EACH_CONST(LumpMappings, i, d->lumpMappings)
        {
            if(!F_MatchFileName(i->first.toUtf8().constData(), Str_Text(searchPattern))) continue;

            found.push_back(PathListItem(i->first, 0 /*only filepaths (i.e., leaves) can be mapped to lumps*/));
        }

        /// @todo Shouldn't these be sorted? -ds
    }

    /*
     * Check native paths.
     */
    // Extract the directory path.
    AutoStr* searchDirectory = AutoStr_NewStd();
    F_FileDir(searchDirectory, searchPattern);

    if(!Str_IsEmpty(searchDirectory))
    {
        PathList nativePaths;
        AutoStr* wildPath = AutoStr_NewStd();
        finddata_t fd;

        for(int i = -1; i < (int)d->pathMappings.count(); ++i)
        {
            if(i == -1)
            {
                Str_Clear(wildPath);
                Str_Appendf(wildPath, "%s*", Str_Text(searchDirectory));
            }
            else
            {
                // Possible mapping?
                Str_Copy(wildPath, searchDirectory);
                if(!applyPathMapping(wildPath, d->pathMappings[i])) continue;

                Str_AppendChar(wildPath, '*');
            }

            if(!myfindfirst(Str_Text(wildPath), &fd))
            {
                // First path found.
                do
                {
                    // Ignore relative directory symbolics.
                    if(Str_Compare(&fd.name, ".") && Str_Compare(&fd.name, ".."))
                    {
                        QString foundPath = QString("%1%2").arg(Str_Text(searchDirectory)).arg(Str_Text(&fd.name));
                        if(!F_MatchFileName(foundPath.toUtf8().constData(), Str_Text(searchPattern))) continue;

                        nativePaths.push_back(PathListItem(foundPath, fd.attrib));
                    }
                } while(!myfindnext(&fd));
            }
            myfindend(&fd);
        }

        // Sort the native paths.
        qSort(nativePaths.begin(), nativePaths.end());

        // Add the native paths to the found results.
        found.append(nativePaths);
    }

    return found.count() - numFoundSoFar;
}

de::File1& FS1::interpret(de::FileHandle& hndl, char const* path, FileInfo const& info)
{
    DENG_ASSERT(path && path[0]);

    de::File1* interpretedFile = 0;

    // Firstly try interpreter(s) for guessed resource types.
    resourcetype_t resTypeGuess = F_GuessResourceTypeByName(path);
    if(resTypeGuess != RT_NONE)
    {
        for(FileInterpreter* intper = interpreters; intper->interpret; ++intper)
        {
            // Not applicable for this resource type?
            if(intper->resourceType != resTypeGuess) continue;

            interpretedFile = intper->interpret(hndl, path, info);
            if(interpretedFile) break;
        }
    }

    // If not yet interpreted - try each recognisable format in order.
    if(!interpretedFile)
    {
        for(FileInterpreter* intper = interpreters; intper->interpret; ++intper)
        {
            // Already tried this?
            if(resTypeGuess && intper->resourceType == resTypeGuess) continue;

            interpretedFile = intper->interpret(hndl, path, info);
            if(interpretedFile) break;
        }
    }

    // Still not interpreted?
    if(!interpretedFile)
    {
        // Use a generic file.
        File1* container = (hndl.hasFile() && hndl.file().isContained())? &hndl.file().container() : 0;
        interpretedFile = new File1(hndl, path, info, container);
    }

    DENG_ASSERT(interpretedFile);
    return *interpretedFile;
}

de::FileHandle& FS1::openFile(char const* path, char const* mode, size_t baseOffset, bool allowDuplicate)
{
#if _DEBUG
    DENG_ASSERT(path && mode);
    for(uint i = 0; mode[i]; ++i)
    {
        if(mode[i] != 'r' && mode[i] != 't' && mode[i] != 'b' && mode[i] != 'f')
            Con_Error("FS1::openFile: Unsupported file open-op in mode string %s for path \"%s\"\n", mode, path);
    }
#endif

    File1* file = d->tryOpenFile(path, mode, baseOffset, allowDuplicate);
    if(!file) throw NotFoundError("FS1::openFile", "No files found matching '" + QString(path) + "'");

    // Add a handle to the opened files list.
    FileHandle& openFilesHndl = *FileHandleBuilder::fromFile(*file);
    d->openFiles.push_back(&openFilesHndl); openFilesHndl.setList(reinterpret_cast<struct filelist_s*>(&d->openFiles));
    return openFilesHndl;
}

de::FileHandle& FS1::openLump(de::File1& lump)
{
    // Add a handle to the opened files list.
    FileHandle& openFilesHndl = *FileHandleBuilder::fromFileLump(lump.container(), lump.info().lumpIdx, false/*do buffer*/);
    d->openFiles.push_back(&openFilesHndl); openFilesHndl.setList(reinterpret_cast<struct filelist_s*>(&d->openFiles));
    return openFilesHndl;
}

bool FS1::accessFile(char const* path)
{
    de::File1* file = d->tryOpenFile(path, "rb", 0, true /* allow duplicates */);
    if(!file) return false;
    delete file;
    return true;
}

void FS1::mapPathToLump(char const* symbolicPath, char const* lumpName)
{
    if(!symbolicPath || !symbolicPath[0] || !lumpName || !lumpName[0]) return;

    // Convert the symbolic path into a real path.
    AutoStr* path = AutoStr_FromTextStd(symbolicPath);
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

FS1& FS1::clearPathLumpMappings()
{
    d->lumpMappings.clear();
    return *this;
}

/// @return  @c true iff the mapping matched the path.
static bool applyPathMapping(ddstring_t* path, PathMapping const& pm)
{
    if(!path) return false;
    QByteArray destUtf8 = pm.first.toUtf8();
    AutoStr* dest = AutoStr_FromTextStd(destUtf8.constData());
    if(qstrnicmp(Str_Text(path), Str_Text(dest), Str_Length(dest))) return false;

    // Replace the beginning with the source path.
    QByteArray sourceUtf8 = pm.second.toUtf8();
    AutoStr* temp = AutoStr_FromTextStd(sourceUtf8.constData());
    Str_PartAppend(temp, Str_Text(path), pm.first.length(), Str_Length(path) - pm.first.length());
    Str_Copy(path, temp);
    return true;
}

void FS1::mapPath(char const* source, char const* destination)
{
    if(!source || !source[0] || !destination || !destination[0]) return;

    AutoStr* dest = AutoStr_FromTextStd(destination);
    Str_Strip(dest);
    F_FixSlashes(dest, dest);
    F_ExpandBasePath(dest, dest);
    F_PrependWorkPath(dest, dest);
    F_AppendMissingSlash(dest);

    AutoStr* src = AutoStr_FromTextStd(source);
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

FS1& FS1::clearPathMappings()
{
    d->pathMappings.clear();
    return *this;
}

void FS1::printDirectory(ddstring_t const* path)
{
    AutoStr* searchPattern = AutoStr_FromTextStd(Str_Text(path));

    Str_Strip(searchPattern);
    F_FixSlashes(searchPattern, searchPattern);
    // Ensure it ends in a directory separator character.
    F_AppendMissingSlash(searchPattern);
    if(!F_ExpandBasePath(searchPattern, searchPattern))
        F_PrependBasePath(searchPattern, searchPattern);

    Con_Printf("Directory: %s\n", F_PrettyPath(Str_Text(searchPattern)));

    // Make the pattern.
    Str_AppendChar(searchPattern, '*');

    PathList found;
    if(findAllPaths(Str_Text(searchPattern), 0, found))
    {
        qSort(found.begin(), found.end());

        DENG2_FOR_EACH_CONST(PathList, i, found)
        {
            QByteArray foundPath = i->path.toUtf8();
            bool const makePretty = CPP_BOOL( F_IsRelativeToBase(foundPath, ddBasePath) );

            Con_Printf("  %s\n", makePretty? F_PrettyPath(foundPath.constData()) : foundPath.constData());
        }
    }
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
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    if(fileSystem)
    {
        LumpIndex::print(App_FileSystem()->nameIndex());
        return true;
    }
    Con_Printf("WAD module is not presently initialized.\n");
    return false;
}

/// List presently loaded files in original load order.
D_CMD(ListFiles)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    Con_Printf("Loaded Files (in load order):\n");

    size_t totalFiles = 0, totalPackages = 0;
    if(fileSystem)
    {
        FS1::FileList foundFiles;
        int fileCount = App_FileSystem()->findAll(foundFiles);
        if(!fileCount) return true;

        DENG2_FOR_EACH_CONST(FS1::FileList, i, foundFiles)
        {
            de::File1& file = (*i)->file();
            uint crc = 0;

            int fileCount = file.lumpCount();
            if(de::Wad* wad = dynamic_cast<de::Wad*>(&file))
            {
                crc = (!file.hasCustom()? wad->calculateCRC() : 0);
            }

            Con_Printf("\"%s\" (%i %s%s)", F_PrettyPath(Str_Text(file.composePath())),
                       fileCount, fileCount != 1 ? "files" : "file",
                       (file.hasStartup()? ", startup" : ""));
            if(0 != crc)
                Con_Printf(" [%08x]", crc);
            Con_Printf("\n");

            totalFiles += size_t(fileCount);
            ++totalPackages;
        }
    }
    Con_Printf("Total: %lu files in %lu packages.\n", (unsigned long) totalFiles, (unsigned long)totalPackages);
    return true;
}

/**
 * C Wrapper API
 */

de::FS1* App_FileSystem()
{
    if(!fileSystem) throw de::Error("App_FileSystem", "File system not yet initialized");
    return fileSystem;
}

void F_Register(void)
{
    FS1::consoleRegister();
}

void F_Init(void)
{
    DENG_ASSERT(!fileSystem);
    fileSystem = new de::FS1();
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

void F_UnloadAllNonStartupFiles(int* numUnloaded)
{
    App_FileSystem()->unloadAllNonStartupFiles(numUnloaded);
}

void F_AddVirtualDirectoryMapping(char const* source, char const* destination)
{
    App_FileSystem()->mapPath(source, destination);
}

void F_AddLumpDirectoryMapping(char const* symbolicPath, char const* lumpName)
{
    App_FileSystem()->mapPathToLump(symbolicPath, lumpName);
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
    return App_FileSystem()->nameIndex().size();
}

int F_Access(char const* path)
{
    return App_FileSystem()->accessFile(path)? 1 : 0;
}

void F_Index(struct file1_s* file)
{
    if(!file) return;
    App_FileSystem()->index(*reinterpret_cast<de::File1*>(file));
}

void F_Deindex(struct file1_s* file)
{
    if(!file) return;
    App_FileSystem()->deindex(*reinterpret_cast<de::File1*>(file));
}

void F_ReleaseFile(struct file1_s* file)
{
    if(!file) return;
    App_FileSystem()->releaseFile(*reinterpret_cast<de::File1*>(file));
}

struct filehandle_s* F_Open3(char const* path, char const* mode, size_t baseOffset, boolean allowDuplicate)
{
    try
    {
        return reinterpret_cast<struct filehandle_s*>(&App_FileSystem()->openFile(path, mode, baseOffset, CPP_BOOL(allowDuplicate)));
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.
    return 0;
}

struct filehandle_s* F_Open2(char const* path, char const* mode, size_t baseOffset)
{
    try
    {
        return reinterpret_cast<struct filehandle_s*>(&App_FileSystem()->openFile(path, mode, baseOffset));
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.
    return 0;
}

struct filehandle_s* F_Open(char const* path, char const* mode)
{
    try
    {
        return reinterpret_cast<struct filehandle_s*>(&App_FileSystem()->openFile(path, mode));
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.
    return 0;
}

struct filehandle_s* F_OpenLump(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        de::File1& lump = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum);
        return reinterpret_cast<struct filehandle_s*>(&App_FileSystem()->openLump(lump));
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore error.
    return 0;
}

boolean F_IsValidLumpNum(lumpnum_t absoluteLumpNum)
{
    return App_FileSystem()->nameIndexForLump(absoluteLumpNum).isValidIndex(absoluteLumpNum);
}

lumpnum_t F_LumpNumForName(char const* name)
{
    try
    {
        return App_FileSystem()->lumpNumForName(name);
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.
    return -1;
}

ddstring_t const* F_LumpName(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        return App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum).name();
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore this error.
    static de::Str zeroLengthString;
    return zeroLengthString;
}

size_t F_LumpLength(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        return App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum).size();
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore this error.
    return 0;
}

uint F_LumpLastModified(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        return App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum).lastModified();
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore this error.
    return (uint)time(0);
}

void F_Delete(struct filehandle_s* _hndl)
{
    if(!_hndl) return;
    de::FileHandle& hndl = *reinterpret_cast<de::FileHandle*>(_hndl);
    App_FileSystem()->releaseFile(hndl.file());
    delete &hndl;
}

AutoStr* F_ComposePath(struct file1_s const* file)
{
    if(file) return reinterpret_cast<de::File1 const*>(file)->composePath();
    return AutoStr_NewStd();
}

void F_SetCustom(struct file1_s* file, boolean yes)
{
    if(!file) return;
    reinterpret_cast<de::File1*>(file)->setCustom(CPP_BOOL(yes));
}

size_t F_ReadLump(struct file1_s* _file, int lumpIdx, uint8_t* buffer)
{
    if(!_file) return 0;
    return reinterpret_cast<de::File1*>(_file)->lump(lumpIdx).read(buffer);
}

size_t F_ReadLumpSection(struct file1_s* _file, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    if(!_file) return 0;
    return reinterpret_cast<de::File1*>(_file)->lump(lumpIdx).read(buffer, startOffset, length);
}

uint8_t const* F_CacheLump(struct file1_s* _file, int lumpIdx)
{
    if(!_file) return 0;
    return reinterpret_cast<de::File1*>(_file)->lump(lumpIdx).cache();
}

void F_UnlockLump(struct file1_s* _file, int lumpIdx)
{
    if(!_file) return;
    reinterpret_cast<de::File1*>(_file)->lump(lumpIdx).unlock();
}

struct file1_s* F_FindFileForLumpNum2(lumpnum_t absoluteLumpNum, int* lumpIdx)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        de::File1 const& lump = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum);
        if(lumpIdx) *lumpIdx = lump.info().lumpIdx;
        return reinterpret_cast<struct file1_s*>(&lump.container());
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore error.
    return 0;
}

struct file1_s* F_FindFileForLumpNum(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        de::File1 const& lump = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum);
        return reinterpret_cast<struct file1_s*>(&lump.container());
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore error.
    return 0;
}

AutoStr* F_ComposeLumpFilePath(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        return App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum).container().composePath();
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore this error.
    return AutoStr_NewStd();
}

boolean F_LumpIsCustom(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        de::File1 const& lump = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum);
        return lump.container().hasCustom();
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore this error.
    return false;
}

AutoStr* F_ComposeLumpPath2(struct file1_s* _file, int lumpIdx, char delimiter)
{
    if(!_file) return AutoStr_NewStd();
    return reinterpret_cast<de::File1*>(_file)->lump(lumpIdx).composePath(delimiter);
}

AutoStr* F_ComposeLumpPath(struct file1_s* _file, int lumpIdx)
{
    if(!_file) return AutoStr_NewStd();
    return reinterpret_cast<de::File1*>(_file)->lump(lumpIdx).composePath();
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
static ddstring_t* composeFilePathString(FS1::FileList& files, int flags = DEFAULT_PATHTOSTRINGFLAGS,
                                         char const* delimiter = " ")
{
    int delimiterLength = (delimiter? (int)strlen(delimiter) : 0);
    int pLength;
    char const* p, *ext;

    // Determine the maximum number of characters we'll need.
    int maxLength = 0;
    DENG2_FOR_EACH_CONST(FS1::FileList, i, files)
    {
        de::File1& file = (*i)->file();

        if(!(flags & (PTSF_TRANSFORM_EXCLUDE_DIR|PTSF_TRANSFORM_EXCLUDE_EXT)))
        {
            // Caller wants the whole path plus name and extension (if present).
            maxLength += Str_Length(file.composePath());
        }
        else
        {
            if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
            {
                // Caller does not want the directory hierarchy.
                ddstring_t const* name = file.name();
                p = Str_Text(name);
                pLength = Str_Length(name);
            }
            else
            {
                AutoStr* path = file.composePath();
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
            maxLength += 1;
    }
    maxLength += files.count() * delimiterLength;

    // Composite final string.
    ddstring_t* str = Str_Reserve(Str_NewStd(), maxLength);
    int n = 0;
    DENG2_FOR_EACH_CONST(FS1::FileList, i, files)
    {
        de::File1& file = (*i)->file();

        if(flags & PTSF_QUOTED)
            Str_AppendChar(str, '"');

        if(!(flags & (PTSF_TRANSFORM_EXCLUDE_DIR|PTSF_TRANSFORM_EXCLUDE_EXT)))
        {
            // Caller wants the whole path plus name and extension (if present).
            Str_Append(str, Str_Text(file.composePath()));
        }
        else
        {
            if(flags & PTSF_TRANSFORM_EXCLUDE_DIR)
            {
                // Caller does not want the directory hierarchy.
                ddstring_t const* name = file.name();
                p = Str_Text(name);
                pLength = Str_Length(name);
            }
            else
            {
                AutoStr* path = file.composePath();
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
        if(n != files.count())
            Str_Append(str, delimiter);
    }

    return str;
}

static bool findCustomFilesPredicate(de::File1& file, void* /*parameters*/)
{
    return file.hasCustom();
}

/**
 * Compiles a list of file names, separated by @a delimiter.
 */
void F_ComposePWADFileList(char* outBuf, size_t outBufSize, const char* delimiter)
{
    if(!outBuf || 0 == outBufSize) return;
    memset(outBuf, 0, outBufSize);

    FS1::FileList foundFiles;
    if(!App_FileSystem()->findAll<de::Wad>(findCustomFilesPredicate, 0/*no params*/, foundFiles)) return;

    ddstring_t* str = composeFilePathString(foundFiles, PTSF_TRANSFORM_EXCLUDE_DIR, delimiter);
    strncpy(outBuf, Str_Text(str), outBufSize);
    Str_Delete(str);
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
    App_FileSystem()->closeAuxiliaryPrimaryIndex();
}
