/** @file package.cpp  Package containing metadata, data, and/or files.
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

#include "de/Package"
#include "de/PackageLoader"
#include "de/Process"
#include "de/Script"
#include "de/ScriptedInfo"
#include "de/TimeValue"
#include "de/DotPath"
#include "de/App"

namespace de {

static String const PACKAGE("package");
static String const PACKAGE_ORDER("package.__order__");
static String const PACKAGE_IMPORT_PATH("package.importPath");

Package::Asset::Asset(Record const &rec) : RecordAccessor(rec) {}

Package::Asset::Asset(Record const *rec) : RecordAccessor(rec) {}

String Package::Asset::absolutePath(String const &name) const
{
    // For the context, we'll accept either the variable's own record or the package
    // metadata.
    Record const *context = &accessedRecord().parentRecordForMember(name);
    if(!context->has("__source__"))
    {
        context = &accessedRecord();
    }
    return ScriptedInfo::absolutePathInContext(*context, gets(name));
}

DENG2_PIMPL(Package)
, DENG2_OBSERVES(File, Deletion)
{
    File const *file;

    Instance(Public *i, File const *f)
        : Base(i)
        , file(f)
    {
        if(file) file->audienceForDeletion() += this;
    }

    ~Instance()
    {
        if(file) file->audienceForDeletion() -= this;
    }

    void fileBeingDeleted(File const &)
    {
        file = 0;
    }

    void verifyFile() const
    {
        if(!file)
        {
            throw SourceError("Package::verify", "Package's source file missing");
        }
    }

    StringList importPaths() const
    {
        StringList paths;
        if(self.info().has(PACKAGE_IMPORT_PATH))
        {
            ArrayValue const &imp = self.info().geta(PACKAGE_IMPORT_PATH);
            DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, imp.elements())
            {
                // The import paths are relative to the package root, and must exist.
                paths << self.root().locate<File const>((*i)->asText()).path();
            }
        }
        return paths;
    }

    Record &packageInfo()
    {
        return self.info().subrecord(PACKAGE);
    }
};

Package::Package(File const &file) : d(new Instance(this, &file))
{}

Package::~Package()
{}

File const &Package::file() const
{
    d->verifyFile();
    return *d->file;
}

Folder const &Package::root() const
{
    d->verifyFile();
    return d->file->as<Folder>();
}

Record &Package::info()
{
    d->verifyFile();
    return const_cast<File *>(d->file)->info();
}

Record const &Package::info() const
{
    return const_cast<Package *>(this)->info();
}

String Package::identifier() const
{
    d->verifyFile();
    return identifierForFile(*d->file);
}

Package::Assets Package::assets() const
{
    return ScriptedInfo::allBlocksOfType("asset", d->packageInfo());
}

bool Package::executeFunction(String const &name)
{
    Record &pkgInfo = d->packageInfo();
    if(pkgInfo.has(name))
    {
        Script script(name + "()");
        // The global namespace for this function is the package's info namespace.
        Process proc(&pkgInfo);
        proc.run(script);
        proc.execute();
        return true;
    }
    return false;
}

void Package::setOrder(int ordinal)
{
    info().set(PACKAGE_ORDER, ordinal);
}

int Package::order() const
{
    return info().geti(PACKAGE_ORDER);
}

void Package::didLoad()
{
    // The package's own import paths come into effect when loaded.
    foreach(String imp, d->importPaths())
    {
        App::scriptSystem().addModuleImportPath(imp);
    }

    executeFunction("onLoad");
}

void Package::aboutToUnload()
{
    executeFunction("onUnload");

    foreach(String imp, d->importPaths())
    {
        App::scriptSystem().removeModuleImportPath(imp);
    }

    // Not loaded any more, so doesn't have an ordinal.
    delete info().remove(PACKAGE_ORDER);
}

void Package::parseMetadata(File &packageFile) // static
{
    static String const TIMESTAMP("__timestamp__");

    if(Folder *folder = packageFile.maybeAs<Folder>())
    {
        // The package's information is stored in a subrecord.
        if(!packageFile.info().has(PACKAGE))
        {
            packageFile.info().addRecord(PACKAGE);
        }

        Record &metadata        = packageFile.info().subrecord(PACKAGE);
        File *metadataInfo      = folder->tryLocateFile("Info");
        File *initializerScript = folder->tryLocateFile("__init__.de");
        Time parsedAt           = Time::invalidTime();
        bool needParse          = true;

        if(!metadataInfo && !initializerScript) return; // Nothing to do.

        if(metadata.has(TIMESTAMP))
        {
            // Already parsed.
            needParse = false;

            // Only parse if the source has changed.
            if(TimeValue const *time = metadata.get(TIMESTAMP).maybeAs<TimeValue>())
            {
                needParse =
                        (metadataInfo      && metadataInfo->status().modifiedAt      > time->time()) ||
                        (initializerScript && initializerScript->status().modifiedAt > time->time());
            }
        }

        if(!needParse) return;

        // The package identifier and path are automatically set.
        metadata.set("id", identifierForFile(packageFile));
        metadata.set("path", packageFile.path());

        // Check for a ScriptedInfo source.
        if(metadataInfo)
        {
            ScriptedInfo script(&metadata);
            script.parse(*metadataInfo);

            parsedAt = metadataInfo->status().modifiedAt;
        }

        // Check for an initialization script.
        if(initializerScript)
        {
            Script script(*initializerScript);
            Process proc(&metadata);
            proc.run(script);
            proc.execute();

            if(parsedAt.isValid() && initializerScript->status().modifiedAt > parsedAt)
            {
                parsedAt = initializerScript->status().modifiedAt;
            }
        }

        metadata.addTime(TIMESTAMP, parsedAt);

        LOG_RES_MSG("Parsed metadata of '%s':\n")
                << identifierForFile(packageFile)
                << packageFile.info().asText();
    }
}

void Package::validateMetadata(Record const &packageInfo)
{
    // A domain is required in all package identifiers.
    DotPath const ident(packageInfo.gets("id"));

    if(ident.segmentCount() <= 1)
    {
        throw ValidationError("Package::validateMetadata",
                              QString("Identifier of package \"%1\" must specify a domain")
                              .arg(packageInfo.gets("path")));
    }

    String const &topLevelDomain = ident.segment(0).toString();
    if(topLevelDomain == "feature" || topLevelDomain == "asset")
    {
        // Functional top-level domains cannot be used as package identifiers (only aliases).
        throw ValidationError("Package::validateMetadata",
                              QString("Package \"%1\" has an invalid domain: functional top-level "
                                      "domains can only be used as aliases")
                              .arg(packageInfo.gets("path")));
    }

    char const *required[] = { "title", "version", "license", "tags", 0 };

    for(int i = 0; required[i]; ++i)
    {
        if(!packageInfo.has(required[i]))
        {
            throw IncompleteMetadataError("Package::validateMetadata",
                                          QString("Package \"%1\" does not have '%2' in its metadata")
                                          .arg(packageInfo.gets("path"))
                                          .arg(required[i]));
        }
    }
}

static String stripAfterFirstUnderscore(String str)
{
    int pos = str.indexOf('_');
    if(pos > 0) return str.left(pos);
    return str;
}

static String extractIdentifier(String str)
{
    return stripAfterFirstUnderscore(str.fileNameWithoutExtension());
}

String Package::identifierForFile(File const &file)
{
    // Form the prefix if there are enclosing packs as parents.
    String prefix;
    Folder const *parent = file.parent();
    while(parent && parent->name().fileNameExtension() == ".pack")
    {
        prefix = extractIdentifier(parent->name()) + "." + prefix;
        parent = parent->parent();
    }
    return prefix + extractIdentifier(file.name());
}

String Package::identifierForContainerOfFile(File const &file)
{
    // Find the containing package.
    File const *i = file.parent();
    while(i && i->name().fileNameExtension() != ".pack")
    {
        i = i->parent();
    }
    return i? identifierForFile(*i) : "";
}

} // namespace de
