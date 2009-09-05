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

#include "de/Thinker"
#include "de/Record"
#include "de/Writer"
#include "de/Reader"
#include "de/FunctionValue"
#include "de/Process"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/NumberValue"
#include "de/Map"

using namespace de;

#define HAS_INFO 0x01

Thinker::Constructors Thinker::_constructors;

Thinker::Thinker() : _id(0), _info(0), _map(0)
{}

Thinker::~Thinker()
{
    if(_map)
    {
        _map->remove(*this);
    }
    delete _info;
}

void Thinker::setId(const Id& id)
{
    _id = id;
}

void Thinker::setMap(Map* map)
{
    _map = map;
}

void Thinker::think(const Time::Delta& elapsed)
{
    if(!_info)
    {
        // Must rely on built-in behavior of subclass.
        return;
    }

    const Function* func = _info->function("thinker");
    if(func)
    {
        // Prepare the arguments for the thinker function.
        ArrayValue args;
        args.add(new DictionaryValue); // No named arguments.
        args.add(new NumberValue(_id));
        args.add(new NumberValue(elapsed));
            
        // We'll use a temporary process to execute the function in the 
        // private namespace.
        Process(_info).call(*func, args);
    }
}

void Thinker::define(Constructor constructor)
{
    _constructors.insert(constructor);
}

void Thinker::undefine(Constructor constructor)
{
    _constructors.erase(constructor);
}

Thinker* Thinker::constructFrom(Reader& reader)
{
    FOR_EACH(i, _constructors, Constructors::iterator)
    {
        Reader attempt(reader);
        Thinker* thinker = (*i)(attempt);
        if(thinker)
        {
            // Advance the main reader.
            reader.setOffset(attempt.offset());
            return thinker;
        }
    }    
    /// @throw UnrecognizedError  Could not construct thinker because the type id was unknown.
    throw UnrecognizedError("Thinker::constructFrom", "Unknown thinker type");
}

Thinker* Thinker::fromReader(Reader& reader)
{
    SerialId sid;
    reader >> sid;
    if(sid != THINKER)
    {
        return 0;
    }
    std::auto_ptr<Thinker> th(new Thinker);
    reader >> *th.get();
    return th.release();
}

void Thinker::operator >> (Writer& to) const
{
    to << _id << _bornAt;
    if(_info)
    {
        to << duint8(HAS_INFO) << *_info;
    }
    else
    {
        to << duint8(0);
    }
}

void Thinker::operator << (Reader& from)
{
    from >> _id >> _bornAt;
    duint8 flags;
    from >> flags;
    if(flags & HAS_INFO)
    {
        if(!_info)
        {
            _info = new Record;
        }
        from >> *_info;
    }
}
