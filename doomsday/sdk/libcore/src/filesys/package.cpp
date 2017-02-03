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
#include "de/App"
#include "de/DotPath"
#include "de/LogBuffer"
#include "de/PackageLoader"
#include "de/Process"
#include "de/Script"
#include "de/ScriptSystem"
#include "de/ScriptedInfo"
#include "de/TimeValue"

#include <QRegularExpression>

namespace de {

String const Package::VAR_PACKAGE      ("package");
String const Package::VAR_PACKAGE_ID   ("package.ID");
String const Package::VAR_PACKAGE_ALIAS("package.alias");
String const Package::VAR_PACKAGE_TITLE("package.title");
String const Package::VAR_ID           ("ID");
String const Package::VAR_TITLE        ("title");
String const Package::VAR_VERSION      ("version");

static String const PACKAGE_ORDER      ("package.__order__");
static String const PACKAGE_IMPORT_PATH("package.importPath");
static String const PACKAGE_REQUIRES   ("package.requires");
static String const PACKAGE_RECOMMENDS ("package.recommends");
static String const PACKAGE_EXTRAS     ("package.extras");
static String const PACKAGE_TAGS       ("package.tags");

static String const VAR_ID  ("ID");
static String const VAR_PATH("path");
static String const VAR_TAGS("tags");

Package::Asset::Asset(Record const &rec) : RecordAccessor(rec) {}

Package::Asset::Asset(Record const *rec) : RecordAccessor(rec) {}

String Package::Asset::absolutePath(String const &name) const
{
    // For the context, we'll accept either the variable's own record or the package
    // metadata.
    Record const *context = &accessedRecord().parentRecordForMember(name);
    if (!context->has(ScriptedInfo::VAR_SOURCE))
    {
        context = &accessedRecord();
    }
    return ScriptedInfo::absolutePathInContext(*context, gets(name));
}

DENG2_PIMPL(Package)
, DENG2_OBSERVES(File, Deletion)
{
    File const *file;
    Version version; // version of the loaded package

    Impl(Public *i, File const *f)
        : Base(i)
        , file(f)
    {
        if (file)
        {
            file->audienceForDeletion() += this;

            // Check the file name first, then metadata.
            version = split(versionedIdentifierForFile(*file)).second;
            if (!version.isValid())
            {
                version = Version(metadata(*file).gets(VAR_VERSION, String()));
            }
        }
    }

    void fileBeingDeleted(File const &)
    {
        file = 0;
    }

    void verifyFile() const
    {
        if (!file)
        {
            throw SourceError("Package::verify", "Package's source file missing");
        }
    }

    StringList importPaths() const
    {
        StringList paths;
        if (self().objectNamespace().has(PACKAGE_IMPORT_PATH))
        {
            ArrayValue const &imp = self().objectNamespace().geta(PACKAGE_IMPORT_PATH);
            DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, imp.elements())
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

Package::Package(File const &file) : d(new Impl(this, &file))
{}

Package::~Package()
{}

File const &Package::file() const
{
    d->verifyFile();
    return *d->file;
}

File const &Package::sourceFile() const
{
    return App::rootFolder().locate<File const>(objectNamespace().gets("package.path"));
}

Folder const &Package::root() const
{
    d->verifyFile();
    return d->file->expectedAs<Folder>();
}

Record &Package::objectNamespace()
{
    d->verifyFile();
    return const_cast<File *>(d->file)->objectNamespace();
}

Record const &Package::objectNamespace() const
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
    return ScriptedInfo::allBlocksOfType(QStringLiteral("asset"), d->packageInfo());
}

bool Package::executeFunction(String const &name)
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

void Package::findPartialPath(String const &path, FileIndex::FoundFiles &found) const
{
    App::fileSystem().nameIndex().findPartialPath(identifier(), path, found);
}

void Package::didLoad()
{
    // The package's own import paths come into effect when loaded.
    foreach (String imp, d->importPaths())
    {
        App::scriptSystem().addModuleImportPath(imp);
    }

    executeFunction("onLoad");
}

void Package::aboutToUnload()
{
    executeFunction("onUnload");

    foreach (String imp, d->importPaths())
    {
        App::scriptSystem().removeModuleImportPath(imp);
    }

    // Not loaded any more, so doesn't have an ordinal.
    delete objectNamespace().remove(PACKAGE_ORDER);
}

void Package::parseMetadata(File &packageFile) // static
{
    static String const TIMESTAMP("__timestamp__");

    if (Folder *folder = packageFile.maybeAs<Folder>())
    {
        File *initializerScript = folder->tryLocateFile("__init__.de");
        File *metadataInfo      = folder->tryLocateFile("Info.dei");
        if (!metadataInfo) metadataInfo = folder->tryLocateFile("Info"); // alternate name
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
                if (TimeValue const *time = metadata.get(TIMESTAMP).maybeAs<TimeValue>())
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

void Package::validateMetadata(Record const &packageInfo)
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
                              QString("Identifier of package \"%1\" must specify a domain")
                              .arg(packageInfo.gets("path")));
    }

    String const &topLevelDomain = ident.segment(0).toString();
    if (topLevelDomain == QStringLiteral("feature") ||
        topLevelDomain == QStringLiteral("asset"))
    {
        // Functional top-level domains cannot be used as package identifiers (only aliases).
        throw ValidationError("Package::validateMetadata",
                              QString("Package \"%1\" has an invalid domain: functional top-level "
                                      "domains can only be used as aliases")
                              .arg(packageInfo.gets("path")));
    }

    static String const required[] = { "title", "version", "license", VAR_TAGS };
    for (auto const &req : required)
    {
        if (!packageInfo.has(req))
        {
            throw IncompleteMetadataError("Package::validateMetadata",
                                          QString("Package \"%1\" does not have '%2' in its metadata")
                                          .arg(packageInfo.gets("path"))
                                          .arg(req));
        }
    }

    static QRegularExpression const regexReservedTags("\\b(loaded)\\b");
    auto match = regexReservedTags.match(packageInfo.gets(VAR_TAGS));
    if (match.hasMatch())
    {
        throw ValidationError("Package::validateMetadata",
                              QString("Package \"%1\" has a tag that is reserved for internal use (%2)")
                              .arg(packageInfo.gets("path"))
                              .arg(match.captured(1)));
    }
}

Record &Package::initializeMetadata(File &packageFile, String const &id)
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

Record const &Package::metadata(File const &packageFile)
{
    return packageFile.objectNamespace().subrecord(VAR_PACKAGE);
}

QStringList Package::tags(File const &packageFile)
{
    return tags(packageFile.objectNamespace().gets(PACKAGE_TAGS));
}

bool Package::matchTags(File const &packageFile, String const &tagRegExp)
{
    return QRegExp(tagRegExp).indexIn(packageFile.objectNamespace().gets(PACKAGE_TAGS, "")) >= 0;
}

QStringList Package::tags(String const &tagsString)
{
    return tagsString.split(' ', QString::SkipEmptyParts);
}

StringList Package::requires(File const &packageFile)
{
    return packageFile.objectNamespace().getStringList(PACKAGE_REQUIRES);
}

void Package::addRequiredPackage(File &packageFile, String const &id)
{
    packageFile.objectNamespace().appendToArray(PACKAGE_REQUIRES, new TextValue(id));
}

bool Package::hasOptionalContent(String const &packageId)
{
    if (File const *file = PackageLoader::get().select(packageId))
    {
        return hasOptionalContent(*file);
    }
    return false;
}

bool Package::hasOptionalContent(File const &packageFile)
{
    Record const &meta = packageFile.objectNamespace();
    return meta.has(PACKAGE_RECOMMENDS) || meta.has(PACKAGE_EXTRAS);
}

static String stripAfterFirstUnderscore(String str)
{
    int pos = str.indexOf('_');
    if (pos > 0) return str.left(pos);
    return str;
}

static String extractIdentifier(String str)
{
    return stripAfterFirstUnderscore(str.fileNameWithoutExtension());
}

std::pair<String, Version> Package::split(String const &identifier_version)
{
    std::pair<String, Version> idVer;

    if (identifier_version.contains(QChar('_')))
    {
        idVer.first  = stripAfterFirstUnderscore(identifier_version);
        idVer.second = Version(identifier_version.substr(idVer.first.size() + 1));
    }
    else
    {
        idVer.first  = identifier_version;
    }
    return idVer;
}

bool Package::equals(String const &id1, String const &id2)
{
    return split(id1).first == split(id2).first;
}

String Package::identifierForFile(File const &file)
{
    // Form the prefix if there are enclosing packs as parents.
    String prefix;
    Folder const *parent = file.parent();
    while (parent && parent->extension() == ".pack")
    {
        prefix = extractIdentifier(parent->name()) + "." + prefix;
        parent = parent->parent();
    }
    return prefix + extractIdentifier(file.name());
}

String Package::versionedIdentifierForFile(File const &file)
{
    String id = identifierForFile(file);
    if (id.isEmpty()) return String();
    auto const id_ver = split(file.name().fileNameWithoutExtension());
    if (id_ver.second.isValid())
    {
        return String("%1_%2").arg(id).arg(id_ver.second.asText());
    }
    return id;
}

Version Package::versionForFile(File const &file)
{
    return split(versionedIdentifierForFile(file)).second;
}

File const *Package::containerOfFile(File const &file)
{
    // Find the containing package.
    File const *i = file.parent();
    while (i && i->extension() != ".pack")
    {
        i = i->parent();
    }
    return i;
}

String Package::identifierForContainerOfFile(File const &file)
{
    // Find the containing package.
    File const *c = containerOfFile(file);
    return c? identifierForFile(*c) : "";
}

Time Package::containerOfFileModifiedAt(File const &file)
{
    File const *c = containerOfFile(file);
    if (!c) return file.status().modifiedAt;
    return c->status().modifiedAt;
}

} // namespace de
