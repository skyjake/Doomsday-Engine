/** @file packageloader.cpp  Loads and unloads packages.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/App"
#include "de/ArchiveFolder"
#include "de/CommandLine"
#include "de/Config"
#include "de/DictionaryValue"
#include "de/FS"
#include "de/Info"
#include "de/LinkFile"
#include "de/LogBuffer"
#include "de/PackageFeed"
#include "de/Parser"
#include "de/TextValue"
#include "de/Version"
#include "de/RegExp"

namespace de {

static const char *VAR_PACKAGE_VERSION("package.version");

DE_PIMPL(PackageLoader)
, DE_OBSERVES(File, Deletion) // loaded package source file is deleted?
{
    LoadedPackages loaded; ///< Identifiers are unversioned; only one version can be loaded at a time.
    int loadCounter;

    Impl(Public *i) : Base(i), loadCounter(0)
    {}

    ~Impl() override
    {
        // We own all loaded packages.
        loaded.deleteAll();
    }

    /**
     * Determines if a specific package is currently loaded.
     * @param file  Package root.
     * @return @c true, if loaded.
     */
    Package *tryFindLoaded(File const &file) const
    {
        #if 0
        {
            std::cerr << "tryFindLoaded:" << Package::identifierForFile(file) << file.description();
            for (auto i = loaded.begin(); i != loaded.end(); ++i)
            {
                std::cerr << "  currently loaded:" << i.key() << "file:" << i.value()->file().path() << "source:" << i.value()->sourceFile().path();
            }
        }
        #endif

        auto found = loaded.find(Package::identifierForFile(file));
        if (found != loaded.end())
        {
            auto const &pkg = *found->second;
            if (&pkg.file() == &file || &pkg.sourceFile() == &file)
            {
                return found->second;
            }
        }
        return nullptr;
    }

    static bool shouldIgnoreFile(File const &packageFile)
    {
        if (Feed const *feed = packageFile.originFeed())
        {
            // PackageFeed generates links to loaded packages.
            if (is<PackageFeed>(feed)) return true;
        }
        return false;
    }

    int findAllVariants(String const &packageIdVer, FS::FoundFiles &found) const
    {
        // The version may be provided optionally, to set the preferred version.
        auto idVer = Package::split(packageIdVer);

        const String     packageId  = idVer.first;
        const StringList components = packageId.split('.');

        String id;

        // The package may actually be inside other packages, so we need to check
        // each component of the package identifier.
        for (auto i = components.rbegin(); i != components.rend(); ++i)
        {
            id = *i + (!id.isEmpty()? "." + id : "");

            FS::FoundFiles files;
            App::fileSystem().findAllOfTypes(StringList()
                                             << DE_TYPE_NAME(Folder)
                                             << DE_TYPE_NAME(ArchiveFolder)
                                             << DE_TYPE_NAME(LinkFile),
                                             id + ".pack", files);

            files.remove_if([&packageId] (File *file) {
                if (shouldIgnoreFile(*file)) return true;
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

        if (!packFile.objectNamespace().has(Package::VAR_PACKAGE))
        {
            throw Package::NotPackageError("PackageLoader::checkPackage",
                                           packFile.description() + " is not a package");
        }

        Package::validateMetadata(packFile.objectNamespace().subrecord("package"));
    }

    File const *selectPackage(IdentifierList const &idList) const
    {
        for (auto const &id : idList.ids)
        {
            if (File const *f = selectPackage(id))
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
        if (!findAllVariants(packageId, found))
        {
            // None found.
            return nullptr;
        }

        // Each must have a version specified.
        {
            FS::FoundFiles checked;
            DE_FOR_EACH_CONST(FS::FoundFiles, i, found)
            {
                try
                {
                    checkPackage(**i);
                    checked.push_back(*i);
                }
                catch (const Error &)
                {
                    // Packages with errors cannot be selected.
                }
            }
            found = std::move(checked);
        }

        // If the identifier includes a version, only accept that specific version.
        const bool mustMatchExactVersion = (packageId.contains("_"));

        if (mustMatchExactVersion)
        {
            auto const idVer = Package::split(packageId);
            found.remove_if([&idVer] (File *f) {
                Version const pkgVer = f->objectNamespace().gets(VAR_PACKAGE_VERSION);
                return (pkgVer != idVer.second); // Ignore other versions.
            });
            // Did we run out of candidates?
            if (found.empty()) return nullptr;
        }

        // Sorted descending by version.
        found.sort([] (File const *a, File const *b) -> bool
        {
            // The version must be specified using a format understood by Version.
            Version const aVer(a->objectNamespace().gets(VAR_PACKAGE_VERSION));
            Version const bVer(b->objectNamespace().gets(VAR_PACKAGE_VERSION));

            if (aVer == bVer)
            {
                // Identifical versions are prioritized by modification time.
                return de::cmp(a->status().modifiedAt, b->status().modifiedAt) < 0;
            }
            return aVer < bVer;
        });

        LOG_RES_XVERBOSE("Selected '%s': %s", packageId << found.back()->description());

        return found.back();
    }

    Package &load(String const &packageId, File const &source)
    {
        if (loaded.contains(packageId))
        {
            throw AlreadyLoadedError("PackageLoader::load",
                                     "Package '" + packageId + "' is already loaded from \"" +
                                     loaded[packageId]->objectNamespace().gets("path") + "\"");
        }

        // Loads all the packages listed as required for this package. Throws an
        // exception is any are missing.
        loadRequirements(source);

        // Optional content can be loaded once Config is available so the user's
        // preferences are known.
        if (App::configExists())
        {
            loadOptionalContent(source);
        }

        Package *pkg = new Package(source);
        loaded.insert(packageId, pkg);
        pkg->setOrder(loadCounter++);
        pkg->didLoad();
        source.audienceForDeletion() += this;

        return *pkg;
    }

    bool unload(String identifier)
    {
        LoadedPackages::iterator found = loaded.find(identifier);
        if (found == loaded.end()) return false;

        Package *pkg = found->second;
        pkg->aboutToUnload();
        if (pkg->sourceFileExists())
        {
            pkg->sourceFile().audienceForDeletion() -= this;
        }
        delete pkg;
        loaded.remove(identifier);
        return true;
    }

    void fileBeingDeleted(const File &file) override
    {
        const String pkgId = Package::identifierForFile(file);
        LOG_RES_WARNING("Unloading package \"%s\": source file has been modified or deleted")
            << pkgId;
        unload(pkgId);
    }

    void listPackagesInIndex(FileIndex const &index, StringList &list)
    {
        for (File *indexedFile : index.files())
        {
            if (shouldIgnoreFile(*indexedFile)) continue;

            String const fileName = indexedFile->name();
            if (fileName.fileNameExtension() == ".pack")
            {
                try
                {
                    //File &file = *indexedFile;
                    String path = indexedFile->path();

                    // The special persistent data package should be ignored.
                    if (path == DE_STR("/home/persist.pack")) continue;

                    // Check the metadata.
                    checkPackage(*indexedFile);

                    list.append(path);
                }
                catch (Package::NotPackageError const &er)
                {
                    // This is usually a .pack folder used only for nesting.
                    LOG_RES_XVERBOSE("\"%s\": %s", fileName << er.asText());
                }
                catch (Package::ValidationError const &er)
                {
                    // Not a loadable package.
                    LOG_RES_MSG("\"%s\": Package is invalid: %s") << fileName << er.asText();
                }
                catch (Parser::SyntaxError const &er)
                {
                    LOG_RES_WARNING("\"%s\": Package has a Doomsday Script syntax error: %s")
                            << fileName << er.asText();
                }
                catch (Info::SyntaxError const &er)
                {
                    // Not a loadable package.
                    LOG_RES_WARNING("\"%s\": Package has a syntax error: %s")
                            << fileName << er.asText();
                }
                catch (const Error &er)
                {
                    LOG_RES_WARNING("\"%s\": %s") << fileName << er.asText();
                }

                /// @todo Store the errors so that the UI can show a list of
                /// problematic packages. -jk
            }
        }
    }

    void loadRequirements(File const &packageFile)
    {
        for (String const &reqId : Package::requiredPackages(packageFile))
        {
            if (!self().isLoaded(reqId))
            {
                self().load(reqId);
            }
        }
    }

    void forOptionalContent(File const &packageFile,
                            std::function<void (String const &)> callback) const
    {
        // Packages enabled/disabled for use.
        DictionaryValue const &selPkgs = Config::get("fs.selectedPackages")
                                         .value<DictionaryValue>();

        const Record &meta = Package::metadata(packageFile);
        const String idVer = Package::versionedIdentifierForFile(packageFile);

        // Helper function to determine if a particular package is marked for loading.
        auto isPackageSelected = [&idVer, &selPkgs] (TextValue const &id, bool byDefault) -> bool {
            TextValue const key(idVer);
            if (selPkgs.contains(key)) {
                auto const &sels = selPkgs.element(key).as<DictionaryValue>();
                if (sels.contains(id)) {
                    return sels.element(id).isTrue();
                }
                else
                {
                    //LOG_MSG("'%s' not in selectedPackages of '%s'") << id << idVer;
                }
            }
            else
            {
                //LOG_MSG("'%s' not in fs.selectedPackages") << idVer;
            }
            return byDefault;
        };

        if (meta.has("recommends"))
        {
            for (auto const *id : meta.geta("recommends").elements())
            {
                String const pkgId = id->asText();
                if (isPackageSelected(pkgId, true))
                {
                    callback(pkgId);
                }
            }
        }
        if (meta.has("extras"))
        {
            for (auto const *id : meta.geta("extras").elements())
            {
                String const pkgId = id->asText();
                if (isPackageSelected(pkgId, false))
                {
                    callback(pkgId);
                }
            }
        }
    }

    void loadOptionalContent(File const &packageFile)
    {
        forOptionalContent(packageFile, [this] (String const &pkgId)
        {
            if (!self().isLoaded(pkgId))
            {
                self().load(pkgId);
            }
        });
    }

    List<Package *> loadedInOrder() const
    {
        List<Package *> pkgs;
        for (auto &i : loaded) pkgs << i.second;
        std::sort(pkgs.begin(), pkgs.end(), [] (Package const *a, Package const *b) {
            return a->order() < b->order();
        });
        return pkgs;
    }

    DE_PIMPL_AUDIENCE(Activity)
    DE_PIMPL_AUDIENCE(Load)
    DE_PIMPL_AUDIENCE(Unload)
};

DE_AUDIENCE_METHOD(PackageLoader, Activity)
DE_AUDIENCE_METHOD(PackageLoader, Load)
DE_AUDIENCE_METHOD(PackageLoader, Unload)

PackageLoader::PackageLoader() : d(new Impl(this))
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
    if (!packFile)
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
        DE_NOTIFY(Load, i)
        {
            i->packageLoaded(id);
        }
        DE_NOTIFY(Activity, i)
        {
            i->setOfLoadedPackagesChanged();
        }
    }
    catch (Error const &er)
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

    if (isLoaded(id))
    {
        DE_NOTIFY(Unload, i)
        {
            i->aboutToUnloadPackage(id);
        }

        d->unload(id);

        DE_NOTIFY(Activity, i)
        {
            i->setOfLoadedPackagesChanged();
        }
    }
}

void PackageLoader::unloadAll()
{
    LOG_AS("PackageLoader");
    LOG_RES_MSG("Unloading %i packages") << d->loaded.size();

    while (!d->loaded.isEmpty())
    {
        unload(d->loaded.begin()->first);
    }
}

void PackageLoader::refresh()
{
    LOG_AS("PackageLoader");
    FS::locate<Folder>("/packs").populate(Folder::PopulateOnlyThisFolder);
}

bool PackageLoader::isLoaded(String const &packageId) const
{
    // Split ID, check version too if specified.
    auto const id_ver = Package::split(packageId);
    auto found = d->loaded.find(id_ver.first);
    if (found == d->loaded.end())
    {
        return false;
    }
    return (!id_ver.second.isValid() /* no valid version provided in argumnet */ ||
            id_ver.second == found->second->version());
}

bool PackageLoader::isLoaded(File const &file) const
{
    return d->tryFindLoaded(file) != nullptr;
}

Package const *PackageLoader::tryFindLoaded(File const &file) const
{
    return d->tryFindLoaded(file);
}

PackageLoader::LoadedPackages const &PackageLoader::loadedPackages() const
{
    return d->loaded;
}

List<Package *> PackageLoader::loadedPackagesInOrder() const
{
    return d->loadedInOrder();
}

FS::FoundFiles PackageLoader::loadedPackagesAsFilesInPackageOrder() const
{
    List<Package *> pkgs = d->loadedInOrder();
    FS::FoundFiles sorted;
    for (auto &p : pkgs)
    {
        sorted.push_back(const_cast<File *>(&p->sourceFile()));
    }
    return sorted;
}

StringList PackageLoader::loadedPackageIdsInOrder(IdentifierType idType) const
{
    List<Package *> pkgs = d->loadedInOrder();
    StringList ids;
    for (auto p : pkgs)
    {
        Record const &meta = Package::metadata(p->file());
        Version const pkgVersion(meta.gets("version"));
        if (idType == Versioned && pkgVersion.isValid()) // nonzero
        {
            ids << Stringf("%s_%s", meta.gets("ID").c_str(), pkgVersion.fullNumber().c_str());
        }
        else
        {
            // Unspecified version.
            ids << meta.gets("ID");
        }
    }
    return ids;
}

Package const &PackageLoader::package(String const &packageId) const
{
    if (!isLoaded(packageId))
    {
        throw NotFoundError("PackageLoader::package", "Package '" + packageId + "' is not loaded");
    }
    return *d->loaded[packageId];
}

void PackageLoader::sortInPackageOrder(FS::FoundFiles &filesToSort) const
{
    typedef std::pair<File *, int> FileAndOrder;

    // Find the packages for files.
    List<FileAndOrder> all;
    for (auto *i : filesToSort)
    {
        Package const *pkg = 0;
        String identifier = Package::identifierForContainerOfFile(*i);
        if (isLoaded(identifier))
        {
            pkg = &package(identifier);
        }
        all << FileAndOrder(i, pkg? pkg->order() : -1);
    }

    // Sort by package order.
    std::sort(all.begin(), all.end(), [] (const FileAndOrder &a, const FileAndOrder &b) {
        return a.second < b.second;
    });

    // Put the results back in the given array.
    filesToSort.clear();
    for (const FileAndOrder &f : all)
    {
        filesToSort.push_back(f.first);
    }
}

StringList PackageLoader::loadedFromCommandLine() const
{
    StringList pkgs;
    CommandLine &args = App::commandLine();
    for (duint p = 0; p < duint(args.count()); )
    {
        // Find all the -pkg options.
        if (!args.matches("-pkg", args.at(p)))
        {
            ++p;
            continue;
        }
        // Load all the specified packages (by identifier, not by path).
        while (++p != duint(args.count()) && !args.isOption(p))
        {
            pkgs << args.at(p);
        }
    }
    return pkgs;
}

StringList PackageLoader::findAllPackages() const
{
    StringList all;
    for (const char *typeName : {DE_TYPE_NAME(Folder),
                                 DE_TYPE_NAME(ArchiveFolder),
                                 DE_TYPE_NAME(LinkFile)})
    {
        d->listPackagesInIndex(App::fileSystem().indexFor(typeName), all);
    }
    return all;
}

StringList PackageLoader::expandDependencies(StringList const &packageIdentifiers) const
{
    StringList expanded;
    for (String pkgId : packageIdentifiers)
    {
        if (File const *file = select(pkgId))
        {
            for (String reqId : Package::requiredPackages(*file))
            {
                expanded << reqId;
            }
            d->forOptionalContent(*file, [&expanded] (String const &pkgId)
            {
                expanded << pkgId;
            });
        }
        expanded << pkgId;
    }
    return expanded;
}

PackageLoader &PackageLoader::get()
{
    return App::packageLoader();
}

PackageLoader::IdentifierList::IdentifierList(const String &spaceSeparatedIds)
{
    static RegExp anySpace("\\s");
    for (const String &qs : spaceSeparatedIds.split(anySpace))
    {
        if (qs)
        {
            ids << qs;
        }
    }
}

} // namespace de
