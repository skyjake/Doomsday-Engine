/** @file dedregister.h  Register of definitions.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_DEDREGISTER_H
#define LIBDOOMSDAY_DEDREGISTER_H

#include "../libdoomsday.h"
#include <de/Record>

/**
 * General purpose register of DED definitions.
 *
 * The important characteristics of definitions are:
 * - preserving the order in which the definitions were parsed
 * - definitions are looked up by ID, name, and/or other members in addition to the
 *   ordinal number (lookup is text-based)
 *
 * DEDRegister is not specific to any one kind of definition, but instead maintains an
 * array of definitions and a set of lookup dictionaries referencing subrecords in the
 * ordered array.
 *
 * This implementation assumes that definitions are only added, not removed (unless
 * all of them are removed at once).
 */
class LIBDOOMSDAY_PUBLIC DEDRegister
{
public:
    /// The specified index or key value was not found from the register. @ingroup errors
    DENG2_ERROR(NotFoundError);

    /// Attempted to use a key for looking up that hasn't been previously registered.
    /// @ingroup errors
    DENG2_ERROR(UndefinedKeyError);

    enum LookupFlag
    {
        CaseSensitive = 0x1,    ///< Looking up is done case sensitively.
        OnlyFirst     = 0x2,    ///< Only the first defined value is kept in lookup (otherwise last).

        DefaultLookup = 0       ///< Latest in order, case insensitive.
    };
    Q_DECLARE_FLAGS(LookupFlags, LookupFlag)

public:
    DEDRegister(de::Record &names);

    /**
     * Adds a member variable that is needed for looking up definitions. Once added,
     * the key can be used in has(), tryFind() and find().
     *
     * @param keyName  Name of the key variable.
     * @param flags    Indexing behavior for the lookup.
     */
    void addLookupKey(de::String const &variableName, LookupFlags const &flags = DefaultLookup);

    /**
     * Clears the existing definitions. The existing lookup keys are not removed.
     */
    void clear();

    /**
     * Adds a new definition as the last one.
     *
     * @return Reference to the added definition. Same as operator[](size()-1).
     */
    de::Record &append();

    int size() const;
    bool has(de::String const &key, de::String const &value) const;

    de::Record &       operator [] (int index);
    de::Record const & operator [] (int index) const;

    de::Record *       tryFind(de::String const &key, de::String const &value);
    de::Record const * tryFind(de::String const &key, de::String const &value) const;

    de::Record &       find(de::String const &key, de::String const &value);
    de::Record const & find(de::String const &key, de::String const &value) const;

private:
    DENG2_PRIVATE(d)
};

#endif // LIBDOOMSDAY_DEDREGISTER_H
