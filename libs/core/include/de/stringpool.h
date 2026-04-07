/** @file stringpool.h  Pool of strings (case insensitive).
 *
 * @author Copyright © 2010-2015 Daniel Swanson <danij@dengine.net>
 * @author Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_STRINGPOOL_H
#define LIBCORE_STRINGPOOL_H

#include "de/libcore.h"
#include "de/iserializable.h"
#include "de/string.h"
#include <functional>

namespace de {

/**
 * Container data structure for a set of unique case-insensitive strings.
 * Comparable to a @c std::set with unique IDs assigned to each contained string.
 *
 * The term "intern" is used here to refer to the act of inserting a string to
 * the pool. As a result of interning a string, a new internal copy of the
 * string may be created in the pool.
 *
 * Each string that actually gets added to the pool is assigned a unique
 * identifier. If one tries to intern a string that already exists in the pool
 * (case insensitively speaking), no new internal copy is created and no new
 * identifier is assigned. Instead, the existing id of the previously interned
 * string is returned. The string identifiers are not unique over the lifetime
 * of the container: if a string is removed from the pool, its id is free to be
 * reassigned to the next new string. Zero is not a valid id.
 *
 * Each string can also have an associated, custom user-defined uint32 value
 * and/or void *data pointer.
 *
 * The implementation has, at worst, O(log n) complexity for addition, removal,
 * string lookup, and user value/pointer set/get.
 *
 * @todo Add case-sensitive mode.
 *
 * @ingroup data
 */
class DE_PUBLIC StringPool : public ISerializable
{
public:
    /// String identifier was invalid. @ingroup errors
    DE_ERROR(InvalidIdError);

    /// The pool does not have any available identifiers. @ingroup errors
    DE_ERROR(FullError);

    /// String identifier. Each string is assigned its own Id. Because this is
    /// 32-bit, there can be approximately 4.2 billion unique strings in the pool.
    typedef duint32 Id;

public:
    /**
     * Constructs an empty StringPool.
     */
    StringPool();

    /**
     * Constructs an empty StringPool and interns a number of strings.
     *
     * @param strings  Array of strings to be interned (must contain at least @a count strings).
     * @param count    Number of strings to be interned.
     */
    StringPool(const String *strings, uint count);

    /**
     * Clear the string pool. All strings in the pool will be destroyed.
     */
    void clear();

    /**
     * Is the pool empty?
     * @return  @c true if there are no strings present in the pool.
     */
    bool empty() const;

    bool isEmpty() const { return empty(); }

    /**
     * Determines the number of strings in the pool.
     * @return Number of strings in the pool.
     */
    dsize size() const;

    /**
     * Interns string @a str. If this string is not already in the pool, a new
     * internal copy is created; otherwise, the existing internal copy is returned.
     * New internal copies will be assigned new identifiers.
     *
     * @param str  String to be added (must not be of zero-length).
     *             A copy of this is made if the string is interned.
     *
     * @return  Unique Id associated with the internal copy of @a str.
     */
    Id intern(const String &str);

    /**
     * Interns string @a str. If this string is not already in the pool, a new
     * internal copy is created; otherwise, the existing internal copy is returned.
     * New internal copies will be assigned new identifiers.
     *
     * @param str  String to be added (must not be of zero-length).
     *             A copy of this is made if the string is interned.
     *
     * @return The interned copy of the string owned by the pool.
     */
    String internAndRetrieve(const String &str);

    /**
     * Sets the user-specified custom value associated with the string @a id.
     * The default user value is zero.
     *
     * @param id     Id of a string.
     * @param value  User value.
     */
    void setUserValue(Id id, uint value);

    /**
     * Retrieves the user-specified custom value associated with the string @a id.
     * The default user value is zero.
     *
     * @param id     Id of a string.
     *
     * @return User value.
     */
    uint userValue(Id id) const;

    /**
     * Sets the user-specified custom pointer associated with the string @a id.
     * By default the pointer is NULL.
     *
     * @note  User pointer values are @em not serialized.
     *
     * @param id     Id of a string.
     * @param ptr    User pointer.
     */
    void setUserPointer(Id id, void *ptr);

    /**
     * Retrieves the user-specified custom pointer associated with the string @a id.
     *
     * @param id     Id of a string.
     *
     * @return User pointer.
     */
    void *userPointer(Id id) const;

    /**
     * Is @a str considered to be in the pool?
     *
     * @param str   String to look for.
     *
     * @return  Id of the matching string; else @c 0.
     */
    Id isInterned(const String &str) const;

    /**
     * Retrieve an immutable copy of the interned string associated with the
     * string @a id.
     *
     * @param id    Id of the string to retrieve.
     *
     * @return  Interned string associated with @a internId.
     */
    String string(Id id) const;

    /**
     * Returns a reference to an interned string. Only use this when the
     * pool is being accessed as a const object -- the returned references
     * may become invalid when the pool is changed.
     *
     * @param id  Id of the string to retrieve.
     *
     * @return Reference to interned string. Do not change the contents of
     * the pool while retaining the returned reference.
     */
    const String &stringRef(Id id) const;

    /**
     * Removes a string from the pool.
     *
     * @param str   String to be removed.
     *
     * @return  @c true iff a string was removed.
     */
    bool remove(const String &str);

    /**
     * Removes a string from the pool.
     *
     * @param id    Id of the string to remove.
     *
     * @return  @c true iff a string was removed.
     */
    bool removeById(Id id);

    /**
     * Iterate over all strings in the pool making a callback for each. Iteration
     * ends when all strings have been processed or a callback returns non-zero.
     *
     * @param func  Callback to make for each string.
     */
    LoopResult forAll(const std::function<LoopResult (Id)>& func) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

#ifdef DE_DEBUG
    /**
     * Print contents of the pool (for debugging).
     */
    void print() const;
#endif

private:
    DE_PRIVATE(d)
};

}  // namespace de

#endif  // LIBCORE_STRINGPOOL_H
