/** @file savedsession.cpp  Saved (game) session.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include "de/game/Savedsession"

#include "de/App"
#include "de/ArrayValue"
#include "de/game/Game"
#include "de/game/MapStateReader"
#include "de/game/SavedSessionRepository"
#include "de/Info"
#include "de/Log"
#include "de/NumberValue"
#include "de/NativePath"
#include "de/PackageFolder"
#include "de/Writer"

namespace de {
namespace game {

static String const BLOCK_GROUP    = "group";
static String const BLOCK_GAMERULE = "gamerule";

void SavedSession::Metadata::parse(String const &source)
{
    clear();

    /// @todo Info keys are converted to lowercase when parsed?
    try
    {
        Info info;
        info.setAllowDuplicateBlocksOfType(QStringList() << BLOCK_GROUP << BLOCK_GAMERULE);
        info.parse(source);

        // Rebuild the game rules subrecord.
        Record &rules = addRecord("gameRules");
        foreach(Info::Element const *elem, info.root().contentsInOrder())
        {
            if(!elem->isBlock()) continue;

            // Perhaps a ruleset group?
            Info::BlockElement const &groupBlock = elem->as<Info::BlockElement>();
            if(groupBlock.blockType() == BLOCK_GROUP)
            {
                foreach(Info::Element const *grpElem, groupBlock.contentsInOrder())
                {
                    if(!grpElem->isBlock()) continue;

                    // Perhaps a gamerule?
                    Info::BlockElement const &ruleBlock = grpElem->as<Info::BlockElement>();
                    if(ruleBlock.blockType() == BLOCK_GAMERULE)
                    {
                        rules.set(ruleBlock.name(), ruleBlock.keyValue("value").text);
                    }
                }
            }
        }

        // Apply the other known values.
        String value;
        if(info.findValueForKey("gameidentitykey", value))
        {
            set("gameIdentityKey", value);
        }
        if(info.findValueForKey("maptime", value))
        {
            set("mapTime", value.toInt());
        }
        if(info.findValueForKey("mapuri", value))
        {
            set("mapUri", value);
        }
        if(info.findValueForKey("sessionid", value))
        {
            set("sessionId", value.toInt());
        }
        if(info.findValueForKey("userdescription", value))
        {
            set("userDescription", value);
        }
        if(info.findValueForKey("version", value))
        {
            set("version", value.toInt());
        }
    }
    catch(de::Error const &er)
    {
        LOG_WARNING(er.asText());
    }
}

String SavedSession::Metadata::asTextWithInfoSyntax() const
{
    String text;
    QTextStream os(&text);
    os.setCodec("UTF-8");

    if(has("gameIdentityKey")) os <<   "gameIdentityKey: " << gets("gameIdentityKey");
    if(has("mapTime"))         os << "\nmapTime: "         << String::number(geti("mapTime"));
    if(has("mapUri"))          os << "\nmapUri: "          << gets("mapUri");
    if(has("players"))         os << "\nplayers: "         << geta("players").asText();
    if(has("sessionId"))       os << "\nsessionId: "       << String::number(geti("sessionId"));
    if(has("userDescription")) os << "\nuserDescription: " << gets("userDescription");
    if(has("version"))         os << "\nversion: "         << String::number(geti("version"));

    if(hasSubrecord("gameRules"))
    {
        os << "\n" << BLOCK_GROUP << " ruleset {";

        Record const &rules = subrecord("gameRules");
        for(Record::Members::const_iterator i = rules.members().begin();
            i != rules.members().end(); ++i)
        {
            os << "\n    " << BLOCK_GAMERULE << " \"" << i.key() << "\""
               << " { value= \"" << i.value()->value().asText().replace("\"", "''") << "\" }";
        }

        os << "\n}";
    }

    return text;
}

DENG2_PIMPL(SavedSession)
{
    SavedSessionRepository *repo; ///< The owning repository (if any).

    String fileName; ///< Name of the game session file.

    QScopedPointer<Metadata> metadata;
    Status status;
    bool needUpdateStatus;

    Instance(Public *i)
        : Base(i)
        , repo            (0)
        , metadata        (new Metadata)
        , status          (Unused)
        , needUpdateStatus(true)
    {}

    Instance(Public *i, Instance const &other)
        : Base(i)
        , repo            (other.repo)
        , fileName        (other.fileName)
        , status          (other.status)
        , needUpdateStatus(other.needUpdateStatus)
    {
        DENG2_ASSERT(!other.metadata.isNull());
        metadata.reset(new Metadata(*other.metadata));
    }

    static SessionMetadata *readMetadata(PackageFolder const &pack)
    {
        if(!pack.has("Info")) return 0;

        try
        {
            File const &file = pack.locate<File const>("Info");
            Block raw;
            file >> raw;

            QScopedPointer<SessionMetadata> metadata(new SessionMetadata);
            metadata->parse(String::fromUtf8(raw));

            // Future version?
            if(metadata->geti("version") > 14) return 0;

            // So far so good.
            return metadata.take();
        }
        catch(IByteArray::OffsetError const &)
        {
            LOG_RES_WARNING("Archive in %s is truncated") << pack.description();
        }
        catch(IIStream::InputError const &)
        {
            LOG_RES_WARNING("%s cannot be read") << pack.description();
        }
        catch(Archive::FormatError const &)
        {
            LOG_RES_WARNING("Archive in %s is invalid") << pack.description();
        }

        return 0;
    }

    void notifyMetadataChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(MetadataChange, i)
        {
            i->savedSessionMetadataChanged(self);
        }
    }

    PackageFolder *tryLocatePackage()
    {
        if(!repo) return false;
        return repo->folder().tryLocate<PackageFolder>(fileName);
    }

    void updateStatusIfNeeded()
    {
        if(!needUpdateStatus) return;

        needUpdateStatus = false;
        LOGDEV_XVERBOSE("Updating SavedSession %p status") << thisPublic;
        Status const oldStatus = status;

        status = Unused;
        if(self.hasFile())
        {
            status = Incompatible;
            // Game identity key missmatch?
            if(!metadata->gets("gameIdentityKey", "").compareWithoutCase(App::game().id()))
            {
                /// @todo Validate loaded add-ons and checksum the definition database.
                status = Loadable; // It's good!
            }
        }

        if(status != oldStatus)
        {
            DENG2_FOR_PUBLIC_AUDIENCE2(StatusChange, i)
            {
                i->savedSessionStatusChanged(self);
            }
        }
    }

    DENG2_PIMPL_AUDIENCE(StatusChange)
    DENG2_PIMPL_AUDIENCE(MetadataChange)
};

DENG2_AUDIENCE_METHOD(SavedSession, StatusChange)
DENG2_AUDIENCE_METHOD(SavedSession, MetadataChange)

SavedSession::SavedSession(String const &fileName) : d(new Instance(this))
{
    d->fileName = fileName;
    if(!d->fileName.isEmpty() && d->fileName.fileNameExtension().isEmpty())
    {
        d->fileName += ".save";
    }
}

SavedSession::SavedSession(SavedSession const &other) : d(new Instance(this, *other.d))
{}

SavedSession &SavedSession::operator = (SavedSession const &other)
{
    d.reset(new Instance(this, *other.d));
    return *this;
}

SavedSessionRepository &SavedSession::repository() const
{
    if(d->repo)
    {
        return *d->repo;
    }
    /// @throw MissingRepositoryError No repository is configured.
    throw MissingRepositoryError("SavedSession::repository", "No repository is configured");
}

void SavedSession::setRepository(SavedSessionRepository *newRepository)
{
    d->repo = newRepository;
}

SavedSession::Status SavedSession::status() const
{
    d->updateStatusIfNeeded();
    return d->status;
}

String SavedSession::statusAsText() const
{
    static String const statusTexts[] = {
        "Loadable",
        "Incompatible",
        "Unused",
    };
    return statusTexts[int(status())];
}

static String metadataAsStyledText(SavedSession::Metadata const &metadata)
{
    return String(_E(b) "%1\n" _E(.)
                  _E(l) "IdentityKey: " _E(.)_E(i) "%2 "  _E(.)
                  _E(l) "Current map: " _E(.)_E(i) "%3\n" _E(.)
                  _E(l) "Version: "     _E(.)_E(i) "%4 "  _E(.)
                  _E(l) "Session id: "  _E(.)_E(i) "%5\n" _E(.)
                  _E(D) "Game rules:\n" _E(.) "  %6")
             .arg(metadata.gets("userDescription", ""))
             .arg(metadata.gets("gameIdentityKey", ""))
             .arg(metadata.gets("mapUri", ""))
             .arg(metadata.geti("version", 14))
             .arg(metadata.geti("sessionId", 0))
             .arg(metadata.gets("gameRules", ""));
}

String SavedSession::description() const
{
    return metadataAsStyledText(metadata()) + "\n" +
           String(_E(l) "Source file: " _E(.)_E(i) "\"%1\"\n" _E(.)
                  _E(D) "Status: " _E(.) "%2")
               .arg(d->repo? NativePath(d->repo->folder().path() / d->fileName).pretty() : "None")
               .arg(statusAsText());
}

String SavedSession::fileName() const
{
    return d->fileName;
}

void SavedSession::setFileName(String newName)
{
    if(!newName.isEmpty() && newName.fileNameExtension().isEmpty())
    {
        newName += ".save";
    }

    if(d->fileName != newName)
    {
        d->fileName         = newName;
        d->needUpdateStatus = true;
    }
}

bool SavedSession::hasFile() const
{
    if(!d->repo) return false;
    return d->repo->folder().has(fileName());
}

PackageFolder &SavedSession::locateFile()
{
    if(PackageFolder *pack = d->tryLocatePackage())
    {
        return *pack;
    }
    /// @throw MissingFileError Failed to locate the source file package.
    throw MissingFileError("SavedSession::locateFile", "Source file for " + d->fileName + " could not be located");
}

PackageFolder const &SavedSession::locateFile() const
{
    return const_cast<SavedSession *>(this)->locateFile();
}

bool SavedSession::recognizeFile()
{
    if(PackageFolder *pack = d->tryLocatePackage())
    {
        // Ensure we have up-to-date info about the package contents.
        pack->populate();

        // Attempt to read the session metadata.
        if(SessionMetadata *metadata = Instance::readMetadata(*pack))
        {
            replaceMetadata(metadata);
            return true;
        }
    }
    return 0; // Unrecognized
}

void SavedSession::updateFromFile()
{
    LOGDEV_VERBOSE("Updating SavedSession %p from the repository") << this;

    if(recognizeFile())
    {
        // Ensure we have a valid description.
        if(d->metadata->gets("userDescription", "").isEmpty())
        {
            d->metadata->set("userDescription", "UNNAMED");
            d->notifyMetadataChanged();
        }
    }
    else
    {
        // Unrecognized or the file could not be accessed (perhaps its a network path?).
        // Return the session to the "null/invalid" state.
        d->metadata->set("userDescription", "");
        d->metadata->set("sessionId", duint32(0));
        d->notifyMetadataChanged();
    }

    d->updateStatusIfNeeded();
}

void SavedSession::copyFile(SavedSession const &source)
{
    if(&source == this) return; // Sanity check.

    Writer(repository().folder().replaceFile(d->fileName)) << source.locateFile().archive();
    repository().folder().populate(Folder::PopulateOnlyThisFolder);

    // Perform recognition of the copied file and update the session status.
    updateFromFile();
}

void SavedSession::removeFile()
{
    if(hasFile())
    {
        d->repo->folder().removeFile(fileName());
        d->needUpdateStatus = true;
    }

    /// Force a status update. @todo necessary?
    updateFromFile();
}

bool SavedSession::hasMapState(String mapUriStr) const
{
    if(!mapUriStr.isEmpty())
    {
        if(PackageFolder const *pack = d->tryLocatePackage())
        {
            return pack->has(Path("maps") / mapUriStr);
        }
    }
    return false;
}

std::auto_ptr<MapStateReader> SavedSession::mapStateReader()
{
    if(recognizeFile())
    {
        std::auto_ptr<MapStateReader> p(repository().makeReader(*this));
        return p;
    }
    /// @throw UnrecognizedMapStateError The game state format was not recognized.
    throw UnrecognizedMapStateError("SavedSession::mapStateReader", "Unrecognized map state format");
}

SavedSession::Metadata const &SavedSession::metadata() const
{
    DENG2_ASSERT(!d->metadata.isNull());
    return *d->metadata;
}

void SavedSession::replaceMetadata(Metadata *newMetadata)
{
    DENG2_ASSERT(newMetadata != 0);
    d->metadata.reset(newMetadata);
    d->needUpdateStatus = true;
}

} // namespace game
} // namespace de
