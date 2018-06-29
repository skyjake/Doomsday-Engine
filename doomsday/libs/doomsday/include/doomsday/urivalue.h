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
#include <de/Value>

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
    UriValue(de::Uri const &initialValue = "");

    /// Converts the UriValue to plain de::Uri.
    operator de::Uri const &() const;

    de::Uri &uri();
    de::Uri const &uri() const;

    Text typeId() const;
    de::Value *duplicate() const;
    //Number asNumber() const;
    Text asText() const;
    //Record *memberScope() const;
    //dsize size() const;
    bool contains(de::Value const &value) const;
    bool isTrue() const;
    de::dint compare(de::Value const &value) const;
    //void sum(Value const &value);
    //void multiply(Value const &value);
    //void divide(Value const &value);
    //void modulo(Value const &divisor);

    // Implements ISerializable.
    void operator >> (de::Writer &to) const;
    void operator << (de::Reader &from);

protected:
    /// Changes the URI.
    void setValue(de::Uri const &uri);

private:
    de::Uri _uri;
};

#endif // LIBDOOMSDAY_URIVALUE_H
