/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/TextValue"
#include "de/NumberValue"
#include "de/ArrayValue"
#include "de/String"
#include "de/Writer"
#include "de/Reader"

#include <list>
#include <string>
#include <sstream>
#include <cctype>
#include <cmath>

using namespace de;
using std::list;
using std::ostringstream;

TextValue::TextValue(const String& initialValue)
    : _value(initialValue)
{}

TextValue::operator const String& () const
{
    return _value;
}

Value* TextValue::duplicate() const
{
    return new TextValue(_value);
}

Value::Number TextValue::asNumber() const
{
    std::istringstream str(_value);
    Number number = 0;
    str >> number;
    return number;
}

Value::Text TextValue::asText() const
{
    return _value;
}

dsize TextValue::size() const
{
    return _value.size();
}

bool TextValue::isTrue() const
{
    // If there is at least one nonwhite character, this is considered a truth.
    for(Text::const_iterator i = _value.begin(); i != _value.end(); ++i)
    {
        if(!std::isspace(*i))
            return true;
    }
    return false;
}

dint TextValue::compare(const Value& value) const
{
    const TextValue* other = dynamic_cast<const TextValue*>(&value);
    if(other)
    {
        return _value.compare(other->_value);
    }
    return Value::compare(value);
}

void TextValue::sum(const Value& value)
{
    const TextValue* other = dynamic_cast<const TextValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("TextValue::sum", "Value cannot be summed");
    }
    
    _value += other->_value;
}

void TextValue::multiply(const Value& value)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("Value::multiply", "Value cannot be multiplied");
    }
    
    ddouble factor = other->asNumber();
    
    if(factor <= 0)
    {
        _value.clear();
    }
    else
    {
        ostringstream os;
        while(factor-- > 1)
        {
            os << _value;
        }
        // The remainder.
        dint remain = dint(std::floor((factor + 1) * _value.size() + .5));
        os << _value.substr(0, remain);
        _value = os.str();
    }
}

void TextValue::divide(const Value& value)
{
    const TextValue* other = dynamic_cast<const TextValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("TextValue::divide", "Text cannot be divided");
    }
    _value = _value / other->_value;
}

void TextValue::modulo(const Value& value)
{
    list<const Value*> args;
    
    const ArrayValue* array = dynamic_cast<const ArrayValue*>(&value);
    if(array)
    {
        for(ArrayValue::Elements::const_iterator i = array->elements().begin();
            i != array->elements().end(); ++i)
        {
            args.push_back(*i);
        }
    }
    else
    {
        // Just one.
        args.push_back(&value);
    }
    
    _value = substitutePlaceholders(_value, args);
}

String TextValue::substitutePlaceholders(const String& pattern, const std::list<const Value*>& args)
{
    ostringstream result;
    list<const Value*>::const_iterator arg = args.begin();
    
    for(String::const_iterator i = pattern.begin(); i != pattern.end(); ++i)
    {
        char ch = *i;
        
        if(ch == '%')
        {
            if(arg == args.end())
            {
                throw IllegalPatternError("TextValue::replacePlaceholders",
                    "Too few substitution values");
            }
            
            result << String::patternFormat(i, pattern.end(), **arg);
            ++arg;
        }
        else
        {
            result << ch;
        }
    }
    
    return result.str();
}

void TextValue::operator >> (Writer& to) const
{
    to << SerialId(TEXT) << _value;
}

void TextValue::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != TEXT)
    {
        throw DeserializationError("TextValue::operator <<", "Invalid ID");
    }
    from >> _value;
}

void TextValue::setValue(const String& text)
{
    _value = text;
}
