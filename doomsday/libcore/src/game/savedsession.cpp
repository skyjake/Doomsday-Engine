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

#include "de/game/savedsession.h"

#include "de/App"
#include "de/ArrayValue"
#include "de/game/Session"
#include "de/Info"
#include "de/Log"
#include "de/NumberValue"
#include "de/NativePath"
#include "de/ArchiveFolder"
#include "de/Writer"

namespace de {
namespace game {

static String const BLOCK_GROUP    = "group";
static String const BLOCK_GAMERULE = "gamerule";

static Value *makeValueFromInfoValue(Info::Element::Value const &v)
{
    String const text = v;
    if(!text.compareWithoutCase("True"))
    {
        return new NumberValue(true, NumberValue::Boolean);
    }
    else if(!text.compareWithoutCase("False"))
    {
        return new NumberValue(false, NumberValue::Boolean);
    }
    else
    {
        return new TextValue(text);
    }
}

void SavedSession::Metadata::parse(String const &source)
{
    clear();

    try
    {
        Info info;
        info.setAllowDuplicateBlocksOfType(QStringList() << BLOCK_GROUP << BLOCK_GAMERULE);
        info.parse(source);

        // Rebuild the game rules subrecord.
        Record &rules = addRecord("gameRules");
        foreach(Info::Element const *elem, info.root().contentsInOrder())
        {
            if(Info::KeyElement const *key = elem->maybeAs<Info::KeyElement>())
            {
                QScopedPointer<Value> v(makeValueFromInfoValue(key->value()));
                add(key->name()) = v.take();
                continue;
            }
            if(Info::ListElement const *list = elem->maybeAs<Info::ListElement>())
            {
                QScopedPointer<ArrayValue> arr(new ArrayValue);
                foreach(Info::Element::Value const &v, list->values())
                {
                    *arr << makeValueFromInfoValue(v);
                }
                addArray(list->name(), arr.take());
                continue;
            }
            if(Info::BlockElement const *block = elem->maybeAs<Info::BlockElement>())
            {
                // Perhaps a ruleset group?
                if(block->blockType() == BLOCK_GROUP)
                {
                    foreach(Info::Element const *grpElem, block->contentsInOrder())
                    {
                        if(!grpElem->isBlock()) continue;

                        // Perhaps a gamerule?
                        Info::BlockElement const &ruleBlock = grpElem->as<Info::BlockElement>();
                        if(ruleBlock.blockType() == BLOCK_GAMERULE)
                        {
                            QScopedPointer<Value> v(makeValueFromInfoValue(ruleBlock.keyValue("value")));
                            rules.add(ruleBlock.name()) = v.take();
                        }
                    }
                }
                continue;
            }
        }

        // Ensure the map URI has the "Maps" scheme set.
        if(!gets("mapUri").beginsWith("Maps:", Qt::CaseInsensitive))
        {
            set("mapUri", String("Maps:") + gets("mapUri"));
        }

        // Ensure the episode is known. Earlier versions of the savegame format did not save
        // this info explicitly. The assumption was that the episode was inferred by / encoded
        // in the map URI. If the episode is not present in the metadata then we'll assume it
        // is encoded in the map URI and extract it.
        if(!has("episode"))
        {
            String const mapUriPath = gets("mapUri").substr(5);
            if(mapUriPath.beginsWith("MAP", Qt::CaseInsensitive))
            {
                set("episode", "1");
            }
            else if(mapUriPath.at(0).toLower() == 'e' && mapUriPath.at(2).toLower() == 'm')
            {
                set("episode", mapUriPath.substr(1, 1));
            }
            else
            {
                // Hmm, very odd...
                throw Error("SavedSession::metadata::parse", "Failed to extract episode id from map URI \"" + gets("mapUri") + "\"");
            }
        }

        // Ensure we have a valid description.
        if(gets("userDescription").isEmpty())
        {
            set("userDescription", "UNNAMED");
        }
    }
    catch(Error const &er)
    {
        LOG_WARNING(er.asText());
    }
}

String SavedSession::Metadata::asStyledText() const
{
    String currentMapText = String(_E(l)" - Uri: " _E(.)_E(i) "%1" _E(.)).arg(gets("mapUri"));
    // Is the time in the current map known?
    if(has("mapTime"))
    {
        int time = geti("mapTime") / 35 /*TICRATE*/;
        int const hours   = time / 3600; time -= hours * 3600;
        int const minutes = time / 60;   time -= minutes * 60;
        int const seconds = time;
        currentMapText += String("\n" _E(l) " - Time: " _E(.)_E(i) "%1:%2:%3" _E(.))
                             .arg(hours,   2, 10, QChar('0'))
                             .arg(minutes, 2, 10, QChar('0'))
                             .arg(seconds, 2, 10, QChar('0'));
    }

    // Try to format the game rules so they look a little prettier.
    String gameRulesText;
    QStringList rules = gets("gameRules", "None").split("\n", QString::SkipEmptyParts);
    rules.replaceInStrings(QRegExp("^(.*)= (.*)$"), _E(l) "\\1: " _E(.)_E(i) "\\2" _E(.));
    for(int i = 0; i < rules.size(); ++i)
    {
        if(i) gameRulesText += "\n";
        gameRulesText += " - " + rules.at(i).trimmed();
    }

    return String(_E(b) "%1\n" _E(.)
                  _E(l) "IdentityKey: "  _E(.)_E(i) "%2 "  _E(.)
                  _E(l) "Session id: "   _E(.)_E(i) "%3\n" _E(.)
                  _E(l) "Episode: "      _E(.)_E(i) "%4\n" _E(.)
                  _E(D) "Current map:\n" _E(.) "%5\n"
                  _E(D) "Game rules:\n"  _E(.) "%6")
             .arg(gets("userDescription", ""))
             .arg(gets("gameIdentityKey", ""))
             .arg(geti("sessionId", 0))
             .arg(gets("episode"))
             .arg(currentMapText)
             .arg(gameRulesText);
}

/**
 * See the Doomsday Wiki for an example of the syntax:
 * http://dengine.net/dew/index.php?title=Info
 *
 * @todo Use a more generic Record => Info conversion logic.
 */
String SavedSession::Metadata::asTextWithInfoSyntax() const
{
    String text;
    QTextStream os(&text);
    os.setCodec("UTF-8");

    if(has("gameIdentityKey")) os <<   "gameIdentityKey: " << gets("gameIdentityKey");
    if(has("episode"))         os << "\nepisode: "         << gets("episode");
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

DENG2_PIMPL_NOREF(SavedSession::MapStateReader)
{
    SavedSession const *session; ///< Saved session being read. Not Owned.
    Instance(SavedSession const &session) : session(&session) {}
};

SavedSession::MapStateReader::MapStateReader(SavedSession const &session)
    : d(new Instance(session))
{}

SavedSession::MapStateReader::~MapStateReader()
{}

SavedSession::Metadata const &SavedSession::MapStateReader::metadata() const
{
    return d->session->metadata();
}

Folder const &SavedSession::MapStateReader::folder() const
{
    return *d->session;
}

DENG2_PIMPL(SavedSession)
{
    Metadata metadata;  ///< Cached.
    bool needCacheMetadata;

    Instance(Public *i)
        : Base(i)
        , needCacheMetadata(true)
    {}

    bool readMetadata(Metadata &metadata)
    {
        try
        {
            Block raw;
            self.locate<File const>("Info") >> raw;

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
    : ArchiveFolder(sourceArchiveFile, name)
    , d(new Instance(this))
{}

SavedSession::~SavedSession()
{
    DENG2_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    deindex();
    Session::savedIndex().remove(path());
}

void SavedSession::populate(PopulationBehavior behavior)
{
    ArchiveFolder::populate(behavior);
    Session::savedIndex().add(*this);
}

void SavedSession::readMetadata()
{
    LOGDEV_VERBOSE("Updating SavedSession metadata %p") << this;

    // Determine if a .save package exists in the repository and if so, read the metadata.
    Metadata newMetadata;
    if(!d->readMetadata(newMetadata))
    {
        // Unrecognized or the file could not be accessed (perhaps its a network path?).
        // Return the session to the "null/invalid" state.
        newMetadata.set("userDescription", "");
        newMetadata.set("sessionId", duint32(0));
    }

    cacheMetadata(newMetadata);
}

SavedSession::Metadata const &SavedSession::metadata() const
{
    if(d->needCacheMetadata)
    {
        const_cast<SavedSession *>(this)->readMetadata();
    }
    return d->metadata;
}

void SavedSession::cacheMetadata(Metadata const &copied)
{
    d->metadata          = copied;
    d->needCacheMetadata = false;
    DENG2_FOR_AUDIENCE2(MetadataChange, i)
    {
        i->savedSessionMetadataChanged(*this);
    }
}

String SavedSession::stateFilePath(String const &path) //static
{
    if(!path.fileName().isEmpty())
    {
        return path + "State";
    }
    return "";
}

} // namespace game
} // namespace de
