/** @file profiles.cpp  Abstract set of persistent profiles.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

namespace de {

DENG2_PIMPL(Profiles)
{
    typedef QMap<String, AbstractProfile *> Profiles;
    Profiles profiles;
    String persistentName;

    Instance(Public *i) : Base(i)
    {}

    ~Instance()
    {
        clear();
    }

    void add(AbstractProfile *profile)
    {
        if(profiles.contains(profile->name()))
        {
            delete profiles[profile->name()];
        }
        profiles.insert(profile->name(), profile);
    }

    void clear()
    {
        qDeleteAll(profiles.values());
        profiles.clear();
    }
};

Profiles::Profiles()
    : d(new Instance(this))
{}

StringList Profiles::profiles() const
{
    return d->profiles.keys();
}

int Profiles::count() const
{
    return d->profiles.size();
}

Profiles::AbstractProfile *Profiles::tryFind(String const &name) const
{
    auto found = d->profiles.constFind(name);
    if(found != d->profiles.constEnd())
    {
        return found.value();
    }
    return nullptr;
}

Profiles::AbstractProfile &Profiles::find(String const &name) const
{
    if(auto *p = tryFind(name))
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

LoopResult Profiles::forAll(std::function<LoopResult (AbstractProfile &)> func)
{
    foreach(AbstractProfile *prof, d->profiles.values())
    {
        if(auto result = func(*prof))
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
    profile->setOwner(this);
}

void Profiles::remove(AbstractProfile &profile)
{
    DENG2_ASSERT(&profile.owner() == this);

    d->profiles.remove(profile.name());
    profile.setOwner(nullptr);
}

// Profiles::AbstractProfile --------------------------------------------------

DENG2_PIMPL(Profiles::AbstractProfile)
{
    Profiles *owner = nullptr;
    String name;

    Instance(Public *i) : Base(i) {}

    ~Instance() {}
};

Profiles::AbstractProfile::AbstractProfile()
    : d(new Instance(this))
{}

Profiles::AbstractProfile::~AbstractProfile()
{}

void Profiles::AbstractProfile::setOwner(Profiles *owner)
{
    DENG2_ASSERT(d->owner == nullptr);
    d->owner = owner;
}

Profiles &Profiles::AbstractProfile::owner()
{
    DENG2_ASSERT(d->owner);
    return *d->owner;
}

Profiles const &Profiles::AbstractProfile::owner() const
{
    DENG2_ASSERT(d->owner);
    return *d->owner;
}

String Profiles::AbstractProfile::name() const
{
    return d->name;
}

bool Profiles::AbstractProfile::setName(String const &newName)
{
    if(newName.isEmpty()) return false;

    Profiles *owner = d->owner;
    if(owner)
    {
        if(owner->tryFind(newName))
        {
            // This name is already in use.
            return false;
        }
        owner->remove(*this);
    }

    d->name = newName;

    if(owner)
    {
        owner->add(this);
    }
    return true;
}

} // namespace de
