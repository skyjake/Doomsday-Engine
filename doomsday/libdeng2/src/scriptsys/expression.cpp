/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Expression"
#include "de/Evaluator"
#include "de/ArrayExpression"
#include "de/BuiltInExpression"
#include "de/ConstantExpression"
#include "de/DictionaryExpression"
#include "de/NameExpression"
#include "de/OperatorExpression"
#include "de/Writer"
#include "de/Reader"

using namespace de;

Expression::~Expression()
{}

void Expression::push(Evaluator& evaluator, Record* names) const
{
    evaluator.push(this, names);
}

Expression* Expression::constructFrom(Reader& reader)
{
    SerialId id;
    reader.mark();
    reader >> id;
    reader.rewind();
    
    std::auto_ptr<Expression> result;
    switch(id)
    {
    case ARRAY:
        result.reset(new ArrayExpression());
        break;
        
    case BUILT_IN:
        result.reset(new BuiltInExpression());
        break;
        
    case CONSTANT:
        result.reset(new ConstantExpression());
        break;
        
    case DICTIONARY:
        result.reset(new DictionaryExpression());
        break;

    case NAME:
        result.reset(new NameExpression());
        break;

    case OPERATOR:
        result.reset(new OperatorExpression());
        break;
                
    default:
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("Expression::constructFrom", "Invalid expression identifier");
    }

    // Deserialize it.
    reader >> *result.get();
    return result.release();    
}

const Expression::Flags& Expression::flags () const
{
    return _flags;
}

void Expression::setFlags(Flags f)
{
    _flags = f;
}

void Expression::operator >> (Writer& to) const
{
    // Save the flags.
    to << duint16(_flags);
}

void Expression::operator << (Reader& from)
{
    // Restore the flags.
    duint16 f;
    from >> f;
    _flags = Flags(f);
}
