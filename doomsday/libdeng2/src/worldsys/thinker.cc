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

using namespace de;

#define HAS_INFO 0x01

Thinker::Thinker() : id_(0), info_(0)
{}

Thinker::~Thinker()
{
    delete info_;
}

void Thinker::think(const Time::Delta& elapsed)
{
    if(!info_)
    {
        // Must rely on built-in behavior of subclass.
        return;
    }
    
    Record::Members::const_iterator found = info_->members().find("thinker");
    if(found != info_->members().end())
    {
        // Does it have a function as value?
        const FunctionValue* func = dynamic_cast<const FunctionValue*>(&found->second->value());
        if(func)
        {
            // Prepare the arguments for the thinker function.
            ArrayValue args;
            args.add(new DictionaryValue); // No named arguments.
            args.add(new NumberValue(id_));
            args.add(new NumberValue(elapsed));
            
            // We'll use a temporary process to execute the function in the 
            // private namespace.
            Process(info_).call(func->function(), args);
        }
    }
}

void Thinker::operator >> (Writer& to) const
{
    to << id_ << bornAt_;
    if(info_)
    {
        to << duint8(HAS_INFO) << *info_;
    }
    else
    {
        to << duint8(0);
    }
}

void Thinker::operator << (Reader& from)
{
    from >> id_ >> bornAt_;
    duint8 flags;
    from >> flags;
    if(flags & HAS_INFO)
    {
        if(!info_)
        {
            info_ = new Record;
        }
        from >> *info_;
    }
}
