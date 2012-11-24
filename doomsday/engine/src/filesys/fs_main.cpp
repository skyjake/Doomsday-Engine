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

#include "filesys/file.h"
#include "filesys/fileid.h"
#include "filesys/fileinfo.h"
#include "filesys/lumpindex.h"

#include "resource/wad.h"
#include "resource/zip.h"
#include "game.h"

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
    FS1& self;

    /// @c true= Flag newly opened files as "startup".
    bool loadingForStartup;

    /// List of currently opened files.
    FileList openFiles;

    /// List of all loaded files present in the system.
    FileList loadedFiles;

    /// Database of unique identifiers for all loaded/opened files.
    FileIds fileIds;

    /// Primary index of all files in the system.
    LumpIndex primaryIndex;

    /// Type-specific index for ZipFiles.
    LumpIndex zipFileIndex;

    /// Virtual (file) path => Lump name mapping.
    LumpMappings lumpMappings;

    /// Virtual file-directory mapping.
    PathMappings pathMappings;

    /// System subspace schemes containing subsets of the total files.
    Schemes schemes;

    Instance(FS1* d) : self(*d), loadingForStartup(true),
        openFiles(), loadedFiles(), fileIds(),
        primaryIndex(), zipFileIndex(LIF_UNIQUE_PATHS)
    {}

    ~Instance()
    {
        clearLoadedFiles();
        clearOpenFiles();
        clearIndexes();

        fileIds.clear(); // Should be null-op if bookkeeping is correct.

        pathMappings.clear();
        lumpMappings.clear();

        clearAllSchemes();
    }

    void clearAllSchemes()
    {
        DENG2_FOR_EACH(Schemes, i, schemes)
        {
            delete *i;
        }
        schemes.clear();
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
                self.deindex(file);
                delete &file;
            }
        }
    }

    void clearOpenFiles()
    {
        while(!openFiles.empty())
        { delete openFiles.back(); }
    }

    void clearIndexes()
    {
        primaryIndex.clear();
        zipFileIndex.clear();
    }

    String findPath(de::Uri const& search)
    {
        // Within a subspace scheme?
        try
        {
            FS1::Scheme& scheme = self.scheme(search.scheme());
            LOG_TRACE("Using scheme '%s'...") << scheme.name();

            // Ensure the scheme's index is up to date.
            scheme.rebuild();

            // The in-scheme name is the file name sans extension.
            String name = search.path().lastSegment().toString().fileNameWithoutExtension();

            // Perform the search.
            FS1::Scheme::FoundNodes foundNodes;
            if(scheme.findAll(name, foundNodes))
            {
                // At least one node name was matched (perhaps partially).
                DENG2_FOR_EACH_CONST(FS1::Scheme::FoundNodes, i, foundNodes)
                {
                    PathTree::Node& node = **i;
                    if(!node.comparePath(search, PCF_NO_BRANCH))
                    {
                        // This is the file we are looking for.
                        return node.composePath();
                    }
                }
            }

            /// @todo Should return not-found here but some searches are still dependent
            ///       on falling back to a wider search. -ds
        }
        catch(FS1::UnknownSchemeError const&)
        {} // Ignore this error.

        // Try a wider search of the whole virtual file system.
        de::File1* file = openFile(search.path(), "rb", 0, true /* allow duplicates */);
        if(file)
        {
            String found = file->composePath();
            delete file;
            return found;
        }

        return ""; // Not found.
    }

    File1* findLump(String path, String const& /*mode*/)
    {
        if(path.isEmpty()) return 0;

        // We must have an absolute path - prepend the base path if necessary.
        if(QDir::isRelativePath(path))
        {
            path = App_BasePath() / path;
        }

        // First check the Zip lump index.
        lumpnum_t lumpNum = zipFileIndex.indexForPath(de::Uri(path, RC_NULL));
        if(lumpNum >= 0)
        {
            return &zipFileIndex.lump(lumpNum);
        }

        // Nope. Any applicable dir/WAD redirects?
        if(!lumpMappings.empty())
        {
            DENG2_FOR_EACH_CONST(LumpMappings, i, lumpMappings)
            {
                LumpMapping const& mapping = *i;
                if(mapping.first.compare(path)) continue;

                lumpnum_t lumpNum = self.lumpNumForName(mapping.second);
                if(lumpNum < 0) continue;

                return &self.nameIndex().lump(lumpNum);
            }
        }

        return 0;
    }

    FILE* findAndOpenNativeFile(String path, String const& mymode, String& foundPath)
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
        FILE* nativeFile = fopen(nativePath.toUtf8().constData(), mode);
        if(nativeFile)
        {
            foundPath = nativePath.expand().withSeparators('/');
            return nativeFile;
        }

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
                nativeFile = fopen(nativePath.toUtf8().constData(), mode);
                if(nativeFile)
                {
                    foundPath = nativePath.expand().withSeparators('/');
                    return nativeFile;
                }
            }
        }

        return 0;
    }

    de::File1* openFile(String path, String const& mode, size_t baseOffset,
                        bool allowDuplicate)
    {
        if(path.isEmpty()) return 0;

        LOG_AS("FS1::openFile");

        // We must have an absolute path.
        if(QDir::isRelativePath(path))
        {
            path = App_BasePath() / path;
        }

        LOG_TRACE("Trying \"%s\"...") << NativePath(path).pretty();

        bool const reqNativeFile = mode.contains('f');

        FileHandle* hndl = 0;
        FileInfo info; // The temporary info descriptor.

        // First check for lumps?
        if(!reqNativeFile)
        {
            if(File1* found = findLump(path, mode))
            {
                // Do not read files twice.
                if(!allowDuplicate && !self.checkFileId(found->composeUri())) return 0;

                // Get a handle to the lump we intend to open.
                /// @todo The way this buffering works is nonsensical it should not be done here
                ///        but should instead be deferred until the content of the lump is read.
                hndl = FileHandleBuilder::fromLump(*found, /*baseOffset,*/ false/*dontBuffer*/);

                // Prepare a temporary info descriptor.
                info = found->info();
            }
        }

        // Not found? - try a native file.
        if(!hndl)
        {
            String foundPath;
            if(FILE* found = findAndOpenNativeFile(path, mode, foundPath))
            {
                // Do not read files twice.
                if(!allowDuplicate && !self.checkFileId(de::Uri(foundPath, RC_NULL)))
                {
                    fclose(found);
                    return 0;
                }

                // Acquire a handle on the file we intend to open.
                hndl = FileHandleBuilder::fromNativeFile(*found, baseOffset);

                // Prepare the temporary info descriptor.
                info = FileInfo(F_GetLastModified(foundPath.toUtf8().constData()));
            }
        }

        // Nothing?
        if(!hndl) return 0;

        // Search path is used here rather than found path as the latter may have
        // been mapped to another location. We want the file to be attributed with
        // the path it is to be known by throughout the virtual file system.

        File1& file = self.interpret(*hndl, path, info);

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
    delete d;
    FileHandleBuilder::shutdown();
}

