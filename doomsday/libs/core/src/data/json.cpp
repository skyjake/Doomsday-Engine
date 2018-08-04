/** @file json.cpp  JSON parser and composer.
 *
 * Parses JSON and outputs a QVariant with the data.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/data/json.h"
#include <de/ArrayValue>
#include <de/Block>
#include <de/DictionaryValue>
#include <de/Error>
#include <de/Log>
#include <de/NoneValue>
#include <de/NumberValue>
#include <de/RecordValue>
#include <de/TextValue>

namespace de {

namespace internal {

/**
 * @internal Not exposed outside this source file; use parseJSON() instead.
 */
class JSONParser
{
    const String &source;
    String::const_iterator pos;
    String::const_iterator _previous;

public:
    JSONParser(const String &s) : source(s), pos(s.begin())
    {
        skipWhite();
    }

    void advance()
    {
        _previous = pos++;
        skipWhite();
    }

    void skipWhite()
    {
        while (!atEnd() && iswspace(*pos)) _previous = pos++;
    }

    bool atEnd() const
    {
        return pos >= source.end();
    }

    Char peek() const
    {
        if (atEnd()) return 0;
        return *pos;
    }

    Char next()
    {
        if (atEnd()) return 0;
        Char c = *pos;
        advance();
        return c;
    }

    Char nextNoSkip()
    {
        if (atEnd()) return 0;
        return *(_previous = pos++);
    }

    DE_NORETURN void error(const String &message)
    {
        BytePos offset = pos.pos();
        throw Error("JSONParser",
                    stringf("Error at position %zu (%s^%s): %s",
                            offset.index,
                            source.substr(offset - 4, 4).c_str(),
                            source.substr(offset, 4).c_str(),
                            message.c_str()));
    }

    Value *parse()
    {
        LOG_AS("JSONParser");
        if (atEnd())
        {
            return nullptr;
        }
        Char c = peek();
        if (c == '{')
        {
            const TextValue objKey{"__obj__"};
            std::unique_ptr<DictionaryValue> dict(parseObject());
            if (dict->contains(objKey))
            {
                const auto &objClass = dict->element(objKey);
                if (objClass.asText() == "Record")
                {
                    // Convert to a record.
                    return RecordValue::takeRecord(dict->toRecord());
                }
            }
            return dict.release();
        }
        else if (c == '[')
        {
            return parseArray();
        }
        else if (c == '\"')
        {
            return parseString();
        }
        else if (c == '-' || iswdigit(c))
        {
            return parseNumber();
        }
        else
        {
            return parseKeyword();
        }
    }

    DictionaryValue *parseObject()
    {
        std::unique_ptr<DictionaryValue> result(new DictionaryValue);
        Char c = next();
        DE_ASSERT(c == '{');
        for (;;)
        {
            if (peek() == '}')
            {
                // Totally empty.
                break;
            }
            std::unique_ptr<TextValue> name(parseString());
            c = next();
            if (c != ':') error("object keys and values must be separated by a colon");
            // Add to the result.
            result->add(name.release(), parse());
            // Move forward.
            skipWhite();
            c = next();
            if (c == '}')
            {
                // End of object.
                break;
            }
            else if (c != ',')
            {
                LOG_DEBUG(Stringf("got '%lc' instead of ','", c));
                error("key/value pairs must be separated by comma");
            }
        }
        return result.release();
    }

    ArrayValue *parseArray()
    {
        std::unique_ptr<ArrayValue> result(new ArrayValue);
        Char c = next();
        DE_ASSERT(c == '[');
        if (peek() == ']')
        {
            // Empty list.
            next();
            return result.release();
        }
        for (;;)
        {
            *result << parse();
            c = next();
            if (c == ']')
            {
                // End of array.
                break;
            }
            else if (c != ',')
            {
                // What?
                error("array items must be separated by comma");
            }
        }
        return result.release();
    }

    TextValue *parseString()
    {
        String result;
        Char c = next();
        DE_ASSERT(c == '\"');
        for (;;)
        {
            c = nextNoSkip();
            if (c == '\\')
            {
                // Escape.
                c = nextNoSkip();
                if (c == '\"' || c == '\\' || c == '/')
                    result.append(c);
                else if (c == 'b')
                    result.append('\b');
                else if (c == 'f')
                    result.append('\f');
                else if (c == 'n')
                    result.append('\n');
                else if (c == 'r')
                    result.append('\r');
                else if (c == 't')
                    result.append('\t');
                else if (c == 'u')
                {
                    const String code = source.substr(pos.pos(), 4);
                    pos += 4;
                    result.append(Char(code.toUInt32(nullptr, 16)));
                }
                else error("unknown escape sequence in string");
            }
            else if (c == '\"')
            {
                // End of string.
                break;
            }
            else
            {
                result.append(c);
            }
        }
        skipWhite();
        return new TextValue(result);
    }

