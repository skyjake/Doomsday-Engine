/** @file packageloader.cpp  Loads and unloads packages.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/PackageLoader"
#include "de/FS"
#include "de/App"
#include "de/Version"
#include "de/Info"
#include "de/Log"
#include "de/Parser"
#include "de/LinkFile"
#include "de/PackageFeed"

#include <QMap>
#include <QRegExp>

namespace de {

DENG2_PIMPL(PackageLoader)
{
    LoadedPackages loaded;
    int loadCounter;

    Instance(Public *i) : Base(i), loadCounter(0)
    {}

    ~Instance()
    {
        // We own all loaded packages.
        qDeleteAll(loaded.values());
    }

    /**
     * Determines if a specific package is currently loaded.
     * @param file  Package root.
     * @return @c true, if loaded.
     */
    bool isLoaded(File const &file) const
    {
        LoadedPackages::const_iterator found = loaded.constFind(Package::identifierForFile(file));
        if(found != loaded.constEnd())
        {
            return &found.value()->file() == &file;
        }
        return false;
    }

    static bool shouldIgnoreFile(File const &packageFile)
    {
        if(Feed const *feed = packageFile.originFeed())
        {
            // PackageFeed generates links to loaded packages.
            if(feed->is<PackageFeed>()) return true;
        }
        return false;
    }

    int findAllVariants(String const &packageIdVer, FS::FoundFiles &found) const
    {
        // The version may be provided optionally, to set the preferred version.
        auto idVer = Package::split(packageIdVer);

        String const packageId = idVer.first;
        QStringList const components = packageId.split('.');

        String id;

        // The package may actually be inside other packages, so we need to check
        // each component of the package identifier.
        for(int i = components.size() - 1; i >= 0; --i)
        {
            id = components.at(i) + (!id.isEmpty()? "." + id : "");

            FS::FoundFiles files;
            App::fileSystem().findAllOfTypes(StringList()
                                             << DENG2_TYPE_NAME(Folder)
                                             << DENG2_TYPE_NAME(ArchiveFolder)
                                             << DENG2_TYPE_NAME(LinkFile),
                                             id + ".pack", files);

            files.remove_if([&packageId] (File *file) {
                if(shouldIgnoreFile(*file)) return true;
                return Package::identifierForFile(*file) != packageId;
            });

            std::copy(files.begin(), files.end(), std::back_inserter(found));
        }

        return int(found.size());
    }

    /**
     * Parses or updates the metadata of a package, and checks it for validity.
     * A Package::ValidationError is thrown if the package metadata does not
     * comply with the minimum requirements.
     *
     * @param packFile  Package file (".pack" folder).
     */
    static void checkPackage(File &packFile)
    {
        Package::parseMetadata(packFile);

        if(!packFile.objectNamespace().has(Package::VAR_PACKAGE))
        {
            throw Package::ValidationError("PackageLoader::checkPackage",
                                           packFile.description() + " is not a package");
        }

        Package::validateMetadata(packFile.objectNamespace().subrecord("package"));
    }

    File const *selectPackage(IdentifierList const &idList) const
    {
        for(auto const &id : idList.ids)
        {
            if(File const *f = selectPackage(id))
                return f;
        }
        return nullptr;
    }

    /**
     * Given a package identifier, pick one of the available versions of the package
     * based on predefined criteria.
     *
     * @param packageId  Package identifier. This may optionally include the
     *                   required version number.
     *
     * @return Selected package, or @c NULL if a version could not be selected.
     */
    File const *selectPackage(String const &packageId) const
    {
        LOG_AS("selectPackage");

        FS::FoundFiles found;
        if(!findAllVariants(packageId, found))
        {
            // None found.
            return nullptr;
        }

        // Each must have a version specified.
        DENG2_FOR_EACH_CONST(FS::FoundFiles, i, found)
        {
            checkPackage(**i);
        }

        // If the identifier includes a version, only accept that specific version.
        auto idVer = Package::split(packageId);
        if(idVer.second.isValid())
        {
            found.remove_if([&idVer] (File *f) {
                Version const pkgVer = f->objectNamespace().gets("package.version");
                return (pkgVer != idVer.second); // Ignore other versions.
            });
            // Did we run out of candidates?
            if(found.empty()) return nullptr;
        }

        // Sorted descending by version.
        found.sort([] (File const *a, File const *b) -> bool
        {
            // The version must be specified using a format understood by Version.
            Version const aVer(a->objectNamespace().gets("package.version"));
            Version const bVer(b->objectNamespace().gets("package.version"));

            if(aVer == bVer)
            {
                // Identifical versions are prioritized by modification time.
                return de::cmp(a->status().modifiedAt, b->status().modifiedAt) < 0;
            }
            return aVer < bVer;
        });

        LOG_RES_XVERBOSE("Selected '%s': %s") << packageId << found.back()->description();

        return found.back();
    }

    Package &load(String const &packageId, File const &source)
    {
        if(loaded.contains(packageId))
        {
            throw AlreadyLoadedError("PackageLoader::load",
                                     "Package '" + packageId + "' is already loaded from \"" +
                                     loaded[packageId]->objectNamespace().gets("path") + "\"");
        }

        // Loads all the packages listed as required for this package. Throws an
        // exception is any are missing.
        loadRequirements(source);

        Package *pkg = new Package(source);
        loaded.insert(packageId, pkg);
        pkg->setOrder(loadCounter++);
        pkg->didLoad();
        return *pkg;
    }

    bool unload(String identifier)
    {
        LoadedPackages::iterator found = loaded.find(identifier);
        if(found == loaded.end()) return false;

        Package *pkg = found.value();
        pkg->aboutToUnload();
        delete pkg;

        loaded.remove(identifier);
        return true;
    }

    void listPackagesInIndex(FileIndex const &index, StringList &list)
    {
        for(auto i = index.begin(); i != index.end(); ++i)
        {
            if(shouldIgnoreFile(*i->second)) continue;

            if(i->first.fileNameExtension() == ".pack")
            {
                try
                {
                    File &file = *i->second;
                    String path = file.path();

                    // The special persistent data package should be ignored.
                    if(path == "/home/persist.pack") continue;

                    // Check the metadata.
                    checkPackage(file);

                    list.append(path);
                }
                catch(Package::ValidationError const &er)
                {
                    // Not a loadable package.
                    qDebug() << i->first << ": Package is invalid:" << er.asText();
                }
                catch(Parser::SyntaxError const &er)
                {
                    LOG_RES_NOTE("\"%s\": Package has a Doomsday Script syntax error: %s")
                            << i->first << er.asText();
                }
                catch(Info::SyntaxError const &er)
                {
                    // Not a loadable package.
                    LOG_RES_NOTE("\"%s\": Package has a syntax error: %s")
                            << i->first << er.asText();
                }

                /// @todo Store the errors so that the UI can show a list of
                /// problematic packages. -jk
            }
        }
    }

    void loadRequirements(File const &packageFile)
    {
        for(String const &reqId : Package::requires(packageFile))
        {
            if(!self.isLoaded(reqId))
            {
                self.load(reqId);
            }
        }
    }

    DENG2_PIMPL_AUDIENCE(Activity)
    DENG2_PIMPL_AUDIENCE(Load)
    DENG2_PIMPL_AUDIENCE(Unload)
};

