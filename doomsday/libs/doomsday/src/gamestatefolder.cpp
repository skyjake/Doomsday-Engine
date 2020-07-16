/** @file gamestatefolder.cpp  Archived game state.
 *
 * @authors Copyright © 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/gamestatefolder.h"
#include "doomsday/abstractsession.h"
#include "doomsday/res/databundle.h"

#include <de/app.h>
#include <de/archivefolder.h>
#include <de/arrayvalue.h>
#include <de/info.h>
#include <de/logbuffer.h>
#include <de/nativepath.h>
#include <de/numbervalue.h>
#include <de/packageloader.h>
#include <de/regexp.h>
#include <de/textvalue.h>
#include <de/writer.h>
#include <de/ziparchive.h>

using namespace de;

static const String BLOCK_GROUP    = "group";
static const String BLOCK_GAMERULE = "gamerule";

/// @todo Refactor this to use ScriptedInfo. -jk

static Value *makeValueFromInfoValue(const de::Info::Element::Value &v)
{
    const String text = v;
    if (!text.compareWithoutCase("True"))
    {
        return new NumberValue(true, NumberValue::Boolean);
    }
    else if (!text.compareWithoutCase("False"))
    {
        return new NumberValue(false, NumberValue::Boolean);
    }
    else
    {
        return new TextValue(text);
    }
}

DE_PIMPL(GameStateFolder)
{
    Metadata metadata;  ///< Cached.
    bool needCacheMetadata;

    Impl(Public *i)
        : Base(i)
        , needCacheMetadata(true)
    {}

    bool readMetadata(Metadata &metadata)
    {
        try
        {
            Block raw;
            self().locate<const File>("Info") >> raw;

            metadata.parse(String::fromUtf8(raw));

            // So far so good.
            return true;
        }
        catch (const IByteArray::OffsetError &)
        {
            LOG_RES_WARNING("Archive in %s is truncated") << self().description();
        }
        catch (const IIStream::InputError &)
        {
            LOG_RES_WARNING("%s cannot be read") << self().description();
        }
        catch (const Archive::FormatError &)
        {
            LOG_RES_WARNING("Archive in %s is invalid") << self().description();
        }
        catch (const Folder::NotFoundError &)
        {
            LOG_RES_WARNING("%s does not appear to be a .save package") << self().description();
        }
        return false;
    }

    DE_PIMPL_AUDIENCE(MetadataChange)
};

DE_AUDIENCE_METHOD(GameStateFolder, MetadataChange)

GameStateFolder::GameStateFolder(File &sourceArchiveFile, const String &name)
    : ArchiveFolder(sourceArchiveFile, name)
    , d(new Impl(this))
{}

GameStateFolder::~GameStateFolder()
{
    DE_NOTIFY(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();
    deindex();
    //Session::savedIndex().remove(path());
}

/*void GameStateFolder::populate(PopulationBehaviors behavior)
{
    ArchiveFolder::populate(behavior);
    //Session::savedIndex().add(*this);
}*/

void GameStateFolder::readMetadata()
{
    LOGDEV_VERBOSE("Updating GameStateFolder metadata %p") << this;

    // Determine if a .save package exists in the repository and if so, read the metadata.
    Metadata newMetadata;
    if (!d->readMetadata(newMetadata))
    {
        // Unrecognized or the file could not be accessed (perhaps its a network path?).
        // Return the session to the "null/invalid" state.
        newMetadata.set("userDescription", "");
        newMetadata.set("sessionId", duint32(0));
    }

    cacheMetadata(newMetadata);
}

const GameStateFolder::Metadata &GameStateFolder::metadata() const
{
    if (d->needCacheMetadata)
    {
        const_cast<GameStateFolder *>(this)->readMetadata();
    }
    return d->metadata;
}

void GameStateFolder::cacheMetadata(const Metadata &copied)
{
    d->metadata          = copied;
    d->needCacheMetadata = false;
    DE_NOTIFY(MetadataChange, i)
    {
        i->gameStateFolderMetadataChanged(*this);
    }
}

String GameStateFolder::stateFilePath(const String &path) //static
{
    if (!path.fileName().isEmpty())
    {
        return path + "State";
    }
    return "";
}

bool GameStateFolder::isPackageAffectingGameplay(const String &packageId) // static
{
    /**
     * @todo The rules here could be more sophisticated when it comes to checking what
     * exactly do the data bundles contain. Also, packages should be checked for any
     * gameplay-affecting assets. (2016-07-06: Currently there are none.)
     */
    if (const auto *bundle = DataBundle::bundleForPackage(packageId))
    {
        // Collections can be configured, so we need to list the actual files in use
        // rather than just the collection itself.
        return bundle->format() != DataBundle::Collection;
    }

    if (const File *selected = PackageLoader::get().select(packageId))
    {
        const auto &meta = Package::metadata(*selected);
        if (meta.has("dataFiles") && meta.geta("dataFiles").size() > 0)
        {
            // Data files are assumed to affect gameplay.
            return true;
        }
    }
    return false;
}

