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
    _name = name;
}

bool Map::isVoid() const
{
    return _name.empty();
}

Object& Map::newObject()
{
    return addAs<Object>(App::game().SYMBOL(deng_NewObject)());
}

void Map::clear()
{
    _name.clear();
    _info.clear();
    _thinkerEnum.reset();

    // Clear thinkers.
    for(Thinkers::iterator i = _thinkers.begin(); i != _thinkers.end(); ++i)
    {
        delete i->second;
    }
    _thinkers.clear();
    
    // Clear objects.
    for(Objects::iterator i = _objects.begin(); i != _objects.end(); ++i)
    {
        delete i->second;
    }
    _objects.clear();
}
    
Thinker* Map::thinker(const Id& id) const
{
    Thinkers::const_iterator thFound = _thinkers.find(id);
    if(thFound != _thinkers.end())
    {
        return thFound->second;
    }
    return NULL;
}

Object* Map::object(const Id& id) const
{
    Objects::const_iterator obFound = _objects.find(id);
    if(obFound != _objects.end())
    {
        return obFound->second;
    }
    return NULL;
}

Id Map::findUniqueThinkerId()
{
    Id id = _thinkerEnum.get();
    while(_thinkerEnum.overflown() && (thinker(id) || object(id)))
    {
        // This is in use, get the next one.
        id = _thinkerEnum.get();
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
        _objects[thinker->id()] = object;
    }
    else
    {
        _thinkers[thinker->id()] = thinker;
    }
}

Thinker* Map::remove(Thinker& th)
{
    if(thinker(th.id()))
    {
        _thinkers.erase(th.id());
        th.setMap(0);
        return &th;
    }
    else if(object(th.id()))
    {
        _objects.erase(th.id());
        th.setMap(0);
        return &th;
    }
    /// @throw NotFoundError  Thinker @a thinker was not found in the map.
    throw NotFoundError("Map::remove", "Thinker " + th.id().asText() + " not found");
}

void Map::operator >> (Writer& to) const
{
    to << _name << _info;
    
    // Thinkers.
    to << duint32(_objects.size() + _thinkers.size());
    
    for(Objects::const_iterator i = _objects.begin(); i != _objects.end(); ++i)
    {
        to << *i->second;
    }
    for(Thinkers::const_iterator i = _thinkers.begin(); i != _thinkers.end(); ++i)
    {
        to << *i->second;
    }
}

void Map::operator << (Reader& from)
{
    clear();
    
    from >> _name >> _info;
    
    // Thinkers.
    duint32 count;
    from >> count;
    while(count--)
    {
        Thinker* thinker = Thinker::constructFrom(from);
        addThinkerOrObject(thinker);
        _thinkerEnum.claim(thinker->id());
    }
}
