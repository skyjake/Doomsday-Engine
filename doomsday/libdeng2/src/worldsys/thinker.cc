/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <sstream>

using namespace de;

#define HAS_INFO 0x01

Thinker::Constructors Thinker::_constructors;

Thinker::Thinker(SerialId sid) : _serialId(sid), _id(0), _info(0), _map(0)
{}

Thinker::~Thinker()
{
    assert(_map == NULL); // By this time, it must be removed from the map.
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

void Thinker::define(SerialId serializedId, Constructor constructor)
{
    _constructors[serializedId] = constructor;
}

void Thinker::undefine(SerialId serializedId)
{
    _constructors.erase(serializedId);
}

Thinker* Thinker::constructFrom(Reader& reader)
{
    SerialId serialId;
    reader >> serialId;
    reader.rewind(sizeof(SerialId));

    Constructors::iterator found = _constructors.find(serialId);
    if(found != _constructors.end())
    {
        std::auto_ptr<Thinker> th(found->second());
        reader >> *th.get();
        return th.release();
    }
    
    /// @throw UnrecognizedError  Could not construct thinker because the type id was unknown.
    throw UnrecognizedError("Thinker::constructFrom", "Unknown thinker type");
}

void Thinker::operator >> (Writer& to) const
{
    to << _serialId << _id << _bornAt;
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
    SerialId readSerialId;
    from >> readSerialId; 

    // Sanity check.
    if(readSerialId != _serialId)
    {
        std::ostringstream os;
        os << "Invalid serial ID (got " << duint(readSerialId) << " while " << 
            duint(_serialId) << " was expected)";
        /// @throw InvalidTypeError  The read serial ID was unexpected.
        throw InvalidTypeError("Thinker::operator <<", os.str());
    }
    
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
