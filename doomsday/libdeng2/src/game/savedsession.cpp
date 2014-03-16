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
    String repoPath; ///< Relative path to the game session file.
    QScopedPointer<Metadata> metadata;

    Instance(Public *i)
        : Base(i)
        , metadata        (new Metadata)
    {}

    Instance(Public *i, Instance const &other)
        : Base(i)
        , repoPath        (other.repoPath)
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
        return App::fileSystem().root().tryLocate<PackageFolder>(String("/savegame") / repoPath);
    }

    DENG2_PIMPL_AUDIENCE(MetadataChange)
};

DENG2_AUDIENCE_METHOD(SavedSession, MetadataChange)

SavedSession::SavedSession(String repoPath) : d(new Instance(this))
{
    DENG2_ASSERT(!repoPath.isEmpty());
    d->repoPath = repoPath;
    if(d->repoPath.fileNameExtension().isEmpty())
    {
        d->repoPath += ".save";
    }
}

SavedSession::SavedSession(SavedSession const &other) : d(new Instance(this, *other.d))
{}

SavedSession &SavedSession::operator = (SavedSession const &other)
{
    d.reset(new Instance(this, *other.d));
    return *this;
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
           String(_E(l) "Source file: " _E(.)_E(i) "\"%1\"")
               .arg(hasFile()? NativePath(String("/home/savegame") / d->repoPath).pretty() : "None");
}

String SavedSession::path() const
{
    return d->repoPath;
}

void SavedSession::setPath(String newPath)
{
    DENG2_ASSERT(!newPath.isEmpty());

    d->repoPath = newPath;
    if(d->repoPath.fileNameExtension().isEmpty())
    {
        d->repoPath += ".save";
    }
}

bool SavedSession::hasFile() const
{
    return App::fileSystem().root().has(String("/savegame") / d->repoPath);
}

PackageFolder &SavedSession::locateFile()
{
    if(PackageFolder *pack = d->tryLocatePackage())
    {
        return *pack;
    }
    /// @throw MissingFileError Failed to locate the source file package.
    throw MissingFileError("SavedSession::locateFile", "Source file for " + d->repoPath + " could not be located");
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
}

void SavedSession::copyFile(SavedSession const &source)
{
    if(&source == this) return; // Sanity check.

    File &destFile = App::fileSystem().root().replaceFile(String("/savegame") / d->repoPath);
    Writer(destFile) << source.locateFile().archive();
    destFile.parent()->populate(Folder::PopulateOnlyThisFolder);

    // Perform recognition of the copied file and update the session status.
    updateFromFile();
}

void SavedSession::removeFile()
{
    if(hasFile())
    {
        App::fileSystem().root().removeFile(String("/savegame") / d->repoPath);
    }

    /// Force a metadata update.
    updateFromFile();
}

bool SavedSession::hasMapState(String mapUriStr) const
{
    if(!mapUriStr.isEmpty())
    {
        if(PackageFolder const *pack = d->tryLocatePackage())
        {
            return pack->has(Path("maps") / mapUriStr + "State");
        }
    }
    return false;
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
    d->notifyMetadataChanged();
}

} // namespace game
} // namespace de