DENG2_AUDIENCE_METHOD(PackageLoader, Activity)
DENG2_AUDIENCE_METHOD(PackageLoader, Load)
DENG2_AUDIENCE_METHOD(PackageLoader, Unload)

PackageLoader::PackageLoader() : d(new Instance(this))
{}

bool PackageLoader::isAvailable(String const &packageId) const
{
    return d->selectPackage(IdentifierList(packageId)) != nullptr;
}

File const *PackageLoader::select(String const &packageId) const
{
    return d->selectPackage(IdentifierList(packageId));
}

Package const &PackageLoader::load(String const &packageId)
{
    LOG_AS("PackageLoader");

    File const *packFile = d->selectPackage(IdentifierList(packageId));
    if(!packFile)
    {
        throw NotFoundError("PackageLoader::load",
                            "Package \"" + packageId + "\" is not available");
    }

    // Use the identifier computed from the file because @a packageId may
    // actually list multiple alternatives.
    String const id = Package::identifierForFile(*packFile);
    d->load(id, *packFile);

    try
    {
        DENG2_FOR_AUDIENCE2(Load, i)
        {
            i->packageLoaded(id);
        }
        DENG2_FOR_AUDIENCE2(Activity, i)
        {
            i->setOfLoadedPackagesChanged();
        }
    }
    catch(Error const &er)
    {
        // Someone took issue with the loaded package; cancel the load.
        unload(id);
        throw PostLoadError("PackageLoader::load",
                            "Error during post-load actions of package \"" +
                            id + "\": " + er.asText());
    }

    return package(id);
}

