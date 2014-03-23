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
        if(Info::ListElement const *list = info.findByPath("players")->maybeAs<Info::ListElement>())
        {
            QScopedPointer<ArrayValue> arr(new ArrayValue);
            foreach(Info::Element::Value const &v, list->values())
            {
                bool value = !String(v).compareWithoutCase("True");
                *arr << new NumberValue(value, NumberValue::Boolean);
            }
            set("players", arr.take());
        }
        if(info.findValueForKey("sessionid", value))
        {
            set("sessionId", value.toInt());
        }
        if(info.findValueForKey("userdescription", value))
        {
            // Ensure we have a valid description.
            set("userDescription", value.isEmpty()? "UNNAMED" : value);
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
    if(has("players"))
    {
        ArrayValue const &array = geta("players");
        os << "\nplayers <";
        DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, array.elements())
        {
            Value const *value = *i;
            if(i != array.elements().begin()) os << ", ";
            os << (value->as<NumberValue>().isTrue()? "True" : "False");
        }
        os << ">";
    }
    if(has("sessionId"))       os << "\nsessionId: "       << String::number(geti("sessionId"));
    if(has("userDescription")) os << "\nuserDescription: " << gets("userDescription");

    if(hasSubrecord("gameRules"))
    {
        os << "\n" << BLOCK_GROUP << " ruleset {";

        Record const &rules = subrecord("gameRules");
        DENG2_FOR_EACH_CONST(Record::Members, i, rules.members())
        {
            Value const &value = i.value()->value();
            String valueAsText = value.asText();
            if(value.is<Value::Text>())
            {
                valueAsText = "\"" + valueAsText.replace("\"", "''") + "\"";
            }
            os << "\n    " << BLOCK_GAMERULE << " \"" << i.key() << "\""
               << " { value= " << valueAsText << " }";
        }

        os << "\n}";
    }

    return text;
}

/*static inline bool packageExists(String const &repoPath)
{
    return App::homeFolder().has(String("savegames") / repoPath + ".save");
}

static inline PackageFolder *tryLocatePackage(String const &repoPath)
{
    return App::homeFolder().tryLocate<PackageFolder>(String("savegames") / repoPath + ".save");
}*/

DENG2_PIMPL(SavedSession)
{
    Metadata metadata;  ///< Cached metadata.

    Instance(Public *i) : Base(i) {}

    /*static*/ bool readMetadata(Metadata &metadata)//, SavedSession *session)
    {
        try
        {
            // Ensure we have up-to-date info about the package contents.
            self.populate();

            File const &file = self.locate<File const>("Info");
            Block raw;
            file >> raw;

            metadata.parse(String::fromUtf8(raw));

            // So far so good.
            return true;
        }
        catch(IByteArray::OffsetError const &)
        {
            LOG_RES_WARNING("Archive in %s is truncated") << self.description();
        }
        catch(IIStream::InputError const &)
        {
            LOG_RES_WARNING("%s cannot be read") << self.description();
        }
        catch(Archive::FormatError const &)
        {
            LOG_RES_WARNING("Archive in %s is invalid") << self.description();
        }
        catch(Folder::NotFoundError const &)
        {
            LOG_RES_WARNING("%s does not appear to be a .save package") << self.description();
        }

        return 0;
    }

    DENG2_PIMPL_AUDIENCE(MetadataChange)
};

DENG2_AUDIENCE_METHOD(SavedSession, MetadataChange)

SavedSession::SavedSession(File &sourceArchiveFile, String const &name)
    : PackageFolder(sourceArchiveFile, name)
    , d(new Instance(this))
{}

SavedSession::~SavedSession()
{}

static String metadataAsStyledText(SavedSession::Metadata const &metadata)
{
    // Try to format the game rules so they look a little prettier.
    QStringList rules = metadata.gets("gameRules", "").split("\n", QString::SkipEmptyParts);
    rules.replaceInStrings(QRegExp("^(.*)= (.*)$"), _E(l) "\\1: " _E(.)_E(i) "\\2" _E(.));
    String gameRulesText;
    for(int i = 0; i < rules.size(); ++i)
    {
        if(i) gameRulesText += "\n";
        gameRulesText += " - " + rules.at(i).trimmed();
    }

    return String(_E(b) "%1\n" _E(.)
                  _E(l) "IdentityKey: " _E(.)_E(i) "%2 "  _E(.)
                  _E(l) "Current map: " _E(.)_E(i) "%3\n" _E(.)
                  _E(l) "Session id: "  _E(.)_E(i) "%4\n" _E(.)
                  _E(D) "Game rules:\n" _E(.) "%5")
             .arg(metadata.gets("userDescription", ""))
             .arg(metadata.gets("gameIdentityKey", ""))
             .arg(metadata.gets("mapUri", ""))
             .arg(metadata.geti("sessionId", 0))
             .arg(gameRulesText);
}

String SavedSession::styledDescription() const
{
    return metadataAsStyledText(metadata()) + "\n" +
           String(_E(D) "Resource: " _E(.)_E(i) "\"%1\"")
               .arg(NativePath(String("/home/savegames") / repoPath() + ".save").pretty());
}

/*PackageFolder &SavedSession::locateFile()
{
    if(PackageFolder *pack = tryLocatePackage(d->repoPath))
    {
        return *pack;
    }
    /// @throw MissingFileError Failed to locate the source file package.
    throw MissingFileError("SavedSession::locateFile", "Source file for " + d->repoPath + " could not be located");
}*/

/*PackageFolder const &SavedSession::locateFile() const
{
    return const_cast<SavedSession *>(this)->locateFile();
}*/

void SavedSession::readMetadata()
{
    LOGDEV_VERBOSE("Updating SavedSession metadata %p") << this;

    // Determine if a .save package exists in the repository and if so, read the metadata.
    Metadata newMetadata;
    if(!d->readMetadata(newMetadata))//tryLocatePackage(d->repoPath)))
    {
        // Unrecognized or the file could not be accessed (perhaps its a network path?).
        // Return the session to the "null/invalid" state.
        newMetadata.set("userDescription", "");
        newMetadata.set("sessionId", duint32(0));
    }

    cacheMetadata(newMetadata);
}

/*void SavedSession::copyFile(SavedSession const &source)
{
    if(&source == this) return; // Sanity check.

    File &destFile = App::homeFolder().replaceFile(String("savegames") / d->repoPath + ".save");
    Writer(destFile) << source.locateFile().archive();
    destFile.setMode(File::ReadOnly);
    destFile.parent()->populate(Folder::PopulateOnlyThisFolder);

    // Perform recognition of the copied file and update the session status.
    readMetadata();
}*/

/*void SavedSession::removeFile()
{
    if(packageExists(d->repoPath))
    {
        App::homeFolder().removeFile(String("savegames") / d->repoPath + ".save");
    }

    /// Force a metadata update.
    readMetadata();
}*/

bool SavedSession::hasMapState(String mapUriStr) const
{
    if(!mapUriStr.isEmpty())
    {
        //if(PackageFolder const *pack = tryLocatePackage(d->repoPath))
        {
            return has(Path("maps") / mapUriStr + "State");
        }
    }
    return false;
}

SavedSession::Metadata const &SavedSession::metadata() const
{
    return d->metadata;
}

void SavedSession::cacheMetadata(Metadata const &copied)
{
    d->metadata = copied; // Make a copy.
    DENG2_FOR_AUDIENCE2(MetadataChange, i)
    {
        i->savedSessionMetadataChanged(*this);
    }
}

} // namespace game
} // namespace de