File *GameStateFolder::Interpreter::interpretFile(File *sourceData) const
{
    try
    {
        if (ZipArchive::recognize(*sourceData))
        {
            // It is a ZIP archive: we will represent it as a folder.
            if (sourceData->extension() == ".save")
            {
                /// @todo fixme: Don't assume this is a save package.
                LOG_RES_XVERBOSE("Interpreted %s as a GameStateFolder", sourceData->description());
                std::unique_ptr<ArchiveFolder> package;
                package.reset(new GameStateFolder(*sourceData, sourceData->name()));

                // Archive opened successfully, give ownership of the source to the folder.
                package->setSource(sourceData);
                return package.release();
            }
        }
    }
    catch (const Error &er)
    {
        // Even though it was recognized as an archive, the file
        // contents may still prove to be corrupted.
        LOG_RES_WARNING("Failed to read archive in %s: %s")
                << sourceData->description()
                << er.asText();
    }
    return nullptr;
}

//---------------------------------------------------------------------------------------

DE_PIMPL_NOREF(GameStateFolder::MapStateReader)
{
    const GameStateFolder *session; ///< Saved session being read. Not owned.

    Impl(const GameStateFolder &session) : session(&session)
    {}
};

GameStateFolder::MapStateReader::MapStateReader(const GameStateFolder &session)
    : d(new Impl(session))
{}

GameStateFolder::MapStateReader::~MapStateReader()
{}

const GameStateFolder::Metadata &GameStateFolder::MapStateReader::metadata() const
{
    return d->session->metadata();
}

const Folder &GameStateFolder::MapStateReader::folder() const
{
    return *d->session;
}

//---------------------------------------------------------------------------------------

void GameStateFolder::Metadata::parse(const String &source)
{
    try
    {
        clear();

        Info info;
        info.setAllowDuplicateBlocksOfType({BLOCK_GROUP, BLOCK_GAMERULE});
        info.parse(source);

        // Rebuild the game rules subrecord.
        Record &rules = addSubrecord("gameRules");
        for (const auto *elem : info.root().contentsInOrder())
        {
            if (const Info::KeyElement *key = maybeAs<Info::KeyElement>(elem))
            {
                std::unique_ptr<Value> v(makeValueFromInfoValue(key->value()));
                add(key->name()) = v.release();
                continue;
            }
            if (const Info::ListElement *list = maybeAs<Info::ListElement>(elem))
            {
                std::unique_ptr<ArrayValue> arr(new ArrayValue);
                for (const Info::Element::Value &v : list->values())
                {
                    *arr << makeValueFromInfoValue(v);
                }
                addArray(list->name(), arr.release());
                continue;
            }
            if (const Info::BlockElement *block = maybeAs<Info::BlockElement>(elem))
            {
                // Perhaps a ruleset group?
                if (block->blockType() == BLOCK_GROUP)
                {
                    for (const Info::Element *grpElem : block->contentsInOrder())
                    {
                        if (!grpElem->isBlock()) continue;

                        // Perhaps a gamerule?
                        const Info::BlockElement &ruleBlock = grpElem->as<Info::BlockElement>();
                        if (ruleBlock.blockType() == BLOCK_GAMERULE)
                        {
                            std::unique_ptr<Value> v(makeValueFromInfoValue(ruleBlock.keyValue("value")));
                            rules.add(ruleBlock.name()) = v.release();
                        }
                    }
                }
                continue;
            }
        }

        // Ensure the map URI has the "Maps" scheme set.
        if (!gets("mapUri").beginsWith("Maps:", CaseInsensitive))
        {
            set("mapUri", String("Maps:") + gets("mapUri"));
        }

        // Ensure the episode is known. Earlier versions of the savegame format did not save
        // this info explicitly. The assumption was that the episode was inferred by / encoded
        // in the map URI. If the episode is not present in the metadata then we'll assume it
        // is encoded in the map URI and extract it.
        if (!has("episode"))
        {
            const String mapUriPath = gets("mapUri").substr(BytePos(5));
            if (mapUriPath.beginsWith("MAP", CaseInsensitive))
            {
                set("episode", "1");
            }
            else if (mapUriPath.first().lower() == 'e' && Char(mapUriPath.at(BytePos(2))).lower() == 'm')
            {
                set("episode", mapUriPath.substr(BytePos(1), 1));
            }
            else
            {
                // Hmm, very odd...
                throw Error("GameStateFolder::metadata::parse",
                            "Failed to extract episode id from map URI \"" + gets("mapUri") + "\"");
            }
        }

        if (info.root().contains("packages"))
        {
            const Info::ListElement &list = info.root().find("packages")->as<Info::ListElement>();
            auto *pkgs = new ArrayValue;
            for (const auto &value : list.values())
            {
                *pkgs << new TextValue(value.text);
            }
            set("packages", pkgs);
        }
        else
        {
            set("packages", new ArrayValue);
        }

        // Ensure we have a valid description.
        if (gets("userDescription").isEmpty())
        {
            set("userDescription", "UNNAMED");
        }

        //qDebug() << "Parsed save metadata:\n" << asText();
    }
    catch (const Error &er)
    {
        LOG_WARNING(er.asText());
    }
}

