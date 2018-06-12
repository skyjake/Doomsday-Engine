/** @file profiles.cpp  Abstract set of persistent profiles.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Profiles"
#include "de/App"
#include "de/FileSystem"
#include "de/Folder"
#include "de/String"

namespace de {

DE_PIMPL(Profiles)
, DE_OBSERVES(Deletable, Deletion)
{
    typedef Map<String, AbstractProfile *, String::InsensitiveLessThan> Profiles;
    Profiles profiles;
    String persistentName;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        clear();
    }

    void add(AbstractProfile *profile)
    {
        String const key = profile->name();
        if (profiles.contains(key))
        {
            delete profiles[key];
        }
        profiles.insert(key, profile);
        profile->setOwner(thisPublic);
        profile->audienceForDeletion += this;

        DE_FOR_PUBLIC_AUDIENCE2(Addition, i)
        {
            i->profileAdded(*profile);
        }
    }

    void remove(AbstractProfile &profile)
    {
        profile.audienceForDeletion -= this;
        profile.setOwner(nullptr);
        profiles.remove(profile.name());

        DE_FOR_PUBLIC_AUDIENCE2(Removal, i)
        {
            i->profileRemoved(profile);
        }
    }

    void changeLookupKey(AbstractProfile const &profile, String const &newName)
    {
        profiles.remove(profile.name());
        profiles.insert(newName, const_cast<AbstractProfile *>(&profile));
    }

    void objectWasDeleted(Deletable *obj)
    {
        // At this point the AbstractProfile itself is already deleted.
        for (auto i = profiles.begin(); i != profiles.end(); ++i)
        {
            if (i->second == obj)
            {
                profiles.erase(i);
                break;
            }
        }
    }

    void clear()
    {
        for (auto &prof : profiles)
        {
            prof.second->audienceForDeletion -= this;
            prof.second->setOwner(nullptr);
            delete prof.second;
        }
        profiles.clear();
    }

    /**
     * For persistent profiles, determines the file name of the Info file
     * where all the profile contents are written to and read from.
     */
    String fileName() const
    {
        if (persistentName.isEmpty()) return "";
        return String::format("/home/configs/%s.dei", persistentName.c_str());
    }

    void loadProfilesFromInfo(File const &file, bool markReadOnly)
    {
        try
        {
            LOG_RES_VERBOSE("Reading profiles from %s") << file.description();

            Block raw;
            file >> raw;

            de::Info info;
            info.parse(String::fromUtf8(raw));

            for (const auto *elem : info.root().contentsInOrder())
            {
                if (!elem->isBlock()) continue;

                // There may be multiple profiles in the file.
                de::Info::BlockElement const &profBlock = elem->as<de::Info::BlockElement>();
                if (profBlock.blockType() == "group" &&
                    profBlock.name()      == "profile")
                {
                    String profileName = profBlock.keyValue("name").text;
                    if (profileName.isEmpty()) continue; // Name is required.

                    LOG_VERBOSE("Reading profile '%s'") << profileName;

                    auto *prof = self().profileFromInfoBlock(profBlock);
                    prof->setName(profileName);
                    prof->setReadOnly(markReadOnly);
                    add(prof);
                }
            }
        }
        catch (Error const &er)
        {
            LOG_RES_WARNING("Failed to load profiles from %s:\n%s")
                    << file.description() << er.asText();
        }
    }
    DE_PIMPL_AUDIENCE(Addition)
    DE_PIMPL_AUDIENCE(Removal)
};

DE_AUDIENCE_METHOD(Profiles, Addition)
DE_AUDIENCE_METHOD(Profiles, Removal)

Profiles::Profiles()
    : d(new Impl(this))
{}

Profiles::~Profiles()
{}

StringList Profiles::profiles() const
{
    StringList names;
    for (auto &p : d->profiles) names << p.second->name();
    return names;
}

int Profiles::count() const
{
    return d->profiles.size();
}

Profiles::AbstractProfile *Profiles::tryFind(String const &name) const
{
    auto found = d->profiles.find(name);
    if (found != d->profiles.end())
    {
        return found->second;
    }
    return nullptr;
}

Profiles::AbstractProfile &Profiles::find(String const &name) const
{
    if (auto *p = tryFind(name))
    {
        return *p;
    }
    throw NotFoundError("Profiles::find", "Profile '" + name + "' not found");
}

void Profiles::setPersistentName(String const &name)
{
    d->persistentName = name;
}

String Profiles::persistentName() const
{
    return d->persistentName;
}

bool Profiles::isPersistent() const
{
    return !d->persistentName.isEmpty();
}

LoopResult Profiles::forAll(const std::function<LoopResult (AbstractProfile &)>& func) const
{
    for (const auto &prof : d->profiles)
    {
        if (auto result = func(*prof.second))
        {
            return result;
        }
    }
    return LoopContinue;
}

