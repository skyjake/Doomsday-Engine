/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Map"
#include "de/Object"
#include "de/Writer"
#include "de/Reader"
#include "de/App"
#include "de/Library"

using namespace de;

Map::Map()
{}

Map::~Map()
{
    clear();
}

void Map::load(const String& name)
{
    name_ = name;
}

bool Map::isVoid() const
{
    return name_.empty();
}

Object& Map::newObject()
{
    return addAs<Object>(App::game().SYMBOL(deng_NewObject)());
}

void Map::clear()
{
    name_.clear();
    info_.clear();
    thinkerEnum_.reset();

    // Clear thinkers.
    for(Thinkers::iterator i = thinkers_.begin(); i != thinkers_.end(); ++i)
    {
        delete i->second;
    }
    thinkers_.clear();
    
    // Clear objects.
    for(Objects::iterator i = objects_.begin(); i != objects_.end(); ++i)
    {
        delete i->second;
    }
    objects_.clear();
}
    
Thinker* Map::thinker(const Id& id) const
{
    Thinkers::const_iterator thFound = thinkers_.find(id);
    if(thFound != thinkers_.end())
    {
        return thFound->second;
    }
    return NULL;
}

Object* Map::object(const Id& id) const
{
    Objects::const_iterator obFound = objects_.find(id);
    if(obFound != objects_.end())
    {
        return obFound->second;
    }
    return NULL;
}

Id Map::findUniqueThinkerId()
{
    Id id = thinkerEnum_.get();
    while(thinkerEnum_.overflown() && (thinker(id) || object(id)))
    {
        // This is in use, get the next one.
        id = thinkerEnum_.get();
    }
    return id;
}

Thinker& Map::add(Thinker* thinker)
{
    // Give the thinker a new id.
    thinker->setId(findUniqueThinkerId());
    addThinkerOrObject(thinker);
    return *thinker;
}

void Map::addThinkerOrObject(Thinker* thinker)
{
    thinker->setMap(this);
    Object* object = dynamic_cast<Object*>(thinker);
    if(object)
    {
        // Put it in the separate objects map.
        objects_[thinker->id()] = object;
    }
    else
    {
        thinkers_[thinker->id()] = thinker;
    }
}

Thinker* Map::remove(Thinker& th)
{
    if(thinker(th.id()))
    {
        thinkers_.erase(th.id());
        th.setMap(0);
        return &th;
    }
    else if(object(th.id()))
    {
        objects_.erase(th.id());
        th.setMap(0);
        return &th;
    }
    /// @throw NotFoundError  Thinker @a thinker was not found in the map.
    throw NotFoundError("Map::remove", "Thinker " + th.id().asText() + " not found");
}

void Map::operator >> (Writer& to) const
{
    to << name_ << info_;
    
    // Thinkers.
    to << duint32(objects_.size() + thinkers_.size());
    
    for(Objects::const_iterator i = objects_.begin(); i != objects_.end(); ++i)
    {
        to << *i->second;
    }
    for(Thinkers::const_iterator i = thinkers_.begin(); i != thinkers_.end(); ++i)
    {
        to << *i->second;
    }
}

void Map::operator << (Reader& from)
{
    clear();
    
    from >> name_ >> info_;
    
    // Thinkers.
    duint32 count;
    from >> count;
    while(count--)
    {
        Thinker* thinker = Thinker::constructFrom(from);
        addThinkerOrObject(thinker);
        thinkerEnum_.claim(thinker->id());
    }
}