String GameStateFolder::Metadata::asStyledText() const
{
    String currentMapText = Stringf(
                _E(Ta)_E(l) "  Episode: " _E(.)_E(Tb) "%s\n"
                _E(Ta)_E(l) "  Uri: "     _E(.)_E(Tb) "%s",
            gets("episode").c_str(),
            gets("mapUri").c_str());
    // Is the time in the current map known?
    if (has("mapTime"))
    {
        int time = geti("mapTime") / 35 /*TICRATE*/;
        const int hours   = time / 3600; time -= hours * 3600;
        const int minutes = time / 60;   time -= minutes * 60;
        const int seconds = time;
        currentMapText += Stringf(
            "\n" _E(Ta) _E(l) "  Time: " _E(.) _E(Tb) "%02d:%02d:%02d", hours, minutes, seconds);
    }

    StringList rules = gets("gameRules", "None").split("\n");
    for (auto &r : rules)
    {
        static const RegExp reKeyValue(R"(\s*(.*)\s*:\s*([^ ].*)\s*)");
        r.replace(reKeyValue, _E(l) "\\1: " _E(.) "\\2");
    }
    String gameRulesText = String::join(rules, "\n - ");

    const auto &pkgs = geta("packages");
    StringList pkgIds;
    for (const auto *val : pkgs.elements())
    {
        pkgIds << Package::splitToHumanReadable(val->asText());
    }

    return Stringf(_E(1) "%s\n" _E(.)
                  _E(Ta)_E(l) "  Game: "  _E(.)_E(Tb) "%s\n"
                  _E(Ta)_E(l) "  Session ID: "   _E(.)_E(Tb)_E(m) "0x%x" _E(.) "\n"
                  _E(T`)_E(D) "Current map:\n" _E(.) "%s\n"
                  _E(T`)_E(D) "Game rules:\n"  _E(.) " - %s\n"
                  _E(T`)_E(D) "Packages:\n" _E(.) " - %s",
             gets("userDescription", "").c_str(),
             gets("gameIdentityKey", "").c_str(),
             getui("sessionId"),
             currentMapText.c_str(),
             gameRulesText.c_str(),
             String::join(pkgIds, "\n - ").c_str());
}

/*
 * See the Doomsday Wiki for an example of the syntax:
 * http://dengine.net/dew/index.php?title=Info
 */
String GameStateFolder::Metadata::asInfo() const
{
    /// @todo Use a more generic Record => Info conversion logic.

    std::ostringstream os;

    if (has("gameIdentityKey")) os <<   "gameIdentityKey: " << gets("gameIdentityKey");
    if (has("packages"))
    {
        os << "\npackages " << geta("packages").asInfo();
    }
    if (has("episode"))         os << "\nepisode: "         << gets("episode");
    if (has("mapTime"))         os << "\nmapTime: "         << geti("mapTime");
    if (has("mapUri"))          os << "\nmapUri: "          << gets("mapUri");
    if (has("players"))
    {
        os << "\nplayers <";
        const ArrayValue &playersArray = geta("players");
        DE_FOR_EACH_CONST(ArrayValue::Elements, i, playersArray.elements())
        {
            const Value *value = *i;
            if (i != playersArray.elements().begin()) os << ", ";
            os << (value->as<NumberValue>().isTrue()? "True" : "False");
        }
        os << ">";
    }
    if (has("visitedMaps"))
    {
        os << "\nvisitedMaps " << geta("visitedMaps").asInfo();
    }
    if (has("sessionId"))       os << "\nsessionId: "       << geti("sessionId");
    if (has("userDescription")) os << "\nuserDescription: " << gets("userDescription");

    if (hasSubrecord("gameRules"))
    {
        os << "\n" << BLOCK_GROUP << " ruleset {";

        const Record &rules = subrecord("gameRules");
        DE_FOR_EACH_CONST(Record::Members, i, rules.members())
        {
            const Value &value = i->second->value();
            String valueAsText = value.asText();
            if (is<Value::Text>(value))
            {
                valueAsText = "\"" + valueAsText.replace("\"", "''") + "\"";
            }
            os << "\n    " << BLOCK_GAMERULE << " \"" << i->first << "\""
               << " { value = " << valueAsText << " }";
        }

        os << "\n}";
    }

    return os.str();
}
