/** @file refuge.h  Persistent data storage.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_REFUGE_H
#define LIBDENG2_REFUGE_H

#include "../Record"

namespace de {

/**
 * Persistent data storage.
 *
 * A Record that can be saved and restored to the application's persistent data archive.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Refuge
{
public:
    /**
     * Constructs a Refuge and reads any existing contents from the persistent data
     * archive.
     *
     * @param persistentPath  Path of the serialized data file written to the persistent
     *                        data archive.
     */
    Refuge(String const &persistentPath);

    /**
     * Returns the path of the serialized data in the persistent archive.
     */
    String path() const;

    /**
     * The contents are automatically written to the persistent archive.
     */
    virtual ~Refuge();

    /**
     * Deserialized the contents of the Refuge from the persistent archive.
     */
    void read();

    /**
     * Serializes the contents of the Refuge to the persistent archive.
     */
    void write() const;

    Time lastWrittenAt() const;

    Record &names();

    Record const &names() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_REFUGE_H