void PackageLoader::unload(String const &packageId)
{
    String id = Package::split(packageId).first;

    if(isLoaded(id))
    {
        DENG2_FOR_AUDIENCE2(Unload, i)
        {
            i->aboutToUnloadPackage(id);
        }

        d->unload(id);

        DENG2_FOR_AUDIENCE2(Activity, i)
        {
            i->setOfLoadedPackagesChanged();
        }
    }
}

void PackageLoader::unloadAll()
{
    LOG_AS("PackageLoader");
    LOG_RES_MSG("Unloading %i packages") << d->loaded.size();

    while(!d->loaded.isEmpty())
    {
        unload(d->loaded.begin().key());
    }
}

bool PackageLoader::isLoaded(String const &packageId) const
{
    return d->loaded.contains(packageId);
}

bool PackageLoader::isLoaded(File const &file) const
{
    return d->isLoaded(file);
}

PackageLoader::LoadedPackages const &PackageLoader::loadedPackages() const
{
    return d->loaded;
}

FS::FoundFiles PackageLoader::loadedPackagesAsFilesInPackageOrder() const
{
    QMap<int, File const *> files; // sorted in package order
    for(Package const *pkg : loadedPackages().values())
    {
        files.insert(pkg->order(), &pkg->sourceFile());
    }
    FS::FoundFiles sorted;
    for(auto i : files) sorted.push_back(const_cast<File *>(i));
    return sorted;
}

Package const &PackageLoader::package(String const &packageId) const
{
    if(!isLoaded(packageId))
    {
        throw NotFoundError("PackageLoader::package", "Package '" + packageId + "' is not loaded");
    }
    return *d->loaded[packageId];
}

void PackageLoader::sortInPackageOrder(FS::FoundFiles &filesToSort) const
{
    typedef std::pair<File *, int> FileAndOrder;

    // Find the packages for files.
    QList<FileAndOrder> all;
    DENG2_FOR_EACH_CONST(FS::FoundFiles, i, filesToSort)
    {
        Package const *pkg = 0;
        String identifier = Package::identifierForContainerOfFile(**i);
        if(isLoaded(identifier))
        {
            pkg = &package(identifier);
        }
        all << FileAndOrder(*i, pkg? pkg->order() : -1);
    }

    // Sort by package order.
    std::sort(all.begin(), all.end(), [] (FileAndOrder const &a, FileAndOrder const &b) {
        return a.second < b.second;
    });

    // Put the results back in the given array.
    filesToSort.clear();
    foreach(FileAndOrder const &f, all)
    {
        filesToSort.push_back(f.first);
    }
}

void PackageLoader::loadFromCommandLine()
{
    CommandLine &args = App::commandLine();

    for(int p = 0; p < args.count(); )
    {
        // Find all the -pkg options.
        if(!args.matches("-pkg", args.at(p)))
        {
            ++p;
            continue;
        }

        // Load all the specified packages (by identifier, not by path).
        while(++p != args.count() && !args.isOption(p))
        {
            load(args.at(p));
        }
    }
}

StringList PackageLoader::findAllPackages() const
{
    StringList all;
    for(QString typeName : QStringList({ DENG2_TYPE_NAME(Folder),
                                         DENG2_TYPE_NAME(ArchiveFolder),
                                         DENG2_TYPE_NAME(LinkFile) }))
    {
        d->listPackagesInIndex(App::fileSystem().indexFor(typeName), all);
    }
    return all;
}

PackageLoader::IdentifierList::IdentifierList(String const &spaceSeparatedIds)
{
    static QRegExp anySpace("\\s");
    for(auto const &qs : spaceSeparatedIds.split(anySpace, String::SkipEmptyParts))
    {
        ids.append(qs);
    }
}

} // namespace de
