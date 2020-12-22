/** @file urivalue.cpp  Value that holds an Uri instance.
 *
 * @author Copyright &copy; 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "doomsday/urivalue.h"

#include <de/textvalue.h>
#include <de/reader.h>
#include <de/writer.h>

using namespace de;

UriValue::UriValue(const res::Uri &initialValue)
    : _uri(initialValue)
{}

UriValue::operator const res::Uri &() const
{
    return _uri;
}

res::Uri &UriValue::uri()
{
    return _uri;
}

const res::Uri &UriValue::uri() const
{
    return _uri;
}

Value *UriValue::duplicate() const
{
    return new UriValue(_uri);
}

Value::Text UriValue::asText() const
{
    return _uri.asText();
}

/*dsize UriValue::size() const
{
    return _uri.size();
}*/

bool UriValue::contains(const Value &value) const
{
    // We are able to look for substrings within the text, without applying automatic
    // type conversions.
    if (is<TextValue>(value))
    {
        return _uri.asText().indexOf(value.asText(), CaseSensitive) >= 0;
    }
    return Value::contains(value);
}

bool UriValue::isTrue() const
{
    return !_uri.isEmpty();
}

dint UriValue::compare(const Value &value) const
{
    const UriValue *other = dynamic_cast<const UriValue *>(&value);
    if (other)
    {
        return _uri.asText().compare(other->_uri.asText());
    }
    return Value::compare(value);
}

void UriValue::operator >> (Writer &to) const
{
    to << SerialId(URI) << _uri;
}

void UriValue::operator << (de::Reader &from)
{
    SerialId id;
    from >> id;
    if (id != URI)
    {
        throw DeserializationError("UriValue::operator <<", "Invalid ID");
    }
    from >> _uri;
}

void UriValue::setValue(const res::Uri &uri)
{
    _uri = uri;
}

Value::Text UriValue::typeId() const
{
    return "Uri";
}
