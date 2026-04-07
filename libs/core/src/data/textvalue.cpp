/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/textvalue.h"
#include "de/numbervalue.h"
#include "de/arrayvalue.h"
#include "de/cstring.h"
#include "de/writer.h"
#include "de/reader.h"
#include "de/scripting/scriptsystem.h"

#include <list>
#include <cmath>

namespace de {

using std::list;

TextValue::TextValue(const String &initialValue)
    : _value(initialValue)
    , _iteration(nullptr)
{}

TextValue::~TextValue()
{
    delete _iteration;
}

TextValue::operator CString() const
{
    return _value;
}

TextValue::operator const String &() const
{
    return _value;
}

Value *TextValue::duplicate() const
{
    return new TextValue(_value);
}

Value::Number TextValue::asNumber() const
{
    return std::strtod(_value, nullptr);
}

Value::Text TextValue::asText() const
{
    return _value;
}

Record *TextValue::memberScope() const
{
    return &ScriptSystem::builtInClass("String");
}

dsize TextValue::size() const
{
    return _value.size();
}

bool TextValue::contains(const Value &value) const
{
    // We are able to look for substrings within the text, without applying automatic
    // type conversions.
    if (is<TextValue>(value))
    {
        return _value.indexOf(value.as<TextValue>()._value) >= 0;
    }
    return Value::contains(value);
}

Value *TextValue::duplicateElement(const Value &charPos) const
{
    return new TextValue(_value.substr(CharPos(charPos.asInt()), 1));
}

Value *TextValue::next()
{
    if (_iteration && *_iteration == _value.end())
    {
        delete _iteration;
        _iteration = nullptr;
        return nullptr; // The end.
    }
    if (!_iteration)
    {
        _iteration = new String::const_iterator(_value.begin());
    }
    return new TextValue(String(1, *(*_iteration)++));
}

bool TextValue::isTrue() const
{
    // If there is at least one nonwhite character, this is considered a truth.
    for (auto ch : _value)
    {
        if (!ch.isSpace()) return true;
    }
    return false;
}

dint TextValue::compare(const Value &value) const
{
    const TextValue *other = dynamic_cast<const TextValue *>(&value);
    if (other)
    {
        return _value.compare(other->_value);
    }
    return Value::compare(value);
}

void TextValue::sum(const Value &value)
{
    const TextValue *other = dynamic_cast<const TextValue *>(&value);
    if (!other)
    {
        throw ArithmeticError("TextValue::sum", "Value cannot be summed");
    }

    _value += other->_value;
}

void TextValue::multiply(const Value &value)
{
    const NumberValue *other = dynamic_cast<const NumberValue *>(&value);
    if (!other)
    {
        throw ArithmeticError("Value::multiply", "Value cannot be multiplied");
    }

    ddouble factor = other->asNumber();

    if (factor <= 0)
    {
        _value.clear();
    }
    else
    {
        String str;
        while (factor-- > 1)
        {
            str += _value;
        }
        // The remainder.
        dint remain = dint(std::floor((factor + 1) * _value.size() + .5));
        str += _value.left(CharPos(remain));
        _value = str;
    }
}

void TextValue::divide(const Value &value)
{
    const TextValue *other = dynamic_cast<const TextValue *>(&value);
    if (!other)
    {
        throw ArithmeticError("TextValue::divide", "Text cannot be divided");
    }
    _value = _value / other->_value;
}

void TextValue::modulo(const Value &value)
{
    list<const Value *> args;

    const ArrayValue *array = dynamic_cast<const ArrayValue *>(&value);
    if (array)
    {
        for (ArrayValue::Elements::const_iterator i = array->elements().begin();
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

String TextValue::substitutePlaceholders(const String &pattern, const std::list<const Value *> &args)
{
    String result;
    auto arg = args.begin();

    for (auto i = pattern.begin(); i != pattern.end(); ++i)
    {
        Char ch = *i;
        if (ch == '%')
        {
            if (arg == args.end())
            {
                throw IllegalPatternError("TextValue::replacePlaceholders",
                                          "Too few substitution values");
            }
            result += String::patternFormat(i, pattern.end(), **arg);
            ++arg;
        }
        else
        {
            result += ch;
        }
    }
    return result;
}

void TextValue::operator >> (Writer &to) const
{
    to << SerialId(TEXT) << _value;
}

void TextValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != TEXT)
    {
        throw DeserializationError("TextValue::operator <<", "Invalid ID");
    }
    from >> _value;
}

void TextValue::setValue(const String &text)
{
    _value = text;
}

Value::Text TextValue::typeId() const
{
    return "Text";
}

} // namespace de
