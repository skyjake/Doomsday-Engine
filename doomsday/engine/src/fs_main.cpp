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

#include <ctime>

#include <QDir>
#include <QList>
#include <QtAlgorithms>

#include <de/App>
#include <de/Log>
#include <de/NativePath>
#include <de/memory.h>

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

using namespace de;

D_CMD(Dir);
D_CMD(DumpLump);
D_CMD(ListFiles);
D_CMD(ListLumps);

static FS1* fileSystem;

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
    bool releaseFileId(String path)
    {
        if(!path.isEmpty())
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

    de::FileHandle* tryOpenLump(String path, String const& /*mode*/, size_t /*baseOffset*/,
                                bool allowDuplicate, FileInfo& retInfo)
    {
        if(path.isEmpty()) return 0;

        // We must have an absolute path - prepend the base path if necessary.
        if(QDir::isRelativePath(path))
        {
            String basePath = DENG2_APP->nativeBasePath().withSeparators('/');
            path = basePath / path;
        }

        File1 const* found = 0;

        // First check the Zip lump index.
        QByteArray pathUtf8 = path.toUtf8();
        lumpnum_t lumpNum = zipFileIndex.indexForPath(pathUtf8.constData());
        if(lumpNum >= 0)
        {
            found = &zipFileIndex.lump(lumpNum);
        }
        // Nope. Any applicable dir/WAD redirects?
        else if(!lumpMappings.empty())
        {
            DENG2_FOR_EACH_CONST(LumpMappings, i, lumpMappings)
            {
                LumpMapping const& mapping = *i;
                if(mapping.first.compare(path)) continue;

                lumpnum_t absoluteLumpNum = self->lumpNumForName(mapping.second);
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

    de::FileHandle* tryOpenNativeFile(String path, String const& mymode, size_t baseOffset,
                                      bool allowDuplicate, FileInfo& info)
    {
        DENG_ASSERT(!path.isEmpty());

        // We must have an absolute path - prepend the CWD if necessary.
        if(QDir::isRelativePath(path))
        {
            path = NativePath::workPath() / path;
        }

        // Translate mymode to the C-lib's fopen() mode specifiers.
        char mode[8] = "";
        if(mymode.contains('r'))      strcat(mode, "r");
        else if(mymode.contains('w')) strcat(mode, "w");
        if(mymode.contains('b'))      strcat(mode, "b");
        else if(mymode.contains('t')) strcat(mode, "t");

        // First try a real native file at this absolute path.
        NativePath nativePath = NativePath(path);
        QByteArray nativePathUtf8 = nativePath.toUtf8();
        FILE* nativeFile = fopen(nativePathUtf8.constData(), mode);
        if(!nativeFile)
        {
            // Nope. Any applicable virtual directory mappings?
            if(!pathMappings.empty())
            {
                QByteArray pathUtf8 = path.toUtf8();
                AutoStr* mapped = AutoStr_NewStd();
                DENG2_FOR_EACH_CONST(PathMappings, i, pathMappings)
                {
                    Str_Set(mapped, pathUtf8.constData());
                    if(!applyPathMapping(mapped, *i)) continue;
                    // The mapping was successful.

                    nativePath = NativePath(Str_Text(mapped));
                    nativePathUtf8 = nativePath.toUtf8();
                    nativeFile = fopen(nativePathUtf8.constData(), mode);
                    if(nativeFile) break;
                }
            }
        }

        // Nothing?
        if(!nativeFile) return 0;

        // Do not read files twice.
        if(!allowDuplicate && !self->checkFileId(nativePath.withSeparators('/')))
        {
            fclose(nativeFile);
            return 0;
        }

        // Acquire a handle on the file we intend to open.
        FileHandle* hndl = FileHandleBuilder::fromNativeFile(*nativeFile, baseOffset);

        // Prepare the temporary info descriptor.
        info = FileInfo(F_GetLastModified(nativePathUtf8.constData()));

        return hndl;
    }

    de::File1* tryOpenFile(String path, String const& mode, size_t baseOffset,
                           bool allowDuplicate)
    {
        LOG_AS("FS1::tryOpenFile");

        if(path.isEmpty()) return 0;

        // We must have an absolute path.
        if(QDir::isRelativePath(path))
        {
            String basePath = DENG2_APP->nativeBasePath().withSeparators('/');
            path = basePath / path;
        }

        LOG_TRACE("Trying %s...") << NativePath(path).pretty();

        bool const reqNativeFile = mode.contains('f');

        FileHandle* hndl = 0;
        FileInfo info; // The temporary info descriptor.

        // First check for lumps?
        if(!reqNativeFile)
        {
            hndl = tryOpenLump(path, mode, baseOffset, allowDuplicate, info);
        }

        // Not found? - try a native file.
        if(!hndl)
        {
            hndl = tryOpenNativeFile(path, mode, baseOffset, allowDuplicate, info);
        }

        // Nothing?
        if(!hndl) return 0;

        // Search path is used here rather than found path as the latter may have
        // been mapped to another location. We want the file to be attributed with
        // the path it is to be known by throughout the virtual file system.

        File1& file = self->interpret(*hndl, path, info);

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
static FS1::FileList::iterator findListFileByPath(FS1::FileList& list, String path)
{
    if(list.empty()) return list.end();
    if(path.isEmpty()) return list.end();

    // Perform the search.
    FS1::FileList::iterator i;
    for(i = list.begin(); i != list.end(); ++i)
    {
        de::File1& file = (*i)->file();
        if(!file.composePath().compare(path, Qt::CaseInsensitive))
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
        throw de::Error("FS1::index", "File \"" + NativePath(file.composePath()).pretty() + "\" has already been indexed");
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

    QByteArray path = file.composePath().toUtf8();
    d->releaseFileId(path.constData());

    d->zipFileIndex.pruneByFile(file);
    d->primaryIndex.pruneByFile(file);
    if(d->auxiliaryPrimaryIndexInUse)
        d->auxiliaryPrimaryIndex.pruneByFile(file);

    d->loadedFiles.erase(found);
    delete *found;

    return *this;
}

de::File1& FS1::find(String path)
{
    // Convert to an absolute path.
    if(!QDir::isAbsolutePath(path))
    {
        String basePath = DENG2_APP->nativeBasePath().withSeparators('/');
        path = basePath / path;
    }

    FileList::iterator found = findListFileByPath(d->loadedFiles, path);
    if(found == d->loadedFiles.end()) throw NotFoundError("FS1::findFile", "No files found matching '" + path + "'");
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
        de::FileHandle& hndl = **i;
        de::File1& file = hndl.file();

        QByteArray path = file.composePath().toUtf8();
        FileId fileId = FileId::fromPath(path.constData());

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

bool FS1::checkFileId(String path)
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
static void checkSizeConditionInName(String& name, lumpsizecondition_t* pCond, size_t* pSize)
{
    DENG_ASSERT(pCond != 0);
    DENG_ASSERT(pSize != 0);

    *pCond = LSCOND_NONE;
    *pSize = 0;

    int condPos = -1;
    int argPos  = -1;
    if((condPos = name.indexOf("==")) >= 0)
    {
        *pCond = LSCOND_EQUAL;
        argPos = condPos + 2;
    }
    else if((condPos = name.indexOf(">=")) >= 0)
    {
        *pCond = LSCOND_GREATER_OR_EQUAL;
        argPos = condPos + 2;
    }
    else if((condPos = name.indexOf("<=")) >= 0)
    {
        *pCond = LSCOND_LESS_OR_EQUAL;
        argPos = condPos + 2;
    }

    if(condPos < 0) return;

    // Get the argument.
    *pSize = name.mid(argPos).toULong();

    // Remove it from the name.
    name.truncate(condPos);
}

lumpnum_t FS1::lumpNumForName(String name, bool silent)
{
    LOG_AS("FS1::lumpNumForName");

    lumpnum_t lumpNum = -1;
    if(!name.isEmpty())
    {
        lumpsizecondition_t sizeCond;
        size_t lumpSize = 0, refSize;

        // The name may contain a size condition (==, >=, <=).
        checkSizeConditionInName(name, &sizeCond, &refSize);

        // Append a .lmp extension if none is specified.
        if(name.fileNameExtension().isEmpty())
        {
            name += ".lmp";
        }

        // We have to check both the primary and auxiliary lump indexes
        // because we've only got a name and don't know where it is located.
        // Start with the auxiliary lumps because they have precedence.
        QByteArray nameUtf8 = name.toUtf8();
        if(d->useAuxiliaryPrimaryIndex())
        {
            lumpNum = d->activePrimaryIndex->indexForPath(nameUtf8.constData());

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
            lumpNum = d->activePrimaryIndex->indexForPath(nameUtf8.constData());

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

        default: break;
        }

        // If still not found, warn the user.
        if(!silent && lumpNum < 0)
        {
            if(sizeCond == LSCOND_NONE)
            {
                LOG_WARNING("Lump \"%s\" not found.") << name;
            }
            else
            {
                LOG_WARNING("Lump \"%s\" with size%s%i not found.")
                    << name.fileNameWithoutExtension()
                    << (           sizeCond == LSCOND_EQUAL? "==" :
                        sizeCond == LSCOND_GREATER_OR_EQUAL? ">=" :
                                                             "<=" )
                    << int(refSize);
            }
        }
    }
    else if(!silent)
    {
        LOG_WARNING("Empty name, returning invalid lumpnum.");
    }
    return d->logicalLumpNum(lumpNum);
}

lumpnum_t FS1::openAuxiliary(String path, size_t baseOffset)
{
    /// @todo Allow opening Zip files too.
    File1* file = d->tryOpenFile(path, "rb", baseOffset, true /*allow duplicates*/);
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

int FS1::findAllPaths(String searchPattern, int flags, FS1::PathList& found)
{
    int const numFoundSoFar = found.count();

    // We must have an absolute path - prepend the base path if necessary.
    if(!QDir::isAbsolutePath(searchPattern))
    {
        String basePath = DENG2_APP->nativeBasePath().withSeparators('/');
        searchPattern = basePath / searchPattern;
    }

    de::Uri patternMap = de::Uri(searchPattern, RC_NULL);
    QByteArray searchPatternUtf8 = searchPattern.toUtf8();

    /*
     * Check the Zip directory.
     */
    DENG2_FOR_EACH_CONST(LumpIndex::Lumps, i, d->zipFileIndex.lumps())
    {
        File1 const& lump = **i;
        PathTree::Node const& node = lump.directoryNode();

        String* filePath = 0;
        bool patternMatched;
        if(!(flags & SPF_NO_DESCEND))
        {
            filePath = new String(lump.composePath());
            QByteArray filePathUtf8 = filePath->toUtf8();
            patternMatched = F_MatchFileName(filePathUtf8.constData(), searchPatternUtf8.constData());
        }
        else
        {
            patternMatched = !node.comparePath(patternMap, PCF_MATCH_FULL);
        }

        if(!patternMatched) continue;

        // Not yet composed the path?
        if(!filePath)
        {
            filePath = new String(lump.composePath());
        }

        found.push_back(PathListItem(*filePath, !node.isLeaf()? A_SUBDIR : 0));

        delete filePath;
    }

    /*
     * Check the dir/WAD direcs.
     */
    if(!d->lumpMappings.empty())
    {
        DENG2_FOR_EACH_CONST(LumpMappings, i, d->lumpMappings)
        {
            if(!F_MatchFileName(i->first.toUtf8().constData(), searchPatternUtf8.constData())) continue;

            found.push_back(PathListItem(i->first, 0 /*only filepaths (i.e., leaves) can be mapped to lumps*/));
        }

        /// @todo Shouldn't these be sorted? -ds
    }

    /*
     * Check native paths.
     */
    String searchDirectory = searchPattern.fileNamePath();
    if(!searchDirectory.isEmpty())
    {
        QByteArray searchDirectoryUtf8 = searchDirectory.toUtf8();
        PathList nativeFilePaths;
        AutoStr* wildPath = AutoStr_NewStd();
        Str_Reserve(wildPath, searchDirectory.length() + 2 + 16); // Conservative estimate.

        for(int i = -1; i < (int)d->pathMappings.count(); ++i)
        {
            Str_Clear(wildPath);
            Str_Appendf(wildPath, "%s/", searchDirectoryUtf8.constData());

            if(i > -1)
            {
                // Possible mapping?
                if(!applyPathMapping(wildPath, d->pathMappings[i])) continue;
            }
            Str_AppendChar(wildPath, '*');

            finddata_t fd;
            if(!myfindfirst(Str_Text(wildPath), &fd))
            {
                // First path found.
                do
                {
                    // Ignore relative directory symbolics.
                    if(Str_Compare(&fd.name, ".") && Str_Compare(&fd.name, ".."))
                    {
                        String foundPath = searchDirectory / NativePath(Str_Text(&fd.name)).withSeparators('/');
                        QByteArray foundPathUtf8 = foundPath.toUtf8();
                        if(!F_MatchFileName(foundPathUtf8.constData(), searchPatternUtf8.constData())) continue;

                        nativeFilePaths.push_back(PathListItem(foundPath, fd.attrib));
                    }
                } while(!myfindnext(&fd));
            }
            myfindend(&fd);
        }

        // Sort the native file paths.
        qSort(nativeFilePaths.begin(), nativeFilePaths.end());

        // Add the native file paths to the found results.
        found.append(nativeFilePaths);
    }

    return found.count() - numFoundSoFar;
}

struct FileInterpreter
{
    resourcetype_t resourceType;
    de::File1* (*interpret)(de::FileHandle& hndl, String path, FileInfo const& info);
};

static de::File1* interpretAsZipFile(de::FileHandle& hndl, String path, FileInfo const& info)
{
    if(!Zip::recognise(hndl)) return 0;
    LOG_VERBOSE("Interpreted \"") << NativePath(path).pretty() << "\" as a Zip (archive)";
    return new Zip(hndl, path, info);
}

static de::File1* interpretAsWadFile(de::FileHandle& hndl, String path, FileInfo const& info)
{
    if(!Wad::recognise(hndl)) return 0;
    LOG_VERBOSE("Interpreted \"") << NativePath(path).pretty() << "\" as a Wad (archive)";
    return new Wad(hndl, path, info);
}

de::File1& FS1::interpret(de::FileHandle& hndl, String path, FileInfo const& info)
{
    DENG_ASSERT(!path.isEmpty());

    /// @todo Order here should be determined by the resource locator.
    static FileInterpreter interpreters[] = {
        { RT_ZIP,  interpretAsZipFile },
        { RT_WAD,  interpretAsWadFile },
        { RT_NONE, NULL }
    };

    de::File1* interpretedFile = 0;

    // Firstly try interpreter(s) for guessed resource types.
    resourcetype_t resTypeGuess = F_GuessResourceTypeFromFileName(path);
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

de::FileHandle& FS1::openFile(String const& path, String const& mode, size_t baseOffset, bool allowDuplicate)
{
#if _DEBUG
    for(int i = 0; i < mode.length(); ++i)
    {
        if(mode[i] != 'r' && mode[i] != 't' && mode[i] != 'b' && mode[i] != 'f')
            throw Error("FS1::openFile", "Unknown argument in mode string '" + mode + "'");
    }
#endif

    File1* file = d->tryOpenFile(path, mode, baseOffset, allowDuplicate);
    if(!file) throw NotFoundError("FS1::openFile", "No files found matching '" + path + "'");

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

bool FS1::accessFile(String path)
{
    de::File1* file = d->tryOpenFile(path, "rb", 0, true /* allow duplicates */);
    if(!file) return false;
    delete file;
    return true;
}

void FS1::mapPathToLump(String lumpName, String destination)
{
    if(lumpName.isEmpty() || destination.isEmpty()) return;

    // We require an absolute path - prepend the CWD if necessary.
    if(QDir::isRelativePath(destination))
    {
        String workPath = DENG2_APP->currentWorkPath().withSeparators('/');
        destination = workPath / destination;
    }

    // Have already mapped this path?
    LumpMappings::iterator found = d->lumpMappings.begin();
    for(; found != d->lumpMappings.end(); ++found)
    {
        LumpMapping const& ldm = *found;
        if(!ldm.first.compare(destination, Qt::CaseInsensitive))
            break;
    }

    LumpMapping* ldm;
    if(found == d->lumpMappings.end())
    {
        // No. Acquire another mapping.
        d->lumpMappings.push_back(LumpMapping(destination, lumpName));
        ldm = &d->lumpMappings.back();
    }
    else
    {
        // Remap to another lump.
        ldm = &*found;
        ldm->second = lumpName;
    }

    LOG_VERBOSE("Path \"%s\" now mapped to lump \"%s\"") << NativePath(ldm->first).pretty() << ldm->second;
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

void FS1::mapPath(String source, String destination)
{
    if(source.isEmpty() || destination.isEmpty()) return;

    // Have already mapped this source path?
    PathMappings::iterator found = d->pathMappings.begin();
    for(; found != d->pathMappings.end(); ++found)
    {
        PathMapping const& pm = *found;
        if(!pm.second.compare(source, Qt::CaseInsensitive))
            break;
    }

    PathMapping* pm;
    if(found == d->pathMappings.end())
    {
        // No. Acquire another mapping.
        d->pathMappings.push_back(PathMapping(destination, source));
        pm = &d->pathMappings.back();
    }
    else
    {
        // Remap to another destination.
        pm = &*found;
        pm->first = destination;
    }

    LOG_VERBOSE("Path \"%s\" now mapped to \"%s\"")
        << NativePath(pm->second).pretty() << NativePath(pm->first).pretty();
}

FS1& FS1::clearPathMappings()
{
    d->pathMappings.clear();
    return *this;
}

void FS1::printDirectory(String path)
{
    QByteArray pathUtf8 = NativePath(path).pretty().toUtf8();
    Con_Message("Directory: %s\n", pathUtf8.constData());

    // We are interested in *everything*.
    path = path / "*";

    PathList found;
    if(findAllPaths(path, 0, found))
    {
        qSort(found.begin(), found.end());

        DENG2_FOR_EACH_CONST(PathList, i, found)
        {
            QByteArray foundPath = NativePath(i->path).pretty().toUtf8();
            Con_Message("  %s\n", foundPath.constData());
        }
    }
}

/// Print contents of directories as Doomsday sees them.
D_CMD(Dir)
{
    DENG_UNUSED(src);
    if(argc > 1)
    {
        for(int i = 1; i < argc; ++i)
        {
            String path = NativePath(argv[i]).expand().withSeparators('/');
            App_FileSystem()->printDirectory(path);
        }
    }
    else
    {
        App_FileSystem()->printDirectory(String("/"));
    }
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

            QByteArray path = de::NativePath(file.composePath()).pretty().toUtf8();
            Con_Printf("\"%s\" (%i %s%s)", path.constData(),
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

void F_AddVirtualDirectoryMapping(char const* nativeSourcePath, char const* nativeDestinationPath)
{
    String source      = NativePath(nativeSourcePath).expand().withSeparators('/');
    String destination = NativePath(nativeDestinationPath).expand().withSeparators('/');
    App_FileSystem()->mapPath(source, destination);
}

void F_AddLumpDirectoryMapping(char const* lumpName, char const* nativeDestinationPath)
{
    String destination = NativePath(nativeDestinationPath).expand().withSeparators('/');
    App_FileSystem()->mapPathToLump(destination, String(lumpName));
}

void F_ResetFileIds(void)
{
    App_FileSystem()->resetFileIds();
}

boolean F_CheckFileId(char const* nativePath)
{
    String path = NativePath(nativePath).expand().withSeparators('/');
    return App_FileSystem()->checkFileId(path);
}

int F_LumpCount(void)
{
    return App_FileSystem()->nameIndex().size();
}

int F_Access(char const* nativePath)
{
    String path = NativePath(nativePath).expand().withSeparators('/');
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

struct filehandle_s* F_Open3(char const* nativePath, char const* mode, size_t baseOffset, boolean allowDuplicate)
{
    try
    {
        String path = NativePath(nativePath).expand().withSeparators('/');
        return reinterpret_cast<struct filehandle_s*>(&App_FileSystem()->openFile(path, mode, baseOffset, CPP_BOOL(allowDuplicate)));
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.
    return 0;
}

struct filehandle_s* F_Open2(char const* nativePath, char const* mode, size_t baseOffset)
{
    return F_Open3(nativePath, mode, baseOffset, true/*allow duplicates*/);
}

struct filehandle_s* F_Open(char const* nativePath, char const* mode)
{
    return F_Open2(nativePath, mode, 0/*base offset*/);
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

AutoStr* F_LumpName(lumpnum_t absoluteLumpNum)
{
    try
    {
        lumpnum_t lumpNum = absoluteLumpNum;
        String const& name = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum).name();
        QByteArray nameUtf8 = name.toUtf8();
        return AutoStr_FromTextStd(nameUtf8.constData());
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore this error.
    return AutoStr_NewStd();
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

AutoStr* F_ComposePath(struct file1_s const* _file)
{
    if(!_file) return AutoStr_NewStd();
    de::File1 const& file = reinterpret_cast<de::File1 const&>(*_file);
    QByteArray path = file.composePath().toUtf8();
    return AutoStr_FromTextStd(path.constData());
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
        de::File1 const& lump = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum);
        QByteArray path = lump.container().composePath().toUtf8();
        return AutoStr_FromTextStd(path.constData());
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
    de::File1& file = reinterpret_cast<de::File1&>(*_file);
    QByteArray path = file.lump(lumpIdx).composePath(delimiter).toUtf8();
    return AutoStr_FromTextStd(path.constData());
}

AutoStr* F_ComposeLumpPath(struct file1_s* file, int lumpIdx)
{
    return F_ComposeLumpPath2(file, lumpIdx, '/');
}

/**
 * @defgroup pathToStringFlags  Path To String Flags
 * @ingroup flags
 */
///@{
#define PTSF_QUOTED                     0x1 ///< Add double quotes around the path.
#define PTSF_TRANSFORM_EXCLUDE_PATH     0x2 ///< Exclude the path; e.g., c:/doom/myaddon.wad => myaddon.wad
#define PTSF_TRANSFORM_EXCLUDE_EXT      0x4 ///< Exclude the extension; e.g., c:/doom/myaddon.wad => c:/doom/myaddon
///@}

#define DEFAULT_PATHTOSTRINGFLAGS       (PTSF_QUOTED)

/**
 * @param files      List of files from which to compose the path string.
 * @param flags      @ref pathToStringFlags
 * @param delimiter  If not @c NULL, path fragments in the resultant string
 *                   will be delimited by this.
 *
 * @return  New string containing a concatenated, possibly delimited set of
 *          all file paths in the list.
 */
static String composeFilePathString(FS1::FileList& files, int flags = DEFAULT_PATHTOSTRINGFLAGS,
                                    String const& delimiter = ";")
{
    String result;
    DENG2_FOR_EACH_CONST(FS1::FileList, i, files)
    {
        de::File1& file = (*i)->file();

        if(flags & PTSF_QUOTED)
            result.append('"');

        if(flags & PTSF_TRANSFORM_EXCLUDE_PATH)
        {
            if(flags & PTSF_TRANSFORM_EXCLUDE_EXT)
                result.append(file.name().fileNameWithoutExtension());
            else
                result.append(file.name());
        }
        else
        {
            String path = file.composePath();
            if(flags & PTSF_TRANSFORM_EXCLUDE_EXT)
            {
                result.append(path.fileNamePath() + '/' + path.fileNameWithoutExtension());
            }
            else
            {
                result.append(path);
            }
        }

        if(flags & PTSF_QUOTED)
            result.append('"');

        if(*i != files.last())
            result.append(delimiter);
    }

    return result;
}

static bool findCustomFilesPredicate(de::File1& file, void* /*parameters*/)
{
    return file.hasCustom();
}

/**
 * Compiles a list of file names, separated by @a delimiter.
 */
void F_ComposePWADFileList(char* outBuf, size_t outBufSize, char const* delimiter)
{
    if(!outBuf || 0 == outBufSize) return;
    memset(outBuf, 0, outBufSize);

    FS1::FileList foundFiles;
    if(!App_FileSystem()->findAll<de::Wad>(findCustomFilesPredicate, 0/*no params*/, foundFiles)) return;

    String str = composeFilePathString(foundFiles, PTSF_TRANSFORM_EXCLUDE_PATH, delimiter);
    QByteArray strUtf8 = str.toUtf8();
    strncpy(outBuf, strUtf8.constData(), outBufSize);
}

uint F_CRCNumber(void)
{
    return App_FileSystem()->loadedFilesCRC();
}

lumpnum_t F_OpenAuxiliary2(char const* nativePath, size_t baseOffset)
{
    String path = NativePath(nativePath).expand().withSeparators('/');
    return App_FileSystem()->openAuxiliary(path, baseOffset);
}

lumpnum_t F_OpenAuxiliary(char const* nativePath)
{
    return F_OpenAuxiliary2(nativePath, 0/*base offset*/);
}

void F_CloseAuxiliary(void)
{
    App_FileSystem()->closeAuxiliaryPrimaryIndex();
}
