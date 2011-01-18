/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

Map::Map() : _thinkersFrozen(0)
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
    return addAs<Object>(GAME_SYMBOL(deng_NewObject)());
}

void Map::clear()
{
    _name.clear();
    _info.clear();
    _thinkerEnum.reset();

    // Clear thinkers.
    FOR_EACH(i, _thinkers, Thinkers::iterator)
    {
        i->second->setMap(0);
        delete i->second;
    }
    _thinkers.clear();
    
    // Ownership of the thinkers to add was given to us.
    FOR_EACH(i, _thinkersToAdd, PendingThinkers::iterator)
    {
        delete *i;
    }
    _thinkersToDestroy.clear();
}
    
Thinker* Map::thinker(const Id& id) const
{
    Thinkers::const_iterator found = _thinkers.find(id);
    if(found != _thinkers.end())
    {
        if(markedForDestruction(found->second))
        {
            // No longer exists officially.
            return NULL;
        }
        return found->second;
    }
    return NULL;
}

Object* Map::object(const Id& id) const
{
    Thinkers::const_iterator found = _thinkers.find(id);
    if(found != _thinkers.end())
    {
        if(markedForDestruction(found->second))
        {
            // No longer exists officially.
            return NULL;
        }
        return dynamic_cast<Object*>(found->second);
    }
    return NULL;
}

Id Map::findUniqueThinkerId()
{
    Id id = _thinkerEnum.get();
    while(_thinkerEnum.overflown() && thinker(id))
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
    addThinker(thinker);
    return *thinker;
}

void Map::addThinker(Thinker* t)
{
    t->setMap(this);
    
    if(_thinkersFrozen)
    {
        // Add it to the list of pending tasks.
        _thinkersToAdd.push_back(t);
        return;
    }
    
    _thinkers[t->id()] = t;
}

void Map::destroy(Thinker* t)
{
    if(thinker(t->id()))
    {
        if(_thinkersFrozen)
        {
            // Add it to the list of pending tasks.
            _thinkersToDestroy.push_back(t);
            return;
        }
        
        _thinkers.erase(t->id());
        t->setMap(0);
        delete t;
    }
    /// @throw NotFoundError  Thinker @a thinker was not found in the map.
    throw NotFoundError("Map::destroy", "Thinker " + t->id().asText() + " not found");
}

void Map::freezeThinkerList(bool freeze)
{
    if(freeze)
    {
        _thinkersFrozen++;
    }
    else
    {
        _thinkersFrozen--;
        Q_ASSERT(_thinkersFrozen >= 0);
        
        if(!_thinkersFrozen)
        {
            // Perform the pending tasks.
            FOR_EACH(i, _thinkersToAdd, PendingThinkers::iterator)
            {
                add(const_cast<Thinker*>(*i));
            }
            FOR_EACH(i, _thinkersToDestroy, PendingThinkers::iterator)
            {
                destroy(const_cast<Thinker*>(*i));
            }
            _thinkersToAdd.clear();
            _thinkersToDestroy.clear();
        }
    }
}

bool Map::markedForDestruction(const Thinker* thinker) const
{
    FOR_EACH(i, _thinkersToDestroy, PendingThinkers::const_iterator)
    {
        if(*i == thinker)
        {
            return true;
        }
    }
    return false;
}

bool Map::iterate(Thinker::SerialId serialId, bool (*callback)(Thinker*, void*), void* parameters)
{
    freezeThinkerList(true);

    FOR_EACH(i, _thinkers, Thinkers::iterator)
    {
        if(i->second->serialId() == serialId && !markedForDestruction(i->second))
        {
            if(!callback(i->second, parameters))
            {
                freezeThinkerList(false);
                return false;
            }
        }
    }

    freezeThinkerList(false);
    return true;
}

bool Map::iterateObjects(bool (*callback)(Object*, void*), void* parameters)
{
    freezeThinkerList(true);

    FOR_EACH(i, _thinkers, Thinkers::iterator)
    {
        Object* object = dynamic_cast<Object*>(i->second);
        if(object)
        {
            if(!callback(object, parameters) && !markedForDestruction(i->second))
            {
                freezeThinkerList(false);
                return false;
            }
        }
    }

    freezeThinkerList(false);
    return true;    
}

void Map::think(const Time::Delta& elapsed)
{
    freezeThinkerList(true);

    FOR_EACH(i, _thinkers, Thinkers::iterator)
    {
        if(i->second->isAlive() && !markedForDestruction(i->second))
        {
            i->second->think(elapsed);
        }
    }    

    freezeThinkerList(false);
}

void Map::operator >> (Writer& to) const
{
    LOG_AS("Map::operator >>");
    
    to << _name << _info;
    
    LOG_DEBUG("Serializing %i thinkers.") << _thinkers.size();
    
    // Thinkers.
    to << duint32(_thinkers.size());
    FOR_EACH(i, _thinkers, Thinkers::const_iterator)
    {
        to << *i->second;
    }
}

void Map::operator << (Reader& from)
{
    LOG_AS("Map::operator <<");
    
    clear();
    
    from >> _name >> _info;
    
    // Thinkers.
    duint32 count;
    from >> count;
    LOG_DEBUG("Deserializing %i thinkers.") << count;
    while(count--)
    {
        Thinker* thinker = Thinker::constructFrom(from);
        addThinker(thinker);
        _thinkerEnum.claim(thinker->id());
    }
}