void Profiles::clear()
{
    d->clear();
}

void Profiles::add(AbstractProfile *profile)
{
    d->add(profile);
}

void Profiles::remove(AbstractProfile &profile)
{
    DE_ASSERT(&profile.owner() == this);

    d->remove(profile);
}

bool Profiles::rename(AbstractProfile const &profile, String const &newName)
{
    if (newName.isEmpty() || tryFind(newName)) return false;
    d->changeLookupKey(profile, newName);
    return true;
}

void Profiles::serialize() const
{
    if (!isPersistent()) return;

    LOG_AS("Profiles");
    LOGDEV_VERBOSE("Serializing %s profiles") << d->persistentName;

    // We will write one Info file with all the profiles.
    std::ostringstream os;
    os << "# Autogenerated Info file based on " << d->persistentName.c_str()
       << " profiles\n";

    // Write /home/configs/(persistentName).dei with all non-readonly profiles.
    int count = 0;
    for (auto &iter : d->profiles)
    {
        const auto *prof = iter.second;
        if (prof->isReadOnly()) continue;

        os << "\nprofile {\n"
              "    name: " << prof->name().c_str() << "\n";
        for (const auto &line : prof->toInfoSource().splitRef('\n'))
        {
            os << "    " << line.toStdString() << "\n";
        }
        os << "}\n";
        ++count;
    }

    // Create the pack and update the file system.
    File &outFile = App::rootFolder().replaceFile(d->fileName());
    outFile << Block(os.str());
    outFile.flush(); // we're done

    LOG_VERBOSE("Wrote \"%s\" with %i profile%s")
            << d->fileName() << count << (count != 1? "s" : "");
}

void Profiles::deserialize()
{
    if (!isPersistent()) return;

    LOG_AS("Profiles");
    LOGDEV_VERBOSE("Deserializing %s profiles") << d->persistentName;

    clear();

    // Read all fixed profiles from */profiles/(persistentName)/
    FS::FoundFiles folders;
    App::fileSystem().findAll("profiles" / d->persistentName, folders);
    DE_FOR_EACH(FS::FoundFiles, i, folders)
    {
        if (auto const *folder = maybeAs<Folder>(*i))
        {
            // Let's see if it contains any .dei files.
            folder->forContents([this] (String name, File &file)
            {
                if (name.fileNameExtension() == ".dei")
                {
                    // Load this profile.
                    d->loadProfilesFromInfo(file, true /* read-only */);
                }
                return LoopContinue;
            });
        }
    }

    // Read /home/configs/(persistentName).dei
    if (File const *file = App::rootFolder().tryLocate<File const>(d->fileName()))
    {
        d->loadProfilesFromInfo(*file, false /* modifiable */);
    }
}

// Profiles::AbstractProfile --------------------------------------------------

DE_PIMPL(Profiles::AbstractProfile)
{
    Profiles *owner = nullptr;
    String name;
    bool readOnly = false;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        if (owner)
        {
            owner->remove(self());
        }
    }

    DE_PIMPL_AUDIENCE(Change)
};

DE_AUDIENCE_METHOD(Profiles::AbstractProfile, Change)

Profiles::AbstractProfile::AbstractProfile()
    : d(new Impl(this))
{}

Profiles::AbstractProfile::AbstractProfile(AbstractProfile const &profile)
    : d(new Impl(this))
{
    d->name     = profile.name();
    d->readOnly = profile.isReadOnly();
}

Profiles::AbstractProfile::~AbstractProfile()
{}

Profiles::AbstractProfile &Profiles::AbstractProfile::operator = (AbstractProfile const &other)
{
    d->name     = other.d->name;
    d->readOnly = other.d->readOnly;
    // owner is not copied
    return *this;
}

void Profiles::AbstractProfile::setOwner(Profiles *owner)
{
    DE_ASSERT(d->owner != owner);
    d->owner = owner;
}

Profiles &Profiles::AbstractProfile::owner()
{
    DE_ASSERT(d->owner);
    return *d->owner;
}

Profiles const &Profiles::AbstractProfile::owner() const
{
    DE_ASSERT(d->owner);
    return *d->owner;
}

String Profiles::AbstractProfile::name() const
{
    return d->name;
}

bool Profiles::AbstractProfile::setName(String const &newName)
{
    if (newName.isEmpty()) return false;

    Profiles *owner = d->owner;
    if (!owner || owner->rename(*this, newName))
    {
        d->name = newName;
        notifyChange();
    }
    return true;
}

bool Profiles::AbstractProfile::isReadOnly() const
{
    return d->readOnly;
}

void Profiles::AbstractProfile::setReadOnly(bool readOnly)
{
    d->readOnly = readOnly;
}

void Profiles::AbstractProfile::notifyChange()
{
    DE_FOR_AUDIENCE2(Change, i)
    {
        i->profileChanged(*this);
    }
}

} // namespace de