FS1::Scheme& FS1::createScheme(String name, Scheme::Flags flags)
{
    DENG_ASSERT(name.length() >= Scheme::min_name_length);

    // Ensure this is a unique name.
    if(knownScheme(name)) return scheme(name);

    // Create a new scheme.
    Scheme* newScheme = new Scheme(name, flags);
    d->schemes.insert(name.toLower(), newScheme);
    return *newScheme;
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

void FS1::index(de::File1& file)
{
#ifdef DENG_DEBUG
    // Ensure this hasn't yet been indexed.
    FileList::const_iterator found = findListFile(d->loadedFiles, file);
    if(found != d->loadedFiles.end())
        throw Error("FS1::index", "File \"" + NativePath(file.composePath()).pretty() + "\" has already been indexed");
#endif

    // Publish lumps to one or more indexes?
    if(Zip* zip = dynamic_cast<Zip*>(&file))
    {
        if(!zip->empty())
        {
            // Insert the lumps into their rightful places in the index.
            for(int i = 0; i < zip->lumpCount(); ++i)
            {
                File1& lump = zip->lump(i);

                d->primaryIndex.catalogLump(lump);

                // Zip files go into a special ZipFile index as well.
                d->zipFileIndex.catalogLump(lump);
            }
        }
    }
    else if(Wad* wad = dynamic_cast<Wad*>(&file))
    {
        if(!wad->empty())
        {
            // Insert the lumps into their rightful places in the index.
            for(int i = 0; i < wad->lumpCount(); ++i)
            {
                File1& lump = wad->lump(i);
                d->primaryIndex.catalogLump(lump);
            }
        }
    }

    // Add a handle to the loaded files list.
    FileHandle* loadedFilesHndl = FileHandleBuilder::fromFile(file);
    d->loadedFiles.push_back(loadedFilesHndl); loadedFilesHndl->setList(reinterpret_cast<struct filelist_s*>(&d->loadedFiles));
}

void FS1::deindex(de::File1& file)
{
    FileList::iterator found = findListFile(d->loadedFiles, file);
    if(found == d->loadedFiles.end()) return; // Most peculiar..

    QByteArray path = file.composePath().toUtf8();
    d->releaseFileId(path.constData());

    d->zipFileIndex.pruneByFile(file);
    d->primaryIndex.pruneByFile(file);

    d->loadedFiles.erase(found);
    delete *found;
}

de::File1& FS1::find(de::Uri const& search)
{
    LOG_AS("FS1::find");
    if(!search.isEmpty())
    {
        try
        {
            String searchPath = search.resolved();

            // Convert to an absolute path.
            if(!QDir::isAbsolutePath(searchPath))
            {
                searchPath = App_BasePath() / searchPath;
            }

            FileList::iterator found = findListFileByPath(d->loadedFiles, searchPath);
            if(found != d->loadedFiles.end())
            {
                DENG_ASSERT((*found)->hasFile());
                return (*found)->file();
            }
        }
        catch(de::Uri::ResolveError const& er)
        {
            // Log but otherwise ignore unresolved paths.
            LOG_DEBUG(er.asText());
        }
    }

    /// @throw NotFoundError  No files found matching the search term.
    throw NotFoundError("FS1::find", "No files found matching '" + search.compose() + "'");
}

String FS1::findPath(de::Uri const& search, int flags, ResourceClass& rclass)
{
    LOG_AS("FS1::findPath");
    if(!search.isEmpty())
    {
        try
        {
            String searchPath = search.resolved();

            // If an extension was specified, first look for files of the same type.
            String ext = searchPath.fileNameExtension();
            if(!ext.isEmpty() && ext.compare(".*"))
            {
                String found = d->findPath(de::Uri(searchPath, RC_NULL).setScheme(search.scheme()));
                if(!found.isEmpty()) return found;

                // If we are looking for a particular file type, get out of here.
                if(flags & RLF_MATCH_EXTENSION) return "";
            }

            if(isNullResourceClass(rclass) || !rclass.fileTypeCount()) return "";

            /*
             * Try each expected file type name extension for this resource class.
             */
            String searchPathWithoutFileNameExtension = searchPath.fileNamePath() / searchPath.fileNameWithoutExtension();

            DENG2_FOR_EACH_CONST(ResourceClass::FileTypes, typeIt, rclass.fileTypes())
            {
                DENG2_FOR_EACH_CONST(QStringList, i, (*typeIt)->knownFileNameExtensions())
                {
                    String const& ext = *i;
                    String found = d->findPath(de::Uri(searchPathWithoutFileNameExtension + ext, RC_NULL).setScheme(search.scheme()));
                    if(!found.isEmpty()) return found;
                }
            };
        }
        catch(de::Uri::ResolveError const& er)
        {
            // Log but otherwise ignore unresolved paths.
            LOG_DEBUG(er.asText());
        }
    }

    /// @throw NotFoundError  No files found matching the search term.
    throw NotFoundError("FS1::findPath", "No paths found matching '" + search.compose() + "'");
}

String FS1::findPath(de::Uri const& search, int flags)
{
    return findPath(search, flags, DD_ResourceClassById(RC_NULL));
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

int FS1::unloadAllNonStartupFiles()
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

    return numUnloadedFiles;
}

bool FS1::checkFileId(de::Uri const& path)
{
    if(!accessFile(path)) return false;

    // Calculate the identifier.
    FileId fileId = FileId::fromPath(path.compose());
    FileIds::iterator place = qLowerBound(d->fileIds.begin(), d->fileIds.end(), fileId);
    if(place != d->fileIds.end() && *place == fileId) return false;

#ifdef _DEBUG
    LOG_DEBUG("Added FileId %s - \"%s\"") << fileId << fileId.path(); /* path() is debug-only */
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
}

LumpIndex const& FS1::nameIndex() const
{
    return d->primaryIndex;
}

lumpnum_t FS1::lumpNumForName(String name)
{
    LOG_AS("FS1::lumpNumForName");

    if(name.isEmpty()) return -1;

    // Append a .lmp extension if none is specified.
    if(name.fileNameExtension().isEmpty())
    {
        name += ".lmp";
    }

    // Perform the search.
    return d->primaryIndex.indexForPath(de::Uri(name, RC_NULL));
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
        searchPattern = App_BasePath() / searchPattern;
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
        if(!(flags & SearchPath::NoDescend))
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

de::File1& FS1::interpret(de::FileHandle& hndl, String filePath, FileInfo const& info)
{
    DENG_ASSERT(!filePath.isEmpty());

    de::File1* interpretedFile = 0;

    // Firstly try the interpreter for the guessed resource types.
    FileType const& ftypeGuess = DD_GuessFileTypeFromFileName(filePath);
    if(NativeFileType const* fileType = dynamic_cast<NativeFileType const*>(&ftypeGuess))
    {
        interpretedFile = fileType->interpret(hndl, filePath, info);
    }

    // If not yet interpreted - try each recognisable format in order.
    if(!interpretedFile)
    {
        FileTypes const& fileTypes = DD_FileTypes();
        DENG2_FOR_EACH_CONST(FileTypes, i, fileTypes)
        {
            if(NativeFileType const* fileType = dynamic_cast<NativeFileType const*>(*i))
            {
                // Already tried this?
                if(fileType == &ftypeGuess) continue;

                interpretedFile = fileType->interpret(hndl, filePath, info);
                if(interpretedFile) break;
            }
        }
    }

    // Still not interpreted?
    if(!interpretedFile)
    {
        // Use a generic file.
        File1* container = (hndl.hasFile() && hndl.file().isContained())? &hndl.file().container() : 0;
        interpretedFile = new File1(hndl, filePath, info, container);
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

    File1* file = d->openFile(path, mode, baseOffset, allowDuplicate);
    if(!file) throw NotFoundError("FS1::openFile", "No files found matching '" + path + "'");

    // Add a handle to the opened files list.
    FileHandle& openFilesHndl = *FileHandleBuilder::fromFile(*file);
    d->openFiles.push_back(&openFilesHndl); openFilesHndl.setList(reinterpret_cast<struct filelist_s*>(&d->openFiles));
    return openFilesHndl;
}

de::FileHandle& FS1::openLump(de::File1& lump)
{
    // Add a handle to the opened files list.
    FileHandle& openFilesHndl = *FileHandleBuilder::fromLump(lump, false/*do buffer*/);
    d->openFiles.push_back(&openFilesHndl); openFilesHndl.setList(reinterpret_cast<struct filelist_s*>(&d->openFiles));
    return openFilesHndl;
}

bool FS1::accessFile(de::Uri const& search)
{
    try
    {
        String searchPath = search.resolved();
        de::File1* file = d->openFile(searchPath, "rb", 0, true /* allow duplicates */);
        if(file)
        {
            delete file;
            return true;
        }
    }
    catch(de::Uri::ResolveError const& er)
    {
        // Log but otherwise ignore unresolved paths.
        LOG_DEBUG(er.asText());
    }
    return false;
}

void FS1::addPathLumpMapping(String lumpName, String destination)
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

void FS1::clearPathLumpMappings()
{
    d->lumpMappings.clear();
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

void FS1::addPathMapping(String source, String destination)
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

void FS1::clearPathMappings()
{
    d->pathMappings.clear();
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

bool FS1::knownScheme(String name)
{
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return true;
    }
    return false;
}

FS1::Scheme& FS1::scheme(String name)
{
    if(!name.isEmpty())
    {
        Schemes::iterator found = d->schemes.find(name.toLower());
        if(found != d->schemes.end()) return **found;
    }
    /// @throw UnknownSchemeError An unknown scheme was referenced.
    throw UnknownSchemeError("FS1::scheme", "No scheme found matching '" + name + "'");
}

FS1::Schemes const& FS1::allSchemes()
{
    return d->schemes;
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
        lumpnum_t lumpNum = App_FileSystem()->lumpNumForName(argv[1]);
        if(lumpNum >= 0)
        {
            return F_DumpLump(lumpNum);
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

            int fileCount = 1;
            if(de::Zip* zip = dynamic_cast<de::Zip*>(&file))
            {
                fileCount = zip->lumpCount();
            }
            else if(de::Wad* wad = dynamic_cast<de::Wad*>(&file))
            {
                fileCount = wad->lumpCount();
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

FS1* App_FileSystem()
{
    if(!fileSystem) throw Error("App_FileSystem", "File system not yet initialized");
    return fileSystem;
}

String App_BasePath()
{
    /// @todo Shouldn't this end in '/'? It causes failure to locate doomsday.pk3...
    return App::app().nativeBasePath().withSeparators('/'); // + '/';
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

int F_UnloadAllNonStartupFiles(void)
{
    return App_FileSystem()->unloadAllNonStartupFiles();
}

void F_AddVirtualDirectoryMapping(char const* nativeSourcePath, char const* nativeDestinationPath)
{
    String source      = NativePath(nativeSourcePath).expand().withSeparators('/');
    String destination = NativePath(nativeDestinationPath).expand().withSeparators('/');
    App_FileSystem()->addPathMapping(source, destination);
}

void F_AddLumpDirectoryMapping(char const* lumpName, char const* nativeDestinationPath)
{
    String destination = NativePath(nativeDestinationPath).expand().withSeparators('/');
    App_FileSystem()->addPathLumpMapping(destination, String(lumpName));
}

void F_ResetFileIds(void)
{
    App_FileSystem()->resetFileIds();
}

boolean F_CheckFileId(char const* nativePath)
{
    return App_FileSystem()->checkFileId(de::Uri::fromNativePath(nativePath));
}

int F_LumpCount(void)
{
    return App_FileSystem()->nameIndex().size();
}

int F_Access(char const* nativePath)
{
    de::Uri path = de::Uri::fromNativePath(nativePath);
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

struct filehandle_s* F_OpenLump(lumpnum_t lumpNum)
{
    try
    {
        de::File1& lump = App_FileSystem()->nameIndex().lump(lumpNum);
        return reinterpret_cast<struct filehandle_s*>(&App_FileSystem()->openLump(lump));
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore error.
    return 0;
}

boolean F_IsValidLumpNum(lumpnum_t lumpNum)
{
    return App_FileSystem()->nameIndex().isValidIndex(lumpNum);
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

AutoStr* F_LumpName(lumpnum_t lumpNum)
{
    try
    {
        String const& name = App_FileSystem()->nameIndex().lump(lumpNum).name();
        QByteArray nameUtf8 = name.toUtf8();
        return AutoStr_FromTextStd(nameUtf8.constData());
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore this error.
    return AutoStr_NewStd();
}

size_t F_LumpLength(lumpnum_t lumpNum)
{
    try
    {
        return App_FileSystem()->nameIndex().lump(lumpNum).size();
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore this error.
    return 0;
}

uint F_LumpLastModified(lumpnum_t lumpNum)
{
    try
    {
        return App_FileSystem()->nameIndex().lump(lumpNum).lastModified();
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
    de::File1* file = reinterpret_cast<de::File1*>(_file);
    if(Wad* wad = dynamic_cast<Wad*>(file))
    {
        return wad->lump(lumpIdx).read(buffer);
    }
    if(Zip* zip = dynamic_cast<Zip*>(file))
    {
        return zip->lump(lumpIdx).read(buffer);
    }
    return file->read(buffer);
}

size_t F_ReadLumpSection(struct file1_s* _file, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    if(!_file) return 0;
    de::File1* file = reinterpret_cast<de::File1*>(_file);
    if(de::Wad* wad = dynamic_cast<de::Wad*>(file))
    {
        return wad->lump(lumpIdx).read(buffer, startOffset, length);
    }
    if(de::Zip* zip = dynamic_cast<de::Zip*>(file))
    {
        return zip->lump(lumpIdx).read(buffer, startOffset, length);
    }
    return file->read(buffer, startOffset, length);
}

uint8_t const* F_CacheLump(struct file1_s* _file, int lumpIdx)
{
    if(!_file) return 0;
    de::File1* file = reinterpret_cast<de::File1*>(_file);
    if(de::Wad* wad = dynamic_cast<de::Wad*>(file))
    {
        return wad->lump(lumpIdx).cache();
    }
    if(de::Zip* zip = dynamic_cast<de::Zip*>(file))
    {
        return zip->lump(lumpIdx).cache();
    }
    return file->cache();
}

void F_UnlockLump(struct file1_s* _file, int lumpIdx)
{
    if(!_file) return;
    de::File1* file = reinterpret_cast<de::File1*>(_file);
    if(de::Wad* wad = dynamic_cast<de::Wad*>(file))
    {
        wad->unlockLump(lumpIdx);
        return;
    }
    if(de::Zip* zip = dynamic_cast<de::Zip*>(file))
    {
        zip->unlockLump(lumpIdx);
        return;
    }
    file->unlock();
}

struct file1_s* F_FindFileForLumpNum2(lumpnum_t lumpNum, int* lumpIdx)
{
    try
    {
        de::File1 const& lump = App_FileSystem()->nameIndex().lump(lumpNum);
        if(lumpIdx) *lumpIdx = lump.info().lumpIdx;
        return reinterpret_cast<struct file1_s*>(&lump.container());
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore error.
    return 0;
}

struct file1_s* F_FindFileForLumpNum(lumpnum_t lumpNum)
{
    try
    {
        de::File1 const& lump = App_FileSystem()->nameIndex().lump(lumpNum);
        return reinterpret_cast<struct file1_s*>(&lump.container());
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore error.
    return 0;
}

AutoStr* F_ComposeLumpFilePath(lumpnum_t lumpNum)
{
    try
    {
        de::File1 const& lump = App_FileSystem()->nameIndex().lump(lumpNum);
        QByteArray path = lump.container().composePath().toUtf8();
        return AutoStr_FromTextStd(path.constData());
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore this error.
    return AutoStr_NewStd();
}

boolean F_LumpIsCustom(lumpnum_t lumpNum)
{
    try
    {
        de::File1 const& lump = App_FileSystem()->nameIndex().lump(lumpNum);
        return lump.container().hasCustom();
    }
    catch(LumpIndex::NotFoundError const&)
    {} // Ignore this error.
    return false;
}

AutoStr* F_ComposeLumpPath2(struct file1_s* _file, int lumpIdx, char delimiter)
{
    if(!_file) return AutoStr_NewStd();
    de::File1* file = reinterpret_cast<de::File1*>(_file);
    if(de::Wad* wad = dynamic_cast<de::Wad*>(file))
    {
        QByteArray path = wad->lump(lumpIdx).composePath(delimiter).toUtf8();
        return AutoStr_FromTextStd(path.constData());
    }
    if(de::Zip* zip = dynamic_cast<de::Zip*>(file))
    {
        QByteArray path = zip->lump(lumpIdx).composePath(delimiter).toUtf8();
        return AutoStr_FromTextStd(path.constData());
    }
    QByteArray path = file->composePath(delimiter).toUtf8();
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

uint F_LoadedFilesCRC(void)
{
    return App_FileSystem()->loadedFilesCRC();
}

boolean F_FindPath2(resourceclassid_t classId, uri_s const* searchPath,
    ddstring_t* foundPath, int flags)
{
    DENG_ASSERT(searchPath);
    try
    {
        String found = App_FileSystem()->findPath(reinterpret_cast<de::Uri const&>(*searchPath), flags,
                                                  DD_ResourceClassById(classId));
        // Does the caller want to know the matched path?
        if(foundPath)
        {
            Str_Set(foundPath, found.toUtf8().constData());
            F_PrependBasePath(foundPath, foundPath);
        }
        return true;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.
    return false;
}

boolean F_FindPath(resourceclassid_t classId, uri_s const* searchPath,
    ddstring_t* foundPath)
{
    return F_FindPath2(classId, searchPath, foundPath, RLF_DEFAULT);
}

uint F_FindPathInList(resourceclassid_t classId, char const* searchPaths,
    ddstring_t* foundPath, int flags)
{
    DENG_ASSERT(classId != RC_UNKNOWN);
    if(!searchPaths || !searchPaths[0]) return 0;

    ResourceClass& rclass = DD_ResourceClassById(classId);

    QStringList paths = String(searchPaths).split(';', QString::SkipEmptyParts);
    int pathIndex = 0;
    for(QStringList::const_iterator i = paths.constBegin(); i != paths.constEnd(); ++i, ++pathIndex)
    {
        de::Uri searchPath = de::Uri(*i, classId);
        try
        {
            String found = App_FileSystem()->findPath(searchPath, flags, rclass);
            if(!found.isEmpty())
            {
                // Does the caller want to know the matched path?
                if(foundPath)
                {
                    Str_Set(foundPath, found.toUtf8().constData());
                    F_PrependBasePath(foundPath, foundPath);
                }
                return pathIndex + 1; // 1-based index.
            }
        }
        catch(FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    return 0; // Not found.
}