    NumberValue *parseNumber()
    {
        String str;
        Char c = next();
        if (c == '-')
        {
            str.append(c);
            c = nextNoSkip();
        }
        for (; iswdigit(c); c = nextNoSkip())
        {
            str.append(c);
        }
        bool hasDecimal = false;
        if (c == '.')
        {
            str.append(c);
            hasDecimal = true;
            c = nextNoSkip();
            for (; iswdigit(c); c = nextNoSkip())
            {
                str.append(c);
            }
        }
        if (c == 'e' || c == 'E')
        {
            // Exponent.
            str.append(c);
            c = nextNoSkip();
            if (c == '+' || c == '-')
            {
                str.append(c);
                c = nextNoSkip();
            }
            for (; iswdigit(c); c = nextNoSkip())
            {
                str.append(c);
            }
        }
        // Rewind one char (the loop was broken when a non-digit was read).
        pos = _previous;
        skipWhite();
        double value = strtod(str, nullptr);
        if (hasDecimal)
        {
            return new NumberValue(value);
        }
        else
        {
            return new NumberValue(int(value));
        }
    }

    Value *parseKeyword()
    {
        if (iCmpStrN(pos, "true", 4) == 0)
        {
            pos += 4;
            skipWhite();
            return new NumberValue(true);
        }
        else if (iCmpStrN(pos, "false", 5) == 0)
        {
            pos += 5;
            skipWhite();
            return new NumberValue(false);
        }
        else if (iCmpStrN(pos, "null", 4) == 0)
        {
            pos += 4;
            skipWhite();
            return new NoneValue;
        }
        else
        {
            error("unknown keyword");
        }
    }
};

//---------------------------------------------------------------------------------------

static String recordToJSON(Record const &rec);
static String valueToJSON(Value const &value);

static String valueToJSONWithTabNewlines(Value const &value)
{
    String json = valueToJSON(value);
    json.replace("\n", "\n\t");
    return json;
}

static String valueToJSON(Value const &value)
{
    if (is<NoneValue>(value))
    {
        return "null";
    }
    if (auto const *rec = maybeAs<RecordValue>(value))
    {
        return recordToJSON(rec->dereference());
    }
    if (auto const *dict = maybeAs<DictionaryValue>(value))
    {
        String out = "{";
        auto const &elems = dict->elements();
        for (auto i = elems.begin(); i != elems.end(); ++i)
        {
            if (i != elems.begin())
            {
                out += ",";
            }
            out += "\n\t" + valueToJSON(*i->first.value) + ": " +
                   valueToJSONWithTabNewlines(*i->second);
        }
        return out + "\n}";
    }
    if (auto const *array = maybeAs<ArrayValue>(value))
    {
        String out = "[";
        auto const &elems = array->elements();
        for (auto i = elems.begin(); i != elems.end(); ++i)
        {
            if (i != elems.begin())
            {
                out += ",";
            }
            out += "\n\t" + valueToJSONWithTabNewlines(**i);
        }
        return out + "\n]";
    }
    if (auto const *num = maybeAs<NumberValue>(value))
    {
        if (num->semanticHints() & NumberValue::Boolean)
        {
            return num->isTrue()? "true" : "false";
        }
        return num->asText();
    }

    // Text string.
    return "\"" + value.asText().escaped() + "\"";
}

static String recordToJSON(Record const &rec)
{
    String out = "{\n\t\"__obj__\": \"Record\"";
    rec.forMembers([&out] (const String &name, const Variable &var)
    {
        out += ",\n\t\"" + name + "\": " + valueToJSONWithTabNewlines(var.value());
        return LoopContinue;
    });
    return out + "\n}";
}

} // internal

Value *parseJSONValue(const String &jsonText)
{
    return internal::JSONParser(jsonText).parse();
}

Record parseJSON(const String &jsonText)
{
    try
    {
        std::unique_ptr<Value> parsed(parseJSONValue(jsonText));
        // The returned object needs to be a record.
        if (is<DictionaryValue>(parsed.get()))
        {
            return parsed->as<DictionaryValue>().toRecord();
        }
        if (is<RecordValue>(parsed.get()))
        {
            return parsed->as<RecordValue>().dereference();
        }
    }
    catch (Error const &er)
    {
        LOG_WARNING(er.asText().c_str());
    }
    return {}; // invalid
}

String composeJSON(Record const &rec)
{
    return internal::recordToJSON(rec) + "\n";
}

} // de
