/** @file package.cpp  Package containing metadata, data, and/or files.
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

#include "de/package.h"

#include "de/app.h"
#include "de/path.h"
#include "de/logbuffer.h"
#include "de/packageloader.h"
#include "de/dscript.h"
#include "de/regexp.h"
#include "de/timevalue.h"

namespace de {

String const Package::VAR_PACKAGE      ("package");
String const Package::VAR_PACKAGE_ID   ("package.ID");
String const Package::VAR_PACKAGE_ALIAS("package.alias");
String const Package::VAR_PACKAGE_TITLE("package.title");
String const Package::VAR_ID           ("ID");
String const Package::VAR_TITLE        ("title");
String const Package::VAR_VERSION      ("version");

static String const PACKAGE_VERSION    ("package.version");
static String const PACKAGE_ORDER      ("package.__order__");
static String const PACKAGE_IMPORT_PATH("package.importPath");
static String const PACKAGE_REQUIRES   ("package.requires");
static String const PACKAGE_RECOMMENDS ("package.recommends");
static String const PACKAGE_EXTRAS     ("package.extras");
static String const PACKAGE_PATH       ("package.path");
static String const PACKAGE_TAGS       ("package.tags");

static String const VAR_ID  ("ID");
static String const VAR_PATH("path");
static String const VAR_TAGS("tags");

Package::Asset::Asset(const Record &rec) : RecordAccessor(rec) {}

Package::Asset::Asset(const Record *rec) : RecordAccessor(rec) {}

String Package::Asset::absolutePath(const String &name) const
{
    // For the context, we'll accept either the variable's own record or the package
    // metadata.
    const Record *context = &accessedRecord().parentRecordForMember(name);
    if (!context->has(ScriptedInfo::VAR_SOURCE))
    {
        context = &accessedRecord();
    }
    return ScriptedInfo::absolutePathInContext(*context, gets(name));
}

DE_PIMPL(Package)
{
    SafePtr<const File> file;
    Version version; // version of the loaded package

    Impl(Public *i, const File *f)
        : Base(i)
        , file(f)
    {
        if (file)
        {
            // Check the file name first, then metadata.
            version = split(versionedIdentifierForFile(*file)).second;
            if (!version.isValid())
            {
                version = Version(metadata(*file).gets(VAR_VERSION, String()));
            }
        }
    }

    void verifyFile() const
    {
        if (!file)
        {
            throw SourceError("Package::verifyFile", "Package's source file missing");
        }
    }

    StringList importPaths() const
    {
        StringList paths;
        if (self().objectNamespace().has(PACKAGE_IMPORT_PATH))
        {
            const ArrayValue &imp = self().objectNamespace().geta(PACKAGE_IMPORT_PATH);
            DE_FOR_EACH_CONST(ArrayValue::Elements, i, imp.elements())
            {
                Path importPath = (*i)->asText();
                if (!importPath.isAbsolute())
                {
                    // Relative to the package root, and must exist.
                    importPath = self().root().locate<File const >(importPath).path();
                }
                paths << importPath;
            }
        }
        return paths;
    }

    Record &packageInfo()
    {
        return self().objectNamespace().subrecord(VAR_PACKAGE);
    }
};

Package::Package(const File &file) : d(new Impl(this, &file))
{}

Package::~Package()
{}

const File &Package::file() const
{
    d->verifyFile();
    return *d->file;
}

const File &Package::sourceFile() const
{
    return FS::locate<const File>(objectNamespace().gets(PACKAGE_PATH));
}

bool Package::sourceFileExists() const
{
    return d->file && FS::tryLocate<const File>(objectNamespace().gets(PACKAGE_PATH));
}

const Folder &Package::root() const
{
    d->verifyFile();
    if (const Folder *f = maybeAs<Folder>(&d->file->target()))
    {
        return *f;
    }
    return *sourceFile().parent();
}

Record &Package::objectNamespace()
{
    d->verifyFile();
    return const_cast<File *>(d->file.get())->objectNamespace();
}

const Record &Package::objectNamespace() const
{
    return const_cast<Package *>(this)->objectNamespace();
}

String Package::identifier() const
{
    d->verifyFile();
    return identifierForFile(*d->file);
}

Version Package::version() const
{
    return d->version;
}

Package::Assets Package::assets() const
{
    return ScriptedInfo::allBlocksOfType(DE_STR("asset"), d->packageInfo());
}

bool Package::executeFunction(const String &name)
{
    Record &pkgInfo = d->packageInfo();
    if (pkgInfo.has(name))
    {
        // The global namespace for this function is the package's info namespace.
        Process::scriptCall(Process::IgnoreResult, pkgInfo, name);
        return true;
    }
    return false;
}

void Package::setOrder(int ordinal)
{
    objectNamespace().set(PACKAGE_ORDER, ordinal);
}

int Package::order() const
{
    return objectNamespace().geti(PACKAGE_ORDER);
}

void Package::findPartialPath(const String &path, FileIndex::FoundFiles &found) const
{
    App::fileSystem().nameIndex().findPartialPath(identifier(), path, found);
}

void Package::didLoad()
{
    // The package's own import paths come into effect when loaded.
    for (const String &imp : d->importPaths())
    {
        App::scriptSystem().addModuleImportPath(imp);
    }

    executeFunction("onLoad");
}

void Package::aboutToUnload()
{
    executeFunction("onUnload");

    for (const String &imp : d->importPaths())
    {
        App::scriptSystem().removeModuleImportPath(imp);
    }

    // Not loaded any more, so doesn't have an ordinal.
    delete objectNamespace().remove(PACKAGE_ORDER);
}

void Package::parseMetadata(File &packageFile) // static
{
    static String const TIMESTAMP("__timestamp__");

    if (Folder *folder = maybeAs<Folder>(packageFile))
    {
        File *initializerScript = folder->tryLocateFile(DE_STR("__init__.ds"));
        if (!initializerScript) initializerScript = folder->tryLocateFile(DE_STR("__init__.de")); // old extension
        File *metadataInfo      = folder->tryLocateFile(DE_STR("Info.dei"));
        if (!metadataInfo) metadataInfo = folder->tryLocateFile(DE_STR("Info")); // alternate name
        Time parsedAt           = Time::invalidTime();
        bool needParse          = true;

        if (!metadataInfo && !initializerScript) return; // Nothing to do.

        // If the metadata has already been parsed, we may not need to do much.
        // The package's information is stored in a subrecord.
        if (packageFile.objectNamespace().has(VAR_PACKAGE))
        {
            Record &metadata = packageFile.objectNamespace().subrecord(VAR_PACKAGE);
            if (metadata.has(TIMESTAMP))
            {
                // Already parsed.
                needParse = false;

                // Only parse if the source has changed.
                if (const auto *time = maybeAs<TimeValue>(metadata.get(TIMESTAMP)))
                {
                    needParse =
                            (metadataInfo      && metadataInfo->status().modifiedAt      > time->time()) ||
                            (initializerScript && initializerScript->status().modifiedAt > time->time());
                }
            }
            if (!needParse) return;
        }

        // The package identifier and path are automatically set.
        Record &metadata = initializeMetadata(packageFile);

        // Check for a ScriptedInfo source.
        if (metadataInfo)
        {
            ScriptedInfo script(&metadata);
            script.parse(*metadataInfo);

            parsedAt = metadataInfo->status().modifiedAt;
        }

        // Check for an initialization script.
        if (initializerScript)
        {
            Script script(*initializerScript);
            Process proc(&metadata);
            proc.run(script);
            proc.execute();

            if (parsedAt.isValid() && initializerScript->status().modifiedAt > parsedAt)
            {
                parsedAt = initializerScript->status().modifiedAt;
            }
        }

        metadata.addTime(TIMESTAMP, parsedAt);

        if (LogBuffer::get().isEnabled(LogEntry::Dev | LogEntry::XVerbose | LogEntry::Resource))
        {
            LOGDEV_RES_XVERBOSE("Parsed metadata of '%s':\n" _E(m),
                    identifierForFile(packageFile)
                    << packageFile.objectNamespace().asText());
        }
    }
}

void Package::validateMetadata(const Record &packageInfo)
{
    if (!packageInfo.has(VAR_ID))
    {
        throw NotPackageError("Package::validateMetadata", "Not a package");
    }

    // A domain is required in all package identifiers.
    DotPath const ident(packageInfo.gets(VAR_ID));

    if (ident.segmentCount() <= 1)
    {
        throw ValidationError("Package::validateMetadata",
                              stringf("Identifier of package \"%s\" must specify a domain",
                                      packageInfo.gets("path").c_str()));
    }

    const String topLevelDomain = ident.segment(0).toLowercaseString();
    if (topLevelDomain == DE_STR("feature") ||
        topLevelDomain == DE_STR("asset"))
    {
        // Functional top-level domains cannot be used as package identifiers (only aliases).
        throw ValidationError("Package::validateMetadata",
                              stringf("Package \"%s\" has an invalid domain: functional top-level "
                                      "domains can only be used as aliases",
                                      packageInfo.gets("path").c_str()));
    }

    static const String required[] = { "title", "version", "license", VAR_TAGS };
    for (const auto &req : required)
    {
        if (!packageInfo.has(req))
        {
            debug("metadata:\n%s\n", packageInfo.asText().c_str());

            throw IncompleteMetadataError(
                "Package::validateMetadata",
                stringf("Package \"%s\" does not have '%s' in its metadata",
                        packageInfo.gets("path").c_str(),
                        req.c_str()));
        }
    }

    static RegExp const regexReservedTags("\\b(loaded)\\b");
    RegExpMatch match;
    if (regexReservedTags.match(packageInfo.gets(VAR_TAGS), match))
    {
        throw ValidationError(
            "Package::validateMetadata",
            stringf("Package \"%s\" has a tag that is reserved for internal use (%s)",
                    packageInfo.gets("path").c_str(),
                    match.captured(1).c_str()));
    }
}

Record &Package::initializeMetadata(File &packageFile, const String &id)
{
    if (!packageFile.objectNamespace().has(VAR_PACKAGE))
    {
        packageFile.objectNamespace().addSubrecord(VAR_PACKAGE);
    }

    Record &metadata = packageFile.objectNamespace().subrecord(VAR_PACKAGE);
    metadata.set(VAR_ID,   id.isEmpty()? identifierForFile(packageFile) : id);
    metadata.set(VAR_PATH, packageFile.path());
    return metadata;
}

const Record &Package::metadata(const File &packageFile)
{
    return packageFile.objectNamespace().subrecord(VAR_PACKAGE);
}

StringList Package::tags(const File &packageFile)
{
    return tags(packageFile.objectNamespace().gets(PACKAGE_TAGS));
}

bool Package::matchTags(const File &packageFile, const String &tagRegExp)
{
    return RegExp(tagRegExp).hasMatch(packageFile.objectNamespace().gets(PACKAGE_TAGS, ""));
}

StringList Package::tags(const String &tagsString)
{
    return filter(tagsString.split(" "), [](const String &s) { return bool(s); });
}

StringList Package::requiredPackages(const File &packageFile)
{
    return packageFile.objectNamespace().getStringList(PACKAGE_REQUIRES);
}

void Package::addRequiredPackage(File &packageFile, const String &id)
{
    packageFile.objectNamespace().appendToArray(PACKAGE_REQUIRES, new TextValue(id));
}

bool Package::hasOptionalContent(const String &packageId)
{
    if (const File *file = PackageLoader::get().select(packageId))
    {
        return hasOptionalContent(*file);
    }
    return false;
}

bool Package::hasOptionalContent(const File &packageFile)
{
    const Record &meta = packageFile.objectNamespace();
    return meta.has(PACKAGE_RECOMMENDS) || meta.has(PACKAGE_EXTRAS);
}

static String stripAfterFirstUnderscore(String str)
{
    auto pos = str.indexOf('_');
    if (pos > 0) return str.left(pos);
    return str;
}

static String extractIdentifier(String str)
{
    return stripAfterFirstUnderscore(str.fileNameWithoutExtension());
}

std::pair<String, Version> Package::split(const String &identifier_version)
{
    std::pair<String, Version> idVer;
    if (identifier_version.contains("_"))
    {
        idVer.first  = stripAfterFirstUnderscore(identifier_version);
        idVer.second = Version(identifier_version.substr(idVer.first.sizeb() + 1));
    }
    else
    {
        idVer.first  = identifier_version;
    }
    return idVer;
}

String Package::splitToHumanReadable(const String &identifier_version)
{
    const auto id_ver = split(identifier_version);
    return Stringf("%s " _E(C) "(%s)" _E(.),
                          id_ver.first.c_str(),
                          id_ver.second.isValid()
                              ? stringf("version %s", id_ver.second.fullNumber().c_str()).c_str()
                              : "any version");
}

bool Package::equals(const String &id1, const String &id2)
{
    return split(id1).first == split(id2).first;
}

String Package::identifierForFile(const File &file)
{
    // The ID may be specified in the metadata.
    if (const auto *pkgId = file.objectNamespace().tryFind(VAR_PACKAGE_ID))
    {
        return pkgId->value().asText();
    }

    // Form the prefix if there are enclosing packs as parents.
    String prefix;
    const Folder *parent = file.parent();
    while (parent && parent->extension() == ".pack")
    {
        prefix = extractIdentifier(parent->name()) + "." + prefix;
        parent = parent->parent();
    }
    return prefix + extractIdentifier(file.name());
}

String Package::versionedIdentifierForFile(const File &file)
{
    String id = identifierForFile(file);
    if (id.isEmpty()) return String();
    const auto id_ver = split(file.name().fileNameWithoutExtension());
    if (id_ver.second.isValid())
    {
        return Stringf("%s_%s", id.c_str(), id_ver.second.fullNumber().c_str());
    }
    // The version may be specified in metadata.
    if (const auto *pkgVer = file.objectNamespace().tryFind(PACKAGE_VERSION))
    {
        return Stringf("%s_%s", id.c_str(), Version(pkgVer->value().asText()).fullNumber().c_str());
    }
    return id;
}

Version Package::versionForFile(const File &file)
{
    return split(versionedIdentifierForFile(file)).second;
}

const File *Package::containerOfFile(const File &file)
{
    // Find the containing package.
    const File *i = file.parent();
    while (i && i->extension() != ".pack")
    {
        i = i->parent();
    }
    return i;
}

String Package::identifierForContainerOfFile(const File &file)
{
    // Find the containing package.
    const File *c = containerOfFile(file);
    return c? identifierForFile(*c) : "";
}

Time Package::containerOfFileModifiedAt(const File &file)
{
    const File *c = containerOfFile(file);
    if (!c) return file.status().modifiedAt;
    return c->status().modifiedAt;
}

} // namespace de
