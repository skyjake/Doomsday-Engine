/** @file urivalue.h  Value that holds an Uri instance.
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

#ifndef LIBDOOMSDAY_URIVALUE_H
#define LIBDOOMSDAY_URIVALUE_H

#include "uri.h"
#include <de/value.h>

/**
 * Subclass of Value that holds an URI.
 *
 * Using this is preferable to plain TextValues because constructing Uri objects
 * can be expensive.
 *
 * @ingroup data
 */
class LIBDOOMSDAY_PUBLIC UriValue : public de::Value
{
public:
    UriValue(const res::Uri &initialValue = "");

    /// Converts the UriValue to plain res::Uri.
    operator const res::Uri &() const;

    res::Uri &uri();
    const res::Uri &uri() const;

    Text typeId() const;
    de::Value *duplicate() const;
    //Number asNumber() const;
    Text asText() const;
    //Record *memberScope() const;
    //dsize size() const;
    bool contains(const de::Value &value) const;
    bool isTrue() const;
    int compare(const de::Value &value) const;
    //void sum(const Value &value);
    //void multiply(const Value &value);
    //void divide(const Value &value);
    //void modulo(const Value &divisor);

    // Implements ISerializable.
    void operator >> (de::Writer &to) const;
    void operator << (de::Reader &from);

protected:
    /// Changes the URI.
    void setValue(const res::Uri &uri);

private:
    res::Uri _uri;
};

#endif // LIBDOOMSDAY_URIVALUE_H
